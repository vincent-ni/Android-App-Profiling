/*
 *  region_descriptor.h
 *  segmentation
 *
 *  Created by Matthias Grundmann on 10/4/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef REGION_DESCRIPTOR_H__
#define REGION_DESCRIPTOR_H__

#include "common.h"

#include <opencv2/core/core.hpp>

#include "histograms.h"
#include "segmentation.pb.h"

namespace cv {
  class Mat;
}

namespace Segment {

// Generic framework to define region descriptors / features for hierarchical
// segmentation stage.
// Per descriptors three classes are involved:
// 1.) A descriptor extractor derived from RegionDescriptorExtractor. The descriptors
//     extractor's task is to pre-compute any information necessary for extracting
//     descriptors during AddOverSegmentation.
//     The actual form of this extraction is completely up to the user.
// 2.) The extractor defines a abstract interface to create corresponding
//     RegionDescriptor's. During graph creation each RegionDescriptor is passed
//     the oversegmented regions rasterization and the extractor which is used to add
//     to build up the descriptor.
//     Further, each descriptor must support hierarchical merging functions to allow
//     the creation of descriptors of super-regions.
// 3.) An optional protobuffer that extends RegionFeatures in segmentation.proto.
//     Each RegionDescriptor is called to add itself via an extension to RegionFeatures
//     which is used save descriptors to file, e.g. to perform learning.

class RegionDescriptorExtractor;
class SegmentationDesc_Rasterization;

class RegionDescriptor {
public:
  RegionDescriptor() { }
  virtual ~RegionDescriptor() = 0;

  // Called exactly once per frame.
  virtual void AddFeatures(const SegmentationDesc_Rasterization& raster,
                           const RegionDescriptorExtractor& extractor,
                           int frame_number) = 0;

  // Called by framework to determine similarity between two descriptors.
  virtual float RegionDistance(const RegionDescriptor& rhs) const = 0;

  // Descriptors are spatio-temporal and multiple frames are used to compile
  // a desciptor. This function is called when all frames have been added.
  // This function can be called multiple times!
  virtual void PopulatingDescriptorFinished() {}

  // During hierarchical segmentation RegionDescriptors are merged and cloned.
  // It is guaranteed that both functions will only be called after
  // PopulatingDescriptorFinished() has been issued.
  virtual void MergeWithDescriptor(const RegionDescriptor& rhs) = 0;
  virtual RegionDescriptor* Clone() const = 0;

  // Descriptors can optionally have a corresponding feature defined as extension to
  // RegionFeatures in the segmentation.proto. This function is called on output if
  // requested by user to save descriptors to file.
  virtual void AddToRegionFeatures(RegionFeatures* features) const { }
protected:
};

class RegionDescriptorExtractor {
 public:
  RegionDescriptorExtractor() {}
  virtual ~RegionDescriptorExtractor() = 0;

  virtual RegionDescriptor* CreateDescriptor() const = 0;
};

typedef vector<shared_ptr<RegionDescriptorExtractor> > DescriptorExtractorList;

// Generic region distance.
// As argument a list of per-region descriptor distances is passed.
// Region distance is expected to derive a normalized distance from it.
class RegionDistance {
 public:
  virtual float Evaluate(const vector<float>& descriptor_distances) const = 0;
  // Feature Dimension / Number of descriptors this distance is defined for.
  virtual int NumDescriptors() const = 0;
};

// Zero if all distances are close to zero, one if at least one descriptor distance
// is zero.
template<int dimension>
class SquaredORDistance : public RegionDistance {
  virtual float Evaluate(const vector<float>& descriptor_distances) const {
    float result = 1.0f;
    for (int i = 0; i < dimension; ++i) {
      result *= (1.0f - descriptor_distances[i]);
    }
    result = 1.0f - result;
    return result * result;
  }

  virtual int NumDescriptors() const { return dimension; }
};

// Concrete implementation for a variety of different RegionDescriptor's / Extractors.

class AppearanceExtractor;
class AppearanceDescriptor3D : public RegionDescriptor {
 public:
  AppearanceDescriptor3D(int luminance_bins, int color_bins);
  virtual void AddFeatures(const SegmentationDesc_Rasterization& raster,
                           const RegionDescriptorExtractor& extractor,
                           int frame_number);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;
  virtual void PopulatingDescriptorFinished();
  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;
  virtual void AddToRegionFeatures(RegionFeatures* features) const;

 private:
  scoped_ptr<ColorHistogram> color_histogram_;
  bool is_populated_;
};

class WindowedAppearanceDescriptor : public RegionDescriptor {
 public:
  WindowedAppearanceDescriptor(int window_size, int luminance_bins, int color_bins);
  virtual void AddFeatures(const SegmentationDesc_Rasterization& raster,
                           const RegionDescriptorExtractor& extractor,
                           int frame_number);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;
  virtual void PopulatingDescriptorFinished();
  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;
  virtual void AddToRegionFeatures(RegionFeatures* features) const;

 private:
  // Returns 3 element gain change of frame- w.r.t. anchor_log_average.
  static vector<float> GetGainChange(const vector<double>& anchor_log_average,
                                     const vector<double>& frame_log_average);

  // Histogram and log_average (in Lab color space) of anchor frame (first frame).
  struct CalibratedHistogram {
    CalibratedHistogram(const int lum_bins,
                        const int col_bins,
                        const vector<double>& log_average_)
        : color_hist(lum_bins, col_bins),
          log_average(log_average_) {
    }

    ColorHistogram color_hist;
    vector<double> log_average;
  };

  // One histogram per window.
  typedef vector< shared_ptr<CalibratedHistogram> > WindowHistograms;
  WindowHistograms window_;

  int window_size_;
  int compare_radius_;
  int luminance_bins_;
  int color_bins_;
  bool is_populated_;
};

class AppearanceExtractor : public RegionDescriptorExtractor {
 public:
  AppearanceExtractor(int luminance_bins,
                      int color_bins,
                      const cv::Mat& rgb_frame);

  //AppearanceDescriptor3D* CreateDescriptor() const {
  //  return new AppearanceDescriptor3D(luminance_bins_, color_bins_);
  //}

  WindowedAppearanceDescriptor* CreateDescriptor() const {
    return new WindowedAppearanceDescriptor(20, luminance_bins_, color_bins_);
  }

  const cv::Mat& LabFrame() const { return *lab_frame_; }
  const vector<double>& LogAverageValues() const { return log_average_values_; }

 private:
  int luminance_bins_;
  int color_bins_;
  shared_ptr<cv::Mat> lab_frame_;

  // Log average of across each channel.
  vector<double> log_average_values_;
};



class FlowExtractor;
class FlowDescriptor : public RegionDescriptor {
 public:
  FlowDescriptor(int flow_bins);

  virtual void AddFeatures(const SegmentationDesc_Rasterization& raster,
                           const RegionDescriptorExtractor& extractor,
                           int frame_num);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;
  virtual void PopulatingDescriptorFinished();

  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;

 private:
  // A histogram per frame.
  vector< shared_ptr<VectorHistogram> > flow_histograms_;
  int flow_bins_;
  bool is_populated_;
};

class FlowExtractor : public RegionDescriptorExtractor {
 public:
  FlowExtractor(int flow_bins,
                const cv::Mat& flow_x,
                const cv::Mat& flow_y);

  // Use if no flow is present at current frame.
  FlowExtractor(int flow_bins);

  FlowDescriptor* CreateDescriptor() const { return new FlowDescriptor(flow_bins_); }
  const cv::Mat& FlowX() const { return flow_x_; }
  const cv::Mat& FlowY() const { return flow_y_; }
  bool HasValidFlow() const { return valid_flow_; }

 private:
  int flow_bins_;
  bool valid_flow_;
  const cv::Mat flow_x_;
  const cv::Mat flow_y_;
};

// TODO(grundman): Fix texture and match descriptor.
/*

class MatchDescriptor : public RegionDescriptor {
 public:
  MatchDescriptor();
  void Initialize(int my_id);
  void AddMatch(int match_id, float strength);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;

  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;

  virtual void OutputToAggregated(AggregatedDescriptor* agg_desc) const;
 private:
  int my_id_;

  // List of sorted (match_id, strength) tuples.
  struct MatchTuple {
    int match_id;
    float strength;
    bool operator<(const MatchTuple& rhs) const {
      return match_id < rhs.match_id;
    }
  };

  vector<MatchTuple> matches_;
  DECLARE_REGION_DESCRIPTOR(MatchDescriptor, MATCH_DESCRIPTOR);
};

class LBPDescriptor : public RegionDescriptor {
 public:
  LBPDescriptor();
  void Initialize(int frame_width, int frame_height);

  void AddSamples(const SegmentationDesc_Rasterization& raster,
                  const vector<shared_ptr<IplImage> >& lab_inputs);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;

  virtual void PopulatingDescriptorFinished();
  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;

  virtual void OutputToAggregated(AggregatedDescriptor* agg_desc) const;
private:
  void AddLBP(const uchar* lab_ptr, int sample_id, int width_step);
  static int MapLBP(int lbp) {
    if (lbp_lut_.size() == 0) ComputeLBP_LUT(8); return lbp_lut_[lbp]; }

  void static ComputeLBP_LUT(int bits);
 private:
  int frame_width_;
  int frame_height_;

  // Maps 8 bit lbp to rotation invariant numbers from 0 .. 9.
  static vector<int> lbp_lut_;

  vector<shared_ptr<VectorHistogram> > lbp_histograms_;
  vector<shared_ptr<VectorHistogram> > var_histograms_;

  DECLARE_REGION_DESCRIPTOR(LBPDescriptor, LBP_DESCRIPTOR);
};
   */

}  // namespace Segment.

#endif  // REGION_DESCRIPTOR_H__

