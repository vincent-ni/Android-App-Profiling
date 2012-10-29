/*
 *  color_checker.h
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef STABILIZE_H__
#define STABILIZE_H__

#include "optical_flow_unit.h"
#include "video_unit.h"
#include <fstream>

namespace VideoFramework {

  typedef OpticalFlowFrame::TrackedFeature Feature; 
  
  // Each patch is characterized by mean and var of each color channel.
  struct ColorFeature {
    CvPoint3D32f mean;
    CvPoint3D32f var; 

    float Distance(const ColorFeature& rhs);
  };
  
  struct FeaturePatch {
    FeaturePatch() { }
    FeaturePatch(const Feature& feature_, const ColorFeature& color_feature_) :
       feature(feature_), color_feature(color_feature_) { }
    Feature feature;
    ColorFeature color_feature;
  };
  
  typedef vector<vector<FeaturePatch> > FeaturePatchGrid;
  
  class StabilizeUnit : public VideoUnit {
  public:
    StabilizeUnit(const std::string& video_stream_name = "VideoStream",
                  const std::string& flow_stream_name = "OpticalFlow");
    
    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);
    
  protected:
    // Estimates 4 dof similarity or 6 dof affine transform.
    void EstimateLinearMotion(const vector<Feature>& features,
                         bool similarity,
                         CvMat* model);
    
    void EstimatePerspective(const vector<Feature>& features,
                             const CvMat* pre_transform,
                             CvMat* model);
    
    void EstimateHomography(const vector<Feature>& features,
                            CvMat* homography);
    
    void FillFeaturePatch(const IplImage* image,
                          CvPoint pt,
                          int patch_rad,
                          ColorFeature* feature);
    
    enum PushPullFilter {BINOMIAL_3x3, BINOMIAL_5x5};
    void PushPullErrors(const vector<Feature>& feat_erross, 
                        CvPoint origin,
                        CvSize domain_sz,
                        PushPullFilter filter,
                        CvMat* disp);
    
    void GetCorrection(const vector<Feature>& feat_errors,
                       const IplImage* image,
                       CvMat* disp_x,
                       CvMat* disp_y);
                       
    std::string video_stream_name_;
    std::string flow_stream_name_;
    
    shared_ptr<CvMat> homog_composed_;
    shared_ptr<IplImage> input_;
    shared_ptr<CvMat> map_x_;
    shared_ptr<CvMat> map_y_;

    // Homographies and feature errors.
    vector<shared_ptr<CvMat> > homographies_;
    vector<shared_ptr<CvMat> > correction_x_backward_;
    vector<shared_ptr<CvMat> > correction_y_backward_;
    
    vector<shared_ptr<CvMat> > correction_x_forward_;
    vector<shared_ptr<CvMat> > correction_y_forward_;
    
    vector<shared_ptr<CvMat> > camera_path_;
    vector<IplImage> frame_views_;
    
    int feat_grid_dim_x_;
    int feat_grid_dim_y_;
    int feat_grid_sz_;
    
    list<FrameSetPtr> frame_sets_;
    
    bool output_flushed_;
    
    int video_stream_idx_;
    int flow_stream_idx_;
    
    int frame_width_;
    int frame_height_;
    int frame_num_;
  };
  
  
}

#endif  // STABILIZE_H__