/*
 *  region_flow.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 8/5/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */
#include "assert_log.h"
#include "image_util.h"
#include "optical_flow_unit.h"
#include "region_flow.h"
#include "segmentation_util.h"

using namespace std;
using namespace ImageFilter;

namespace VideoFramework {

  typedef unsigned char uchar;
  DEFINE_DATA_FRAME(RegionFlowFrame)

  void RegionFlowFrame::AddFeatures(const std::vector<RegionFlow>& rf, int frame, int frame_id) {
    ASSURE_LOG(frame < matched_frames_);
    features_[frame] = rf;

    int num_features = 0;
    for (vector<RegionFlow>::const_iterator i = rf.begin(); i != rf.end(); ++i)
      num_features += i->features.size();

    num_features_[frame] = num_features;
    frame_ids_[frame] = frame_id;
  }

  void RegionFlowFrame::FillVector(std::vector<RegionFlow>* region_flow, int frame) const {
    ASSURE_LOG(frame < matched_frames_);
    *region_flow = features_[frame];
  }

  RegionFlowUnit::RegionFlowUnit(int level,
                                 const string& vid_stream_name,
                                 const string& flow_stream_name,
                                 const string& seg_stream_name,
                                 const string& region_flow_stream_name)
  : hierarchy_level_(level),
    vid_stream_name_(vid_stream_name),
    flow_stream_name_(flow_stream_name),
    seg_stream_name_(seg_stream_name),
    region_flow_stream_name_(region_flow_stream_name) {

  }

  RegionFlowUnit::~RegionFlowUnit() {

  }

  bool RegionFlowUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);

    if (vid_stream_idx_ < 0) {
      std::cerr << "RegionFlowUnit::OpenStreams: "
                << "Could not find Video stream!\n";
      return false;
    }

    // Get video stream info.
    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                        set->at(vid_stream_idx_).get());
    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();


    // Get optical flow stream.
    flow_stream_idx_ = FindStreamIdx(flow_stream_name_, set);

    if (flow_stream_idx_ < 0) {
      std::cerr << "RegionFlowUnit::OpenStreams: Could not find optical flow stream.\n";
      return false;
    }

    // Get segmentation stream.
    seg_stream_idx_ = FindStreamIdx(seg_stream_name_, set);

    if (seg_stream_idx_ < 0) {
      std::cerr << "RegionFlowUnit::OpenStreams: "
                << "Could not find Segmentation stream!\n";
      return false;
    }

    // Add RegionFlowFrameStream.
    DataStream* region_flow_stream = new DataStream(region_flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(region_flow_stream));

    // Allocate frame data structures.
    id_img_.resize(frame_width_ * frame_height_);

    return true;
  }

  void RegionFlowUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    // Get segmentation.
    const DataFrame* seg_frame = input->at(seg_stream_idx_).get();
    Segment::SegmentationDesc desc;

    desc.ParseFromArray(seg_frame->data(), seg_frame->size());

    // Get optical flow.
    const OpticalFlowFrame* flow_frame =
    dynamic_cast<const OpticalFlowFrame*>(input->at(flow_stream_idx_).get());

    typedef VideoFramework::OpticalFlowFrame::TrackedFeature Feature;
    RegionFlowFrame* rff = new RegionFlowFrame(flow_frame->MatchedFrames(),
                                               flow_frame->IsKeyFrame());

    // Convert to segmentation id image.
    Segment::SegmentationDescToIdImage(&id_img_[0],
                                       frame_width_ * sizeof(int),    // width_step
                                       frame_width_,
                                       frame_height_,
                                       hierarchy_level_,
                                       desc,
                                       0);  // no hierarchy.


    // Process each OpticalFlowFrame.
    for (int t = 0; t < flow_frame->MatchedFrames(); ++t) {
      vector<Feature> features;
      flow_frame->FillVector(&features, t);

      // Could be empty.
      if (features.size() == 0)
        continue;


      typedef vector<Feature> FeatList;
      vector<FeatList> region_features(desc.region(desc.region_size() - 1).id() + 1);

      for (vector<Feature>::const_iterator feat = features.begin();
           feat != features.end();
           ++feat) {
        // Get feature's region id.
        int region_id = id_img_[(int)feat->loc.y * frame_width_ + (int)feat->loc.x];
        ASSERT_LOG(region_id < region_features.size()) << "Region id exceeds max_id";

        region_features[region_id].push_back(*feat);
      }

      // Run RANSAC on each region.
      const int min_inlier_set = 3;
      vector< RegionFlowFrame::RegionFlow > region_flow;

      vector<int> inlier_set;
      vector<int> best_inlier_set;

      for (vector<FeatList>::iterator f_list = region_features.begin();
           f_list != region_features.end();
           ++f_list) {

        if (f_list->size() < min_inlier_set) {
          continue;
        }

        best_inlier_set.clear();

        int max_iterations = 100;
        for (int i = 0; i < max_iterations; ++i) {
          // Pick vector randomly.
          CvPoint2D32f vec = f_list->at(rand() % f_list->size()).vec;
          inlier_set.clear();

          // Compute inlier set.
          for (FeatList::const_iterator j = f_list->begin(); j != f_list->end(); ++j) {
            CvPoint2D32f diff = j->vec - vec;
            if (sq_norm(diff) < 3.84) {
              //  if (VecInCylinder(vec, j->vec, 0.7, 3.84)) {
              inlier_set.push_back(j - f_list->begin());
            }
          }

          // Assure probability of an outlier is not zero.
          const float eps = clamp(1.f  - (float)inlier_set.size() / (float) f_list->size(),
                                  1e-6f, 1.f - 1e-6f);
          // -4.6052 == log(1 - 0.99)
          max_iterations = (int)(-4.6052f / std::log(1.f - (1.f - eps)));
          max_iterations = std::min(max_iterations, 10);

          if (inlier_set.size() > best_inlier_set.size() && inlier_set.size() >= min_inlier_set)
            inlier_set.swap(best_inlier_set);
        }

        if (!best_inlier_set.empty()) {
          // Estimate model = mean vector.
          CvPoint2D32f mean_vec = cvPoint2D32f(0, 0);
          CvPoint2D32f anchor_pt = cvPoint2D32f(0, 0);
          vector<Feature> inlier_list;

          std::pair<CvPoint2D32f, CvPoint2D32f> min_match =
              std::make_pair(cvPoint2D32f(0,0), cvPoint2D32f(1e10, 1e10));

          std::pair<CvPoint2D32f, CvPoint2D32f> max_match =
              std::make_pair(cvPoint2D32f(0,0), cvPoint2D32f(0, 0));

          for (vector<int>::const_iterator inl = best_inlier_set.begin();
               inl != best_inlier_set.end();
               ++inl) {
            const Feature& tmp = f_list->at(*inl);
            mean_vec = mean_vec + tmp.vec;
            anchor_pt = anchor_pt + tmp.loc;

            float n = sq_norm(tmp.vec);
            if (n < sq_norm(min_match.second))
              min_match = std::make_pair(tmp.loc, tmp.vec);

            if (n > sq_norm(max_match.second))
              max_match = std::make_pair(tmp.loc, tmp.vec);

            inlier_list.push_back(tmp);
          }

          float n = 1.0f / best_inlier_set.size();
          typedef RegionFlowFrame::RegionFlow RegionFlow;
          region_flow.push_back(RegionFlow(f_list - region_features.begin(),
                                           inlier_list,
                                           cvPoint2D32f(mean_vec.x * n, mean_vec.y * n),
                                           cvPoint2D32f(anchor_pt.x * n, anchor_pt.y * n),
                                           min_match,
                                           max_match));
        }
      }

      rff->AddFeatures(region_flow, t, flow_frame->FrameID(t));
    }

    rff->SetLevel(std::min(hierarchy_level_, desc.hierarchy_size()));
    input->push_back(shared_ptr<RegionFlowFrame>(rff));
    output->push_back(input);

  }

  void RegionFlowUnit::PostProcess(list<FrameSetPtr>* append) {

  }
}
