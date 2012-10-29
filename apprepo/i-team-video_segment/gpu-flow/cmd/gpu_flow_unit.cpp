/*
 * Copyright (c) ICG. All rights reserved.
 *
 * Institute for Computer Graphics and Vision
 * Graz University of Technology / Austria
 *
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the above copyright notices for more information.
 *
 *
 * Project     : vmgpu
 * Module      : FlowLib Command line tool
 * Class       : $RCSfile$
 * Language    : C++/CUDA
 * Description : Implementation of FlowLib Command Line Tool
 *
 * Author     : Manuel Werlberger
 * EMail      : werlberger@icg.tugraz.at
 *
 */

#include "gpu_flow_unit.h"

// system includes
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

#include <CommonLib.h>
#include <FlowLib.h>

#include <fstream>
#include <boost/scoped_ptr.hpp>
#include <opencv2/imgproc/imgproc_c.h>

#include "assert_log.h"
#include "flow_reader.h"
#include "image_filter.h"

namespace VideoFramework {

GPUFlowUnit::GPUFlowUnit(const string& out_file,
                         FlowType flow_type,
                         int flow_iterations,
                         const string& input_stream_name,
                         const string& backward_flow_stream_name,
                         const string& forward_flow_stream_name,
                         const string& video_out_stream_name)
  : out_file_(out_file),
    flow_type_(flow_type),
    flow_iterations_(flow_iterations),
    input_stream_name_(input_stream_name),
    backward_flow_stream_name_(backward_flow_stream_name),
    forward_flow_stream_name_(forward_flow_stream_name),
    video_out_stream_name_(video_out_stream_name) {
  flow_lib_.reset(new FlowLib());
}

GPUFlowUnit::~GPUFlowUnit() {

}

bool GPUFlowUnit::OpenStreams(StreamSet* set) {
  video_stream_idx_ = FindStreamIdx(input_stream_name_, set);

  if (video_stream_idx_ < 0) {
    std::cerr << "GPUFlowUnit::OpenStreams: "
              << "Could not find video stream!\n";
    return false;
  }

  // Prepare flow lib.
  float lambda = 40;
  float gauss_tv_epsilon = 0.01f;
  unsigned int num_iter = flow_iterations_;
  unsigned int num_warps = 3;
  float scale_factor = 0.7f;
  bool median_filt = true;
  bool diffusion_tensor = true;

  flow_lib_->setLambda(lambda);
  flow_lib_->setGaussTVEpsilon(gauss_tv_epsilon);
  flow_lib_->setNumIter(num_iter);
  flow_lib_->setNumWarps(num_warps);
  flow_lib_->setScaleFactor(scale_factor);
   //flowlib.useTextureRescale(true);
  flow_lib_->useMedianFilter(median_filt);
  flow_lib_->useDiffusionTensor(diffusion_tensor);
  flow_lib_->setStructureTextureDecomposition("ROF", 0.8f, 10.0f);
  //flow_lib_->setNumImages(4);

  frame_number_ = 0;

  // Get video stream.
  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                      set->at(video_stream_idx_).get());
  ASSERT_LOG(vid_stream);

  int frame_width = vid_stream->frame_width();
  int frame_height = vid_stream->frame_height();
  int pixel_format = vid_stream->pixel_format();

  if (vid_stream->pixel_format() != PIXEL_FORMAT_LUMINANCE) {
    std::cerr << "GPUFlowUnit: Expecting luminance input.\n";
    return false;
  }

  width_step_ = frame_width * 3;
  if (width_step_ % 4) {
    width_step_ = width_step_ + (4 - width_step_ % 4);
    ASSERT_LOG(width_step_ % 4 == 0);
  }

  if (!forward_flow_stream_name_.empty()) {
    DataStream* flow_stream = new DataStream(forward_flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(flow_stream));
  }

  if (!backward_flow_stream_name_.empty()) {
    DataStream* flow_stream = new DataStream(backward_flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(flow_stream));
  }

  if (!video_out_stream_name_.empty()) {
    VideoStream* video_out_stream =
        new VideoStream(frame_width, frame_height,
                        width_step_, vid_stream->fps(),
                        vid_stream->frame_count(),
                        PIXEL_FORMAT_BGR24, video_out_stream_name_);
    set->push_back(shared_ptr<VideoStream>(video_out_stream));
  }

  // Open file.
  if (!out_file_.empty()) {
    ofs_.open(out_file_.c_str(), std::ios_base::out | std::ios_base::binary);

    // Write header.
    int flow_type = flow_type_;
    ofs_.write((char*)&frame_width, sizeof(frame_width));
    ofs_.write((char*)&frame_height, sizeof(frame_height));
    ofs_.write((char*)&flow_type, sizeof(flow_type));
  }

  return true;
}
void GPUFlowUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  // Get raw video frame data.
  const VideoFrame* frame =
      dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  const uchar* vid_data = frame->data();

  int width = frame->width();
  int height = frame->height();

  // Allocate cuda memory and convert data.
  float* data = new float[width * height];
  Cuda::Size<2> size = Cuda::Size<2>(width, height);
  Cuda::HostMemoryReference<float,2>* hostRef =
      new Cuda::HostMemoryReference<float, 2>(size, data);

  for (int i = 0; i < height; ++i) {
    const uchar* src_ptr = vid_data + i * frame->width_step();
    float* dst_ptr = data + i * width;
    for (int j = 0; j < width; ++j, ++src_ptr, ++dst_ptr)
      *dst_ptr = ((float)*src_ptr) / 255.0f ;
  }

  Cuda::Size<2> padded_size = flow_lib_->getOptimalMemSize(hostRef->size);
  shared_ptr<Cuda::DeviceMemoryPitched<float,2> > image(
      new Cuda::DeviceMemoryPitched<float,2>(padded_size));
  image->setRegion(Cuda::Size<2>(0,0), hostRef->size);
  Cuda::copy(*image, *hostRef, Cuda::Size<2>(0,0), Cuda::Size<2>(0,0), hostRef->size);

  if (frame_number_ > 0) {
    float* render_flow_x = 0;
    float* render_flow_y = 0;

    shared_ptr<DenseFlowFrame> forward_flow_frame;
    shared_ptr<DenseFlowFrame> backward_flow_frame;

    if (flow_type_ == FLOW_FORWARD || flow_type_ == FLOW_BOTH) {
      flow_lib_->setInput(prev_img_.get(), image.get());
      flow_lib_->runAlgorithm(0);

      // Copy from device.
      forward_flow_frame.reset(new DenseFlowFrame(width, height, false));

      float* flow_data_x = (float*)forward_flow_frame->mutable_data();
      Cuda::HostMemoryReference<float,2>* flow_ref_x =
          new Cuda::HostMemoryReference<float, 2>(size, flow_data_x);
      Cuda::copy(*flow_ref_x, *flow_lib_->getFlowU(),
                 Cuda::Size<2>(0,0),
                 Cuda::Size<2>(0,0),
                 flow_ref_x->size);

      float* flow_data_y =
          ((float*)forward_flow_frame->mutable_data()) + width * height;
      Cuda::HostMemoryReference<float,2>* flow_ref_y =
          new Cuda::HostMemoryReference<float, 2>(size, flow_data_y);
      Cuda::copy(*flow_ref_y,
                 *flow_lib_->getFlowV(),
                 Cuda::Size<2>(0,0),
                 Cuda::Size<2>(0,0),
                 flow_ref_y->size);

      if (!forward_flow_stream_name_.empty()) {
        input->push_back(forward_flow_frame);
      }

      if (!out_file_.empty()) {
        // Write to file.
        ofs_.write((char*)flow_data_x, sizeof(float) * width * height);
        ofs_.write((char*)flow_data_y, sizeof(float) * width * height);
      }

      render_flow_x = flow_data_x;
      render_flow_y = flow_data_y;
    }

    if (flow_type_ == FLOW_BACKWARD || flow_type_ == FLOW_BOTH) {
      flow_lib_->setInput(image.get(), prev_img_.get());
      flow_lib_->runAlgorithm(0);

      // Copy from device.
      backward_flow_frame.reset(new DenseFlowFrame(width, height, true));

      float* flow_data_x = (float*)backward_flow_frame->mutable_data();
      Cuda::HostMemoryReference<float,2>* flow_ref_x =
          new Cuda::HostMemoryReference<float, 2>(size, flow_data_x);
      Cuda::copy(*flow_ref_x, *flow_lib_->getFlowU(),
                 Cuda::Size<2>(0,0),
                 Cuda::Size<2>(0,0),
                 flow_ref_x->size);

      float* flow_data_y =
          ((float*)backward_flow_frame->mutable_data()) + width * height;
      Cuda::HostMemoryReference<float,2>* flow_ref_y =
          new Cuda::HostMemoryReference<float, 2>(size, flow_data_y);
      Cuda::copy(*flow_ref_y,
                 *flow_lib_->getFlowV(),
                 Cuda::Size<2>(0,0),
                 Cuda::Size<2>(0,0),
                 flow_ref_y->size);

      if (!backward_flow_stream_name_.empty()) {
        input->push_back(backward_flow_frame);
      }

      if (!out_file_.empty()) {
        // Write to file.
        ofs_.write((char*)flow_data_x, sizeof(float) * width * height);
        ofs_.write((char*)flow_data_y, sizeof(float) * width * height);
      }

      render_flow_x = flow_data_x;
      render_flow_y = flow_data_y;
    }

    if (!video_out_stream_name_.empty()) {
      // Convert to HSV color image.
      VideoFrame* vid_frame = new VideoFrame(width, height, 3, width_step_,
                                             frame->pts());

      IplImage rgb_image;
      vid_frame->ImageView(&rgb_image);

      shared_ptr<IplImage> hsv_image = ImageFilter::cvCreateImageShared(width, height,
                                                                        IPL_DEPTH_8U, 3);

      for (int i = 0; i < height; ++i) {
        const float* x_ptr = render_flow_x + i * width;
        const float* y_ptr = render_flow_y + i * width;

        uchar* hsv_ptr = ImageFilter::RowPtr<uchar>(hsv_image, i);
        for (int j = 0; j < width; ++j, hsv_ptr +=3, ++x_ptr, ++y_ptr) {
          hsv_ptr[0] = (uchar) ((atan2(*y_ptr, *x_ptr) / M_PI + 1) * 90);
          hsv_ptr[1] = hsv_ptr[2] = (uchar) std::min<float>(
              sqrt(*y_ptr * *y_ptr + *x_ptr * *x_ptr)*20, 255.0);
        }
      }

      cvCvtColor(hsv_image.get(), &rgb_image, CV_HSV2BGR);
      input->push_back(shared_ptr<VideoFrame>(vid_frame));
    }

   } else {
    VideoFrame* vid_frame = new VideoFrame(width, height, 3, width_step_,
                                           frame->pts());

    IplImage rgb_image;
    vid_frame->ImageView(&rgb_image);
    cvZero(&rgb_image);
    input->push_back(shared_ptr<DataFrame>(vid_frame));
  }

  prev_img_.swap(image);
  delete hostRef;
  delete [] data;

  output->push_back(input);
  ++frame_number_;
  if (frame_number_ % 10 == 0) {
    std::cerr << "FlowUnit: Processed Frame #" << frame_number_ << "\n";
  }
}

void GPUFlowUnit::PostProcess(list<FrameSetPtr>* append) {
  // Close file.
  if (!out_file_.empty()) {
    ofs_.close();
  }
}

}  // namespace VideoFramework.
