/*
 *  joint_segmentation_unit.h
 *  segmentation
 *
 *  Created by Matthias Grundmann on 9/22/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef JOINT_SEGMENTATION_UNIT_H__
#define JOINT_SEGMENTATION_UNIT_H__

#include "video_unit.h"
#include "video_pool.h"
#include "segmentation.h"
#include "sift_unit.h"

namespace Segment {
  
using namespace VideoFramework;
class JointSegmentationUnit : public VideoPool {
public:
  JointSegmentationUnit(const string& video_stream_name = "VideoStream",
                        const string& sift_stream_name = "SiftStream",
                        const string& segment_stream_name = "SegmentationStream",
                        const string& video_out_stream_name = "JointVideoStream",
                        const string& segment_out_stream_name = "JointSegmentStream");
  
  // Redefine public pool interface.
  // Pool processing functions.
  virtual void OpenPoolStreams(const vector<StreamSet>& stream_sets);
  virtual void ProcessAllPoolFrames(const FrameSetPtrPool& frame_sets);
  
  // Pool output functions.
  virtual bool OpenRootStream(StreamSet* set);
  virtual void RootPostProcess(list<FrameSetPtr>* append);
  
private:
  struct SortedSift {
    SortedSift(const SiftFrame::SiftFeature* feature_,
               int region_id_,
               int frame_) : feature(feature_), region_id(region_id_), frame(frame_) { }

    const SiftFrame::SiftFeature* feature;
    int region_id;
    int frame;
    
    // lexicographic sort w.r.t. (region_id, frame)
    bool operator<(const SortedSift& rhs) {
      return region_id < rhs.region_id || (region_id == rhs.region_id && frame < rhs.frame);
    }
  };
  
  struct BestSiftMatch {
    BestSiftMatch() : min_dist(1e12), min_dist_idx(-1), second_min_dist(1e12) { }
    float min_dist;
    int min_dist_idx;
    float second_min_dist;
  };
  
  struct RegionMatch {
    int region_id_1;
    int level_1;
    int region_id_2;
    int level_2;
    float strength;
  };
  
  void ComputeAffinityMatrix(const FrameSetPtrPool& frame_sets,
                             vector<RegionMatch>* region_matches);
  
  void VisualizeAndOutputSift(const FrameSetPtrPool& frame_sets);
  void VisualizeAndOutputRegionMatches(const FrameSetPtrPool& frame_sets,
                                       const vector<RegionMatch>& region_matches);
  
  void PerformJointSegmentation(const FrameSetPtrPool& frame_sets,
                                const vector<RegionMatch>& region_matches);  
  
private:
  string video_stream_name_;
  string sift_stream_name_;
  string segment_stream_name_;
  string video_out_stream_name_;
  string segment_out_stream_name_;
  
  int output_width_;
  int output_height_;
  int output_width_step_;
  
  bool output_drained_;
  
  vector<int> video_stream_indices_;
  vector<int> frame_widths_;
  vector<int> frame_heights_;
  vector<shared_ptr<IplImage> > region_id_images_;
  vector<shared_ptr<IplImage> > frame_buffer_;
  
  vector<shared_ptr<VideoFrame> > output_frames_;
  
  vector<int> sift_stream_indices_;
  vector<int> segment_stream_indices_;
  
  shared_ptr<Segmentation> segment_;
};
  
}

#endif   // JOINT_SEGMENTATION_UNIT_H__

