/*
 *  region_flow.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 8/5/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef REGION_FLOW_H__
#define REGION_FLOW_H__

#include <string>
#include <list>
#include <vector>
using std::vector;

#include <opencv2/core/core_c.h>

#include "optical_flow_unit.h"
#include "video_unit.h"

namespace VideoFramework {

  inline bool VecInCylinder(CvPoint2D32f ref_vec, CvPoint2D32f vec, float alpha,
                            float dist) {
    // Is norm of vec in [alpha * |ref_vec|, 1/apha * |ref_vec|]?
    float n2_ref_vec = ref_vec.x * ref_vec.x + ref_vec.y * ref_vec.y;
    float n2_vec = vec.x * vec.x + vec.y * vec.y;

    const float alpha2 = alpha * alpha;
    const float alpha2_inv = 1.0 / alpha2;

    if (n2_vec < alpha2 * n2_ref_vec || n2_vec > alpha2_inv * n2_ref_vec)
      return false;

    // Is prependicular distance < dist?
    float proj_2 = vec.x * ref_vec.x + vec.y * ref_vec.y;
    proj_2 = proj_2 * proj_2 / n2_ref_vec;

    if(n2_vec - proj_2 < dist)
      return true;

    return false;
  }

  class RegionFlowFrame : public VideoFramework::DataFrame {
  public:
    typedef VideoFramework::OpticalFlowFrame::TrackedFeature Feature;
    typedef std::vector<Feature> FeatList;

    // Holds consistent features for each region.
    // Consistency is enforced as a local translational model.
    class RegionFlow {
    public:
      RegionFlow() {}
      RegionFlow(int id,
                 const FeatList& feat,
                 CvPoint2D32f mean,
                 CvPoint2D32f anchor,
                 const std::pair<CvPoint2D32f, CvPoint2D32f>& _min_match,
                 const std::pair<CvPoint2D32f, CvPoint2D32f>& _max_match)
          : region_id(id), features(feat), mean_vector(mean), anchor_pt(anchor),
            min_match(_min_match), max_match(_max_match) {}

    public:
      int region_id;
      FeatList features;
      CvPoint2D32f mean_vector;
      CvPoint2D32f anchor_pt;

      // Minimum and maximum inlier pair.
      // make_pair(location, vector)
      std::pair<CvPoint2D32f, CvPoint2D32f> min_match;
      std::pair<CvPoint2D32f, CvPoint2D32f> max_match;

      bool operator<(const RegionFlow& rhs) { return region_id < rhs.region_id; }

    private:
      friend class boost::serialization::access;
      template <class Archive>
      void serialize(Archive& ar, const unsigned int version) {
        ar & region_id;
        ar & features;
        ar & mean_vector.x & mean_vector.y;
        ar & anchor_pt.x & anchor_pt.y;
        ar & min_match.first.x & min_match.first.y;
        ar & min_match.second.x & min_match.second.y;
        ar & max_match.first.x & max_match.first.y;
        ar & max_match.second.x & max_match.second.y;
      }
    };

  public:
    RegionFlowFrame(int matched_frames = 1,
                    bool is_key_frame = true) : matched_frames_(matched_frames),
                                                 is_key_frame_(is_key_frame),
                                                 features_(matched_frames),
                                                 frame_ids_(matched_frames),
                                                 num_features_(matched_frames) {}

    void AddFeatures(const std::vector<RegionFlow>&, int frame_num, int frame_id);

    // Resizes vector and copies memory.
    // Features are sorted w.r.t. region_id.
    void FillVector(std::vector<RegionFlow>* features, int frame_num = 0) const;
    int NumberOfFeatures(int frame_num) const { return num_features_[frame_num]; }
    int Level() const { return hierarchy_level_; }
    void SetLevel(int l) { hierarchy_level_ = l; }
    int MatchedFrames() const { return matched_frames_; }

    // Returns position of matched frame #idx w.r.t current one, e.g.
    // previous frame is -1, previous key-frame could be -15.
    int FrameID(int idx) const { return frame_ids_[idx]; }
    bool IsKeyFrame() const { return is_key_frame_; }

  private:
    // Serialization stuff.
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar & boost::serialization::base_object<VideoFramework::DataFrame>(*this);
      ar & hierarchy_level_;
      ar & matched_frames_;
      ar & is_key_frame_;
      ar & features_;
      ar & frame_ids_;
      ar & num_features_;
    }

  protected:
    int hierarchy_level_;
    int matched_frames_;
    bool is_key_frame_;

    vector< vector<RegionFlow> > features_;
    vector<int> frame_ids_;
    vector<int> num_features_;                // Number of all features in all regions.

    DECLARE_DATA_FRAME(RegionFlowFrame)
  };

class RegionFlowUnit : public VideoFramework::VideoUnit {
public:
  RegionFlowUnit(int hierarchy_level = 0,
                 const std::string& vid_stream_name = "VideoStream",
                 const std::string& flow_stream_name = "OpticalFlow",
                 const std::string& seg_stream_name = "SegmentationStream",
                 const std::string& region_flow_stream_name = "RegionFlow");
  ~RegionFlowUnit();

  void SetHierarchyLevel(int l) { hierarchy_level_ = l; }

  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

protected:
  int hierarchy_level_;

  std::string vid_stream_name_;
  std::string flow_stream_name_;
  std::string seg_stream_name_;
  std::string region_flow_stream_name_;

  int vid_stream_idx_;
  int flow_stream_idx_;
  int seg_stream_idx_;

  int frame_width_;
  int frame_height_;
  std::vector<int> id_img_;
};

}

#endif // REGION_FLOW_H__
