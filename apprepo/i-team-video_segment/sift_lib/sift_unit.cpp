/*
 *  sift_unit.cpp
 *  sift-lib
 *
 *  Created by Matthias Grundmann on 6/7/2010.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "sift_unit.h"

#include <GL/glew.h>
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "assert_log.h"
#include "image_util.h"
#include "SiftGPU.h"

using namespace std;

namespace VideoFramework {

  DEFINE_DATA_FRAME(SiftFrame);

  float SiftFrame::SiftFeature::L2Diff(const SiftFrame::SiftFeature& feat) const {
    float result = 0;
    const float* desc_1 = &descriptor[0];
    const float* desc_2 = &feat.descriptor[0];

    for (int i = 0; i < 128; ++i, ++desc_1, ++desc_2) {
      const float diff = *desc_1 - *desc_2;
      result += diff * diff;
    }
//    LOG(INFO) << descriptor[10] <<  " " << feat.descriptor[10];
//    LOG(INFO) << "REsult: " << result;
    return result;
  }

  SiftUnit::SiftUnit(bool visualize_feature_points,
                     const string& vid_stream_name,
                     const string& sift_stream_name) :
  visualize_feature_points_(visualize_feature_points),
  vid_stream_name_(vid_stream_name),
  sift_stream_name_(sift_stream_name) {

  }

  SiftUnit::~SiftUnit() {

  }

  bool SiftUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);

    if (vid_stream_idx_ < 0) {
      std::cerr << "SiftUnit::OpenStreams: "
                << "Could not find Video stream!\n";
      return false;
    }

    // Get video stream info.
    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                        set->at(vid_stream_idx_).get());
    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    if (vid_stream->pixel_format() != PIXEL_FORMAT_LUMINANCE) {
      std::cerr << "SiftUnit::OpenStreams: "
                << "Expecting luminance stream!\n";
      return false;
    }

    // Add SiftStream.
    DataStream* sift_stream = new DataStream(sift_stream_name_);
    set->push_back(shared_ptr<DataStream>(sift_stream));

    // Create GPU sift object.
    sift_gpu_.reset(new SiftGPU);

    if (sift_gpu_->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED) {
      std::cerr << "SiftUnit::OpenStrems: "
                << "Could not create ContextGL.\n";
      return false;
    }

    frame_num_ = 0;
    return true;
  }

  void SiftUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(vid_stream_idx_).get());
    ASSERT_LOG(frame);

    // Run sift.
    sift_gpu_->RunSIFT(frame_width_, frame_height_, frame->data(),
                       GL_LUMINANCE, GL_UNSIGNED_BYTE);

    // Obtain results.
    int num_features = sift_gpu_->GetFeatureNum();
    vector<SiftGPU::SiftKeypoint> keypoints(num_features);
    vector<float> descriptors(num_features * 128);
    sift_gpu_->GetFeatureVector(&keypoints[0], &descriptors[0]);

    // Convert to SiftFrame.
    vector<SiftFrame::SiftFeature> sift_features(num_features);
    for (int i = 0; i < num_features; ++i) {
      sift_features[i].x = keypoints[i].x;
      sift_features[i].y = keypoints[i].y;
      sift_features[i].scale = keypoints[i].s;
      sift_features[i].orientation = keypoints[i].o;
      memcpy(sift_features[i].descriptor, &descriptors[i * 128], 128 * sizeof(float));
    }

    if (visualize_feature_points_) {
      IplImage image;
      frame->ImageView(&image);
      for (int i = 0; i < num_features; ++i) {
        // Draw ellipse around feature location.
        cvDrawEllipse(&image, cvPoint(sift_features[i].x, sift_features[i].y),
                      cvSize(sift_features[i].scale, sift_features[i].scale),
                      0, 0, 360, cvScalar(255, 255, 255));
      }

      cvSaveImage((string("sift_viz") + lexical_cast<string>(frame_num_) + ".png").c_str(),
                  &image);
    }


    SiftFrame* sift_frame = new SiftFrame;
    sift_frame->SwapFeatures(&sift_features);
    input->push_back(shared_ptr<SiftFrame>(sift_frame));
    output->push_back(input);
    ++frame_num_;
  }

  void SiftUnit::PostProcess(list<FrameSetPtr>* append) {

  }

  SiftWriter::SiftWriter(bool reset_frames,
                         const std::string& file_name,
                         const std::string& sift_stream_name)
      : reset_frames_(reset_frames),
        file_name_(file_name),
        sift_stream_name_(sift_stream_name) {
  }

  bool SiftWriter::OpenStreams(VideoFramework::StreamSet* set) {
    // Find Sift-Stream.
    sift_stream_idx_ = FindStreamIdx(sift_stream_name_, set);
    if (sift_stream_idx_ < 0) {
      LOG(ERROR) << "Couldn't find Sift stream";
      return false;
    }

    // Open file.
    LOG(INFO) << "Writing sift features to file " << file_name_;
    ofs_.open(file_name_.c_str(), std::ios_base::out | std::ios_base::binary);
    return true;
  }

  void SiftWriter::ProcessFrame(VideoFramework::FrameSetPtr input,
                                std::list<VideoFramework::FrameSetPtr>* output) {
    // Get Sift frame.
    const SiftFrame* sift_frame = dynamic_cast<const SiftFrame*>(input->at(sift_stream_idx_).get());
    ASSURE_LOG(sift_frame) << "Not a SiftFrame at index " << sift_stream_idx_;

    // Write number of features.
    const int num_features = sift_frame->NumFeatures();
    ofs_.write((char*)&num_features, sizeof(num_features));

    // Write all features in one chunk.
    const vector<SiftFrame::SiftFeature>& features = sift_frame->Features();
    LOG(INFO) << features[10].x << " " << features[10].y;
    ofs_.write((char*)&(features[0]), num_features * sizeof(SiftFrame::SiftFeature));

    if (reset_frames_) {
      input->at(sift_stream_idx_).reset();
    }

    output->push_back(input);
  }

  void SiftWriter::PostProcess(std::list<VideoFramework::FrameSetPtr>* append) {
    ofs_.close();
  }


  SiftReader::SiftReader(const std::string& file_name,
                         const std::string& sift_stream_name)
      : file_name_(file_name),
        sift_stream_name_(sift_stream_name) {
  }

  bool SiftReader::OpenStreams(VideoFramework::StreamSet* set) {
    // Add SiftStream.
    DataStream* sift_stream = new DataStream(sift_stream_name_);
    set->push_back(shared_ptr<DataStream>(sift_stream));

    // Open file.
    LOG(INFO) << "Reading sift features from file " << file_name_;
    ifs_.open(file_name_.c_str(), std::ios_base::in | std::ios_base::binary);
    return true;
  }

  void SiftReader::ProcessFrame(VideoFramework::FrameSetPtr input,
                                std::list<VideoFramework::FrameSetPtr>* output) {
     // Reader number of features.
    int num_features;
    ifs_.read((char*)&num_features, sizeof(num_features));
    vector<SiftFrame::SiftFeature> sift_features(num_features);

    // Read all features in one chunk.
    ifs_.read((char*)&sift_features[0], num_features * sizeof(SiftFrame::SiftFeature));

    SiftFrame* sift_frame = new SiftFrame();
    sift_frame->SwapFeatures(&sift_features);

    input->push_back(shared_ptr<SiftFrame>(sift_frame));
    output->push_back(input);
  }

  void SiftReader::PostProcess(std::list<VideoFramework::FrameSetPtr>* append) {
    ifs_.close();
  }

}  // namespace VideoFramework.
