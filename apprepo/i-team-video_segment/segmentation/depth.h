#ifndef DEPTH_H__
#define DEPTH_H__

#include "pixel_distance.h"
#include "region_descriptor.h"
#include "segmentation_unit.h"
#include "video_unit.h"

namespace cv {
  class Mat;
}

namespace VideoFramework {

// Frame containing depth stream as normalized float image.
class DepthFrame : public DataFrame {
public:
  DepthFrame(int width, int height);

  int width() const { return width_; }
  int width_step() const { return width_ * sizeof(float); }
  int height() const { return height_; }

  void MatView(cv::Mat* view) const;

private:
  DepthFrame() : width_(0), height_(0) { }

  int width_;
  int height_;

  DECLARE_DATA_FRAME(DepthFrame);
};

// Reads 3 channel RGB frames, containing by default 13 bit Kinect Depth and converts to
// normalized float frames. Optionally input video frames can be discarded (reset to zero
// pointers).
class DepthVideoConverterUnit : public VideoUnit {
 public:
   DepthVideoConverterUnit(bool purge_video,
                           int max_range = 1 << 14,
                           const string& depth_video_stream_name = "DepthVideoStream",
                           const string& depth_stream_name = "DepthStream");
  virtual ~DepthVideoConverterUnit() {}

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

 private:
  bool purge_video_;
  int max_range_;
  string depth_video_stream_name_;
  string depth_stream_name_;

  int depth_video_stream_idx_;
  PixelFormat pixel_format_;

  int frame_width_;
  int frame_height_;
};

}  // namespace VideoFramework.

namespace Segment {

// Specialized class with depth segmentation support.
class DenseDepthSegmentUnit : public DenseSegmentUnit {
 public:
  DenseDepthSegmentUnit(const string& dense_segment_options,
                        const string& depth_stream_name = "DepthStream");

  virtual bool OpenFeatureStreams(StreamSet* set);
  virtual void ExtractFrameSetFeatures(FrameSetPtr input, int buffer_size);

  virtual void AddFrameToSegmentation(int curr_frame,
                                      const SegmentationDesc* constained_segmentation,
                                      Segmentation* seg);

  virtual void ConnectSegmentationTemporally(int curr_frame,
                                             int prev_frame,
                                             const cv::Mat* flow_x,
                                             const cv::Mat* flow_y,
                                             Segmentation* seg);
 private:
  string depth_stream_name_;
  int depth_stream_idx_;

  // Buffer feature images.
  vector<shared_ptr<cv::Mat> > smoothed_images_;
  vector<shared_ptr<cv::Mat> > depth_images_;

  shared_ptr<cv::Mat> tmp_scaled_frame_;
};

class RegionDepthSegmentUnit : public RegionSegmentUnit {
 public:
  RegionDepthSegmentUnit(const string& region_segment_options,
                         const string& depth_stream_name = "DepthStream");

  virtual bool OpenFeatureStreams(StreamSet* set);
  virtual void GetDescriptorExtractorList(FrameSetPtr input,
                                          DescriptorExtractorList* extractors);

  virtual RegionDistance* GetRegionDistance();
 private:
   string depth_stream_name_;
   int depth_stream_idx_;
};

struct DepthDiffL1 : public PixelDistanceFunction<float, 1> {
  float operator()(const float* ptr_1, const float* ptr_2) {
    if (ptr_1[0] == 0 || ptr_2[0] == 0) {
      return 0.01;  // Return small eps if distance is not defined.
    } else {
      return fabs(ptr_1[0] - ptr_2[0]);
    }
  }
};

struct DepthDiffL2 : public PixelDistanceFunction<float, 1> {
  float operator()(const float* ptr_1, const float* ptr_2) {
    if(ptr_1[0] == 0 || ptr_2[0] == 0) {
      return 0.01;  // Return small eps if distance is not defined.
    } else {
      const float diff = ptr_1[0] - ptr_2[0];
      return diff * diff;
    }
  }
};

typedef SpatialCvMatDistance<DepthDiffL1> SpatialDepthDistanceL1;
typedef SpatialCvMatDistance<DepthDiffL2> SpatialDepthDistanceL2;

class DepthExtractor;

// Maintains average of region per frame.
class DepthDescriptor : public RegionDescriptor {
 public:
  DepthDescriptor(int depth_bins);
  DepthDescriptor() { }

  virtual void AddFeatures(const SegmentationDesc_Rasterization& raster,
                           const RegionDescriptorExtractor& extractor,
                           int frame_num);

  virtual float RegionDistance(const RegionDescriptor& rhs) const;
  virtual void PopulatingDescriptorFinished();
  virtual void MergeWithDescriptor(const RegionDescriptor& rhs);
  virtual RegionDescriptor* Clone() const;
 protected:
  vector< shared_ptr<VectorHistogram> > depth_histograms_;
  int depth_bins_;
  bool is_populated_;
};

class DepthExtractor : public RegionDescriptorExtractor {
 public:
  DepthExtractor(const cv::Mat& depth_frame) : depth_frame_(depth_frame) { }
  RegionDescriptor* CreateDescriptor() const { return new DepthDescriptor(depth_bins_); }
  const cv::Mat& DepthFrame() const { return depth_frame_; }
 private:
  const cv::Mat depth_frame_;
  int depth_bins_;
};

}  // namespace Segment.

#endif  // DEPTH_H__
