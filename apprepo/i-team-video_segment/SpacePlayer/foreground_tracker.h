/*
 *  foreground_tracker.h
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 8/5/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef FOREGROUND_TRACKER_H__
#define FOREGROUND_TRACKER_H__

#include "video_unit.h"
#include "segmentation.h"
#include "region_flow.h"
#include "image_util.h"
using namespace ImageFilter;

#include <string>
#include <list>
#include <vector>
#include <fstream>

#include <ext/hash_map>
using __gnu_cxx::hash_map;

#include <boost/shared_ptr.hpp>
#include <QTransform>

class ForegroundTracker : public VideoFramework::VideoUnit {
public:
  static void lev_func(double* p, double* hx, int m, int n, void* data);
  static void lev_jac(double* p, double* hx, int m, int n, void* data);
  
  typedef VideoFramework::RegionFlowFrame::RegionFlow RegionFlow;
  typedef VideoFramework::RegionFlowFrame RegionFlowFrame;
  typedef VideoFramework::OpticalFlowFrame::TrackedFeature Feature;
  typedef std::vector<Feature> FeatList;
  
  // Writes inlier (background) features and per-frame transforms to
  // feature file.
  // Bundle adjustment is run every ba_max_frames or if the 
  // RegionFlowStream signals that a keyframe was reached.
  // If frame ratio is below ba_frame_area_ratio, a bool = true will be
  // outputted on the KeyFrameStream.
  ForegroundTracker(const std::string& feature_file,
                    bool use_key_frame_tracking,
                    int ba_max_frames,
                    float ba_frame_area_ratio,
                    const std::string& vid_stream_name = "VideoStream",
                    const std::string& seg_stream_name = "SegmentationStream",
                    const std::string& region_flow_stream_name = "RegionFlow",
                    const std::string& key_frame_stream_name = "KeyFrameStream",
                    bool debug_mode = true);
  
  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

protected:
  enum RegionType { REGION_TYPE_INLIER,
                    REGION_TYPE_OUTLIER, 
                    REGION_TYPE_UNTRACKED };
  struct RegionProb {
    RegionProb(int r_id = -1) : region_id(r_id), region_type(REGION_TYPE_UNTRACKED),
                                epipolar_dist(0), flow(cvPoint2D32f(0.f,0.f)), hist(10, 20) {}
    
    int region_id;
    RegionType region_type;
    float epipolar_dist;
    CvPoint2D32f flow;
    
    Segment::ColorHistogram hist;
  };
  
  // Our model is a similarity transform with 4 degrees of freedom:
  // - uniform scale
  // - rotation around axis
  // - translation x
  // - translation y
  struct SimilarityModel {
    SimilarityModel() : s(1.f), phi(0.f), t_x(0.f), t_y(0.f) {}
    SimilarityModel(float _s, float _phi, float _t_x, float _t_y) : 
      s(_s), phi(_phi), t_x(_t_x), t_y(_t_y) {}
    float s;
    float phi;
    float t_x;
    float t_y;
    
    // Concats this model with rhs.
    // like *this * rhs in matrix notation. (column major)
    // s_1 R_1 x_1 + t_1 = s_1 R_1 ( s_0 R_0 x_0 + t_0 ) + t_1
    //  s_1 s_0 R_1 R_0 x_0 + s_1 R_1 t_0 + t_1
    
    SimilarityModel Concat(const SimilarityModel& rhs) const {
      SimilarityModel m = *this;
      m.s *= rhs.s;
      m.phi += rhs.phi;
      CvPoint2D32f rhs_t_rot = rotate(cvPoint2D32f(rhs.t_x, rhs.t_y), phi) * s;
      m.t_x += rhs_t_rot.x;
      m.t_y += rhs_t_rot.y;
      return m;
    }
    
    SimilarityModel Inverted() const {
      CvPoint2D32f t_inv = rotate(cvPoint2D32f(t_x, t_y), -phi) * (-1.0 / s);
      return SimilarityModel(1.0 / s, -phi, t_inv.x, t_inv.y);      
    }
    
    QTransform ToQTransform() const {
      float co = cos(phi);
      float si = sin(phi);
      return QTransform(co * s, si * s, -si * s, co * s, t_x, t_y);
    }
  };
  
  enum ModelParam { MODEL_PARAM_S, MODEL_PARAM_PHI, MODEL_PARAM_TX, MODEL_PARAM_TY };
  static double GetModelParam(int model_id, ModelParam param, const double* p);
  
  struct FrameMatch {
    SimilarityModel model;
    int match_id;
    vector<RegionFlow> flow;
    vector<int> inlier_regions;     // regions used to estimate model
    vector<int> outlier_regions;    // regions that are clearly outlier.
    // Note: union inlier_regions and outlier regions is just the set of trackable regions,
    // not the set of all regions!
    
    shared_ptr<vector<float> > inlier_score;
    int num_features;
    int level;
    int num_regions;
  };
  
  typedef std::list<FrameMatch> FrameMatchList;
  
  struct PointMatch {
    PointMatch(CvPoint2D32f f, CvPoint2D32f s, float w = 1.0) : first(f), second(s), weight(w) {}
    CvPoint2D32f first;
    CvPoint2D32f second;
    float weight;
  };
  
  // Represents point matches between two models.
  // If match_id is negative, external model is used as constraint.
  struct ModelMatch {
    vector<PointMatch> pt_matches;
    SimilarityModel external_model;
    int match_id;
  };
  
  struct LevmarParams {
    vector< std::list<ModelMatch> >* matches;
    float error_threshold;
  };
  
protected:
  
  // For each matched frame in flow_frame, compute Similarity model
  // with inliers and add to matches.
  // Uses pre-transform (location + trans) * scale before model computation.
  void ComputeMatchedModels(const RegionFlowFrame* flow_frame,
                            const CvPoint2D32f& trans,
                            float scale,
                            FrameMatchList* matches);
  
  // Estimates model from from region_flow and outputs set of inlier and outlier regions.
  // Assumes all features were normalized by some translation and a scale of scale,
  // distance between matching points is frame_dist.
  // inlier_score_output returns for each region the average distance each to feature point
  // to its matching epipolar line in pixels.
  // Returns number of features used to estimate the model.
  int EstimateModel(const vector<RegionFlow>& region_flow, 
                    int total_features,
                    float scale,
                    int frame_dist,
                    vector<int>* inlier_regions,
                    vector<int>* outlier_regions,
                    SimilarityModel* model,
                    vector<float>* inlier_score_output = 0);

  // Estimates 4 dog. 2d similarity transform
  // Umeyama, 1991: Least-squares estimation of transformation parameters
  // between two point patterns.
  void SimilarityFromPoints(const vector<CvPoint2D32f>& point_x,
                            const vector<CvPoint2D32f>& point_y,
                            SimilarityModel* model);
  
  // Runs bundle adjustment on a FrameMatchList.
  // Each frame is expected to be matched to every other.
  void BundleAdjustSimilarity(const std::list<FrameMatchList>& matches,
                              vector<SimilarityModel>* computed_models);
  
  void PreConstructGCGraph(const vector<RegionProb>& region_probs,
                           const Segment::SegmentationDesc& cur_seg,
                           const vector<RegionProb>& prev_probs,
                           const Segment::SegmentationDesc* prev_seg);
  
  void PropagateForeground(const IplImage* prev_id_img_,
                           const std::vector<RegionProb>& old_probs,
                           const hash_map<int, int> old_map,
                           const Segment::SegmentationDesc& old_seg,
                           const vector<RegionFlow>& cur_flow, 
                           const Segment::SegmentationDesc& cur_seg,
                           const hash_map<int, int> cur_map,
                           std::vector<RegionProb>* cur_probs);

protected:
  std::string feature_file_;
  bool use_key_frame_tracking_;
  int ba_max_frames_;
  float ba_frame_area_ratio_;
  int last_ba_frame_;
  
  std::string vid_stream_name_;
  std::string seg_stream_name_;
  std::string region_flow_stream_name_;
  std::string key_frame_stream_name_;
  bool is_debug_mode_;
  
  int vid_stream_idx_;
  int flow_stream_idx_;
  int seg_stream_idx_;
  int frame_number_;
  
  std::list<FrameMatchList>  frame_match_list_;
  
  SimilarityModel prev_ba_model_;
  SimilarityModel concat_model_;
  std::ofstream ofs_feat_;
  std::ofstream ofs_debug_;
  
  std::vector<RegionProb> prev_probs_;
  vector<RegionFlow> prev_flow_;
  
  shared_ptr<IplImage> id_img_;
  shared_ptr<IplImage> prev_id_img_;
  shared_ptr<Segment::SegmentationDesc> prev_seg_desc_;
  int frame_width_;
  int frame_height_;
  
  // GC information.
  vector< vector<int> > neighbor_indexes_; // for each node, list of neighbors.
  vector< vector<int> > neighbor_weights_;
  // First Label: background = inlier, Second: foreground = outlier;
  vector< std::pair<int, int> > data_costs_;
  vector<int> frame_id_shift_;   // for each frame hold the id-shift added to each region.
  
  // Output info.
  vector<SimilarityModel> models_to_output_;
};

float PolyArea(const QPolygon& p, float frame_height);

#endif // FOREGROUND_TRACKER_H__