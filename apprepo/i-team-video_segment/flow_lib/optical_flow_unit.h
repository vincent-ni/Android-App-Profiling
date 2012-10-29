/*
 *  optical_flow_unit.h
 *  flow_lib
 *
 *  Created by Matthias Grundmann on 7/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */


#ifndef OPTICAL_FLOW_UNIT_H__
#define OPTICAL_FLOW_UNIT_H__

#include "video_unit.h"
#include <boost/shared_array.hpp>
using boost::shared_array;

#include <boost/serialization/vector.hpp>
#include <boost/circular_buffer.hpp>
#include <opencv2/core/core_c.h>
#include <fstream>

#include "flow_utility_options.pb.h"
#include "flow_frame.pb.h"

namespace VideoFramework {

  // Holds points of a polygon.
  class PolygonFrame : public DataFrame {
  public:
    void SetPoints(const vector<CvPoint2D32f>& polygon) { polygon_ = polygon; }
    const vector<CvPoint2D32f>& GetPoints() const { return polygon_; }
  protected:
    vector<CvPoint2D32f> polygon_;

  private:
    DECLARE_DATA_FRAME(PolygonFrame);
  };

  bool WithinConvexPoly(const CvPoint2D32f& pt,
                        const vector<CvPoint2D32f>& poly,
                        float dist = 0);

  class OpticalFlowFrame : public DataFrame {
  public:
    struct TrackedFeature {
      TrackedFeature() { }
      TrackedFeature(const CvPoint2D32f& loc_, const CvPoint2D32f& vec_)
          : loc(loc_), vec(vec_) { }
      CvPoint2D32f loc;
      CvPoint2D32f vec;

      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive & ar,
                     const unsigned int file_version){
        ar & loc.x & loc.y;
        ar & vec.x & vec.y;
      }
    };

  public:
    // New Frame matched against #matched_frames,
    OpticalFlowFrame(int matched_frames = 1, bool is_key_frame = true);

    // Returns number of added features.
    // Feature displacements above threshold_dist are discarded.
    // If with_noise_sigma is different than zero, gaussian noise will be added.
    // match_idx specifies the index of the matched frame having a relative id
    // of relative_id w.r.t. the current frame.
    int AddFeatures(const CvPoint2D32f* prev_features,
                    const CvPoint2D32f* cur_features,
                    const char* feat_mask,
                    int max_features,
                    float threshold_dist,
                    int match_idx,
                    int relative_id,
                    float with_noise_sigma = 0.f);

    // Outlier rejection by imposing local translation model within a grid cell
    // of size (frame_width, frame_height) * (grid_ratio_x, grid_ratio_y). We use several
    // grids of varying size (scaled by 1 << i, i in [0, num_scales) ) and shifted by
    // j * grid_size / num_offsets, j in [0, grid_size). Returned is the union of inlier
    // features across all grids.
    void ImposeLocallyConsistentGridFlow(int frame_width,
                                         int frame_height,
                                         int frame_num,
                                         float grid_ratio_x,
                                         float grid_ratio_y,
                                         int num_offsets,
                                         int num_scales);

    // Returns all matches in the form
    // pt, match in frame -1, match in frame -2, ...
    // If a point does not have match in frame - N, the list is truncated to the match in
    // frame - N + 1. Only points with matches >= min_matches are returned (use 0
    // to return all matches).
    typedef vector<vector<CvPoint2D32f> > FeatureMatchList;
    void GetFeatureMatchList(int frame_width,
                             int min_matches,
                             FeatureMatchList* feature_list);

    static void FeatureMatchListToFlowFrame(const FeatureMatchList& feature_matches,
                                            FlowFrame* flow_frame);

    // Resizes vector and copies memory.
    void FillVector(std::vector<TrackedFeature>* features, int frame_num = 0) const;
    int MatchedFrames() const { return matched_frames_; }
    int NumberOfFeatures(int frame_num) const { return features_[frame_num].size(); }

    // Returns position of matched frame #idx w.r.t current one, e.g.
    // previous frame is -1, previous key-frame could be -15.
    int FrameID(int idx) const { return frame_ids_[idx]; }
    bool IsKeyFrame() const { return is_key_frame_; }

  private:
    // Serialization stuff.
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar,
                   const unsigned int file_version) {

      ar & boost::serialization::base_object<DataFrame>(*this);
      ar & matched_frames_;
      ar & is_key_frame_;
      ar & features_;
      ar & frame_ids_;
    }

  protected:
    int matched_frames_;
    bool is_key_frame_;
    vector< vector<TrackedFeature> > features_;
    vector<int> frame_ids_;

  private:
    DECLARE_DATA_FRAME(OpticalFlowFrame)
  };


  class OpticalFlowUnit : public VideoUnit {
  public:
    // Numbers of features is determined by
    // frame_width * feat_density * frame_height * feat_density.
    // Features will be matched to a total of matched_frames
    // previous frames, no key-frames will be considered.
    // An optional polygon stream of interest can be fed back into the unit, only
    // features within the polygon will be tracked.
    // If matching stride > 1, matches are spaced by matching stride
    // (starting from prev. frame).
    OpticalFlowUnit(float feat_density,
                    int matched_frames = 1,
                    int matching_stride = 1,  // Matched frames are spaced by this amount.
                    const string& video_stream_name = "LuminanceStream",
                    const string& flow_stream_name = "OpticalFlow",
                    const string& poly_stream_name = "");

    // Features are matched to the previous frame and the previous key-frame.
    // Requires FeedbackStreams to be attached, to feed previous segmentation
    // and RegionFlow back into OpticalFlowUnit.
    // If in addition a boolean KeyFrameStream is specified, it will we used
    // for dynamic key_frame spacing.
    OpticalFlowUnit(float feat_density,
                    int key_frame_spacing,
                    const string& video_stream_name,
                    const string& flow_stream_name,
                    const string& seg_stream_name,
                    const string& region_flow_stream_name,
                    const string& key_frame_stream_name = "");

    virtual ~OpticalFlowUnit();

    virtual bool OpenStreams(StreamSet* set);

    virtual bool OpenFeedbackStreams(const StreamSet& set);

    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);

    virtual void ProcessFrame(FrameSetPtr input, const FrameSet& feedback_input,
                              list<FrameSetPtr>* output);

    virtual void PostProcess(list<FrameSetPtr>* append);

  protected:
    // Tracks prev. Frame, with optional convex polygon of interest.
    void PreviousFrameTrack(FrameSetPtr input, list<FrameSetPtr>* output,
                            const vector<CvPoint2D32f>* polygon = 0);

    void KeyFrameTrack(FrameSetPtr input, const FrameSet& feedback_input,
                       list<FrameSetPtr>* output);

  protected:
    virtual bool SeekImpl(int64_t pts);

  private:
    float feat_density_;
    int max_features_;

    int video_stream_idx_;
    int processed_frames_;

    int frame_num_;
    int matching_stride_;

    string video_stream_name_;
    string flow_stream_name_;

    string seg_stream_name_;              // Additional segmentation input stream.
    string region_flow_stream_name_;
    string poly_stream_name_;
    string key_frame_stream_name_;

    int frame_width_;
    int frame_height_;

    bool keyframe_tracking_;
    int seg_stream_idx_;
    int region_flow_idx_;
    int poly_stream_idx_;
    int key_frame_stream_idx_;
    int last_key_frame_;

    // Internal data structures.
    boost::circular_buffer< shared_ptr<IplImage> > prev_image_;
    shared_ptr<IplImage> eig_image_;
    shared_ptr<IplImage> tmp_image_;

    shared_array<CvPoint2D32f> prev_features_;
    shared_array<CvPoint2D32f> cur_features_;

    shared_ptr<IplImage> cur_pyramid_;
    vector< shared_ptr<IplImage> > prev_pyramid_;

    shared_array<char> flow_status_;
    shared_array<float> flow_track_error_;

    shared_array<int> id_img_;
  };


  // Performs some utility functions on flow frames.
  class FlowUtilityUnit : public VideoUnit {
    typedef OpticalFlowFrame::FeatureMatchList FeatureMatchList;
   public:
    FlowUtilityUnit(FlowUtilityOptions& options) : options_(options) { }

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

  private:
    void PlotFlow(const FeatureMatchList& flow_frame,
                  int max_matches,
                  IplImage* frame);

    FlowUtilityOptions options_;

    std::ofstream ofs_;

    int video_stream_idx_;
    int flow_stream_idx_;

    int frame_width_;
    int frame_height_;
  };

}

#endif // OPTICAL_FLOW_UNIT_H__
