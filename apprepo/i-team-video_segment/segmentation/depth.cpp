#include "depth.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "assert_log.h"
#include "common.h"

namespace VideoFramework {

DEFINE_DATA_FRAME(DepthFrame);

DepthFrame::DepthFrame(int width, int height) : DataFrame(width * height * sizeof(float)),
                                                width_(width),
                                                height_(height) {
}

void DepthFrame::MatView(cv::Mat* view) const {
  ASSURE_LOG(view) << "view not initialized.";
  *view = cv::Mat(height_, width_, CV_32F, (void*)data_, width_step());
}

DepthVideoConverterUnit::DepthVideoConverterUnit(bool purge_video,
                                                 int max_range,
                                                 const string& depth_video_stream_name,
                                                 const string& depth_stream_name)
    : purge_video_(purge_video),
      max_range_(max_range),
      depth_video_stream_name_(depth_video_stream_name),
      depth_stream_name_(depth_stream_name) {
  ASSURE_LOG(max_range_ > 0);
}

bool DepthVideoConverterUnit::OpenStreams(StreamSet* set) {
  // Find video stream idx.
  depth_video_stream_idx_ = FindStreamIdx(depth_video_stream_name_, set);
  ASSURE_LOG(depth_video_stream_idx_ >= 0) << "Could not find depth video stream!\n";

  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
      set->at(depth_video_stream_idx_).get());
  ASSURE_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();
  pixel_format_ = vid_stream->pixel_format();

  if (pixel_format_ != PIXEL_FORMAT_RGB24 &&
      pixel_format_ != PIXEL_FORMAT_BGR24) {
    LOG(FATAL) << "Unsupported input pixel format!\n";
    return false;
  }

  DataStream* depth_stream = new DataStream(depth_stream_name_);
  set->push_back(shared_ptr<DataStream>(depth_stream));
  return true;
}

void DepthVideoConverterUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  const VideoFrame* frame =
      dynamic_cast<const VideoFrame*>(input->at(depth_video_stream_idx_).get());
  ASSERT_LOG(frame);

  cv::Mat image;
  frame->MatView(&image);

  shared_ptr<DepthFrame> depth_frame(new DepthFrame(frame_width_, frame_height_));
  cv::Mat depth;
  depth_frame->MatView(&depth);

  const float denom = 1.0f / max_range_;
  if (pixel_format_ == PIXEL_FORMAT_RGB24) {
    for (int i = 0; i < frame_height_; ++i) {
      uchar* image_ptr = image.ptr<uchar>(i);
      float* depth_ptr = depth.ptr<float>(i);
      for (int j = 0; j < frame_width_; ++j, image_ptr += 3, ++depth_ptr) {
        int depth = (int)image_ptr[0] | (int)image_ptr[1] << 8 | (int)image_ptr[2] << 16;
        ASSERT_LOG(depth < max_range_);
        *depth_ptr = (float)depth * denom;
      }
    }
  } else if (pixel_format_ == PIXEL_FORMAT_BGR24) {
    for (int i = 0; i < frame_height_; ++i) {
      uchar* image_ptr = image.ptr<uchar>(i);
      float* depth_ptr = depth.ptr<float>(i);
      for (int j = 0; j < frame_width_; ++j, image_ptr += 3, ++depth_ptr) {
        int depth = (int)image_ptr[2] | (int)image_ptr[1] << 8 | (int)image_ptr[0] << 16;
        ASSERT_LOG(depth < max_range_);
        *depth_ptr = (float)depth * denom;
      }
    }
  } else {
    LOG(FATAL) << "Unkown pixel format.";
  }

  if (purge_video_) {
    input->at(depth_video_stream_idx_).reset();
  }

  input->push_back(depth_frame);
  output->push_back(input);
}

void DepthVideoConverterUnit::PostProcess(list<FrameSetPtr>* append) {

}

}  // namespace VideoFramework.

namespace Segment {

DenseDepthSegmentUnit::DenseDepthSegmentUnit(const string& dense_segment_options,
                                             const string& depth_stream_name)
    : DenseSegmentUnit(dense_segment_options),
      depth_stream_name_(depth_stream_name) {
}

bool DenseDepthSegmentUnit::OpenFeatureStreams(StreamSet* set) {
  depth_stream_idx_ = FindStreamIdx(depth_stream_name_, set);
  if (depth_stream_idx_ < 0) {
    LOG(ERROR) << "Could not locate depth stream.";
    return false;
  }

  tmp_scaled_frame_.reset(new cv::Mat(frame_height(), frame_width(), CV_32FC3));
  return true;
}

void DenseDepthSegmentUnit::ExtractFrameSetFeatures(FrameSetPtr input, int buffer_size) {
  // Buffer smoothed video frame and depth frames.
  const VideoFrame* video_frame = dynamic_cast<const VideoFrame*>(
      input->at(video_stream_idx()).get());
  ASSURE_LOG(video_frame);

  cv::Mat image_view;
  video_frame->MatView(&image_view);
  image_view.convertTo(*tmp_scaled_frame_, CV_32FC3, 1.0 / 255.0);

  // Smooth image based on our options.
  shared_ptr<cv::Mat> smoothed_frame(new cv::Mat(frame_height(),
                                                 frame_width(),
                                                 CV_32FC3));
  SmoothImage(*tmp_scaled_frame_, smoothed_frame.get());

  // Buffer.
  smoothed_images_.insert(smoothed_images_.begin(), smoothed_frame);

  // Truncate buffer if necessary.
  while (smoothed_images_.size() > buffer_size) {
    smoothed_images_.pop_back();
  }

  // Extract depth frame and store.
  const DepthFrame* depth_frame = dynamic_cast<const DepthFrame*>(
      input->at(depth_stream_idx_).get());
  ASSURE_LOG(depth_frame);

  cv::Mat depth_view;
  depth_frame->MatView(&depth_view);
  depth_images_.insert(depth_images_.begin(),
                       shared_ptr<cv::Mat>(new cv::Mat(depth_view.clone())));

  // Truncate buffer if necessary.
  while (depth_images_.size() > buffer_size) {
    depth_images_.pop_back();
  }

  ASSURE_LOG(depth_images_.size() == smoothed_images_.size());
}

void DenseDepthSegmentUnit::AddFrameToSegmentation(
    int curr_frame,
    const SegmentationDesc* constrained_segmentation,
    Segmentation* seg) {
  ASSURE_LOG(curr_frame < smoothed_images_.size());
  // Combine depth and color linerally.
  SpatialCvMatDistance3L2 color_distance(*smoothed_images_[curr_frame]);
  SpatialDepthDistanceL2 depth_distance(*depth_images_[curr_frame]);
  LinearDistanceAggregator2 linear_aggregator(0.5, 0.5);
  AggregatedDistance<SpatialCvMatDistance3L2,
                     SpatialDepthDistanceL2,
                     LinearDistanceAggregator2> spatial_distance(color_distance,
                                                                 depth_distance,
                                                                 linear_aggregator);
  if (constrained_segmentation) {
    seg->AddGenericImageConstrained(*constrained_segmentation, &spatial_distance);
  } else {
    seg->AddGenericImage(&spatial_distance);
  }
}

void DenseDepthSegmentUnit::ConnectSegmentationTemporally(int curr_frame,
                                                          int prev_frame,
                                                          const cv::Mat* flow_x,
                                                          const cv::Mat* flow_y,
                                                          Segmentation* seg) {
  TemporalCvMatDistance3L2 temporal_distance(
      *smoothed_images_[curr_frame],
      *smoothed_images_[prev_frame]);

  if (flow_x) {
    ASSERT_LOG(flow_y) << "Both flow fields need to be present";
    seg->ConnectTemporallyAlongFlow(*flow_x,
                                    *flow_y,
                                    &temporal_distance);
  } else {
    seg->ConnectTemporally(&temporal_distance);
  }
}

RegionDepthSegmentUnit::RegionDepthSegmentUnit(const string& region_segment_options,
                                               const string& depth_stream_name)
    : RegionSegmentUnit(region_segment_options),
      depth_stream_name_(depth_stream_name) {
}

bool RegionDepthSegmentUnit::OpenFeatureStreams(StreamSet* set) {
  depth_stream_idx_ = FindStreamIdx(depth_stream_name_, set);
  if (depth_stream_idx_ < 0) {
    LOG(ERROR) << "Could not locate depth stream.";
    return false;
  }

  return true;
}

void RegionDepthSegmentUnit::GetDescriptorExtractorList(
    FrameSetPtr input,
    DescriptorExtractorList* extractors) {
  // Add appearance and flow, by calling base
  RegionSegmentUnit::GetDescriptorExtractorList(input, extractors);

  // Add depth.
  const DepthFrame* depth_frame = dynamic_cast<const DepthFrame*>(
      input->at(depth_stream_idx_).get());
  ASSURE_LOG(depth_frame);

  cv::Mat depth_view;
  depth_frame->MatView(&depth_view);
  extractors->push_back(shared_ptr<DepthExtractor>(new DepthExtractor(depth_view)));
}

RegionDistance* RegionDepthSegmentUnit::GetRegionDistance() {
  if (flow_stream_idx() < 0) {
    return new SquaredORDistance<2>;
  } else {
    return new SquaredORDistance<3>;
  }
}

DepthDescriptor::DepthDescriptor(int depth_bins) : depth_bins_(depth_bins),
                                                is_populated_(false) {
}

void DepthDescriptor::AddFeatures(const SegmentationDesc_Rasterization& raster,
                                  const RegionDescriptorExtractor& extractor,
                                  int frame_num) {
  const DepthExtractor& depth_extractor =
      dynamic_cast<const DepthExtractor&>(extractor);

  if (frame_num >= depth_histograms_.size()) {
    depth_histograms_.resize(frame_num + 1);
  }

  if (depth_histograms_[frame_num] == NULL) {
    depth_histograms_[frame_num].reset(new VectorHistogram(100));
  }

  for (RepeatedPtrField<ScanInterval>::const_iterator scan_inter =
           raster.scan_inter().begin();
       scan_inter != raster.scan_inter().end();
       ++scan_inter) {
    const float* depth_ptr =
        depth_extractor.DepthFrame().ptr<float>(scan_inter->y());
    for (int x = scan_inter->left_x(); x <= scan_inter->right_x(); ++x) {
      const float depth = depth_ptr[x];
      if (depth > 0) {
        depth_histograms_[frame_num]->IncrementBinInterpolated(depth * 100);
      }
    }
  }
}

float DepthDescriptor::RegionDistance(const RegionDescriptor& rhs_uncast) const {
  const DepthDescriptor& rhs = dynamic_cast<const DepthDescriptor&>(rhs_uncast);

  int histogram_max_idx = std::min(depth_histograms_.size(), rhs.depth_histograms_.size());
  float sum = 0;
  float total = 0;
  for (int i = 0; i < histogram_max_idx; ++i) {
    if (depth_histograms_[i] != NULL && rhs.depth_histograms_[i] != NULL) {
      float weight = std::min(depth_histograms_[i]->NumVectors(),
                              rhs.depth_histograms_[i]->NumVectors());
      sum += depth_histograms_[i]->ChiSquareDist(*rhs.depth_histograms_[i])*weight;
      total += weight;
    }
  }
  if (total > 0) {
    const float result = sum / total;
    return result;
  } else {
    return 0;
  }
}

void DepthDescriptor::PopulatingDescriptorFinished() {
  if (!is_populated_) {
    for(vector< shared_ptr<VectorHistogram> >::iterator hist_iter =
            depth_histograms_.begin();
        hist_iter != depth_histograms_.end();
        ++hist_iter) {
      if (*hist_iter != NULL) {
        (*hist_iter)->NormalizeToOne();
      }
    }
    is_populated_ = true;
  }
}

void DepthDescriptor::MergeWithDescriptor(const RegionDescriptor& rhs_uncast) {
  const DepthDescriptor& rhs = dynamic_cast<const DepthDescriptor&>(rhs_uncast);

  depth_histograms_.resize(std::max(depth_histograms_.size(),
                                   rhs.depth_histograms_.size()));

  for (size_t i = 0; i < rhs.depth_histograms_.size(); ++i) {
    if (rhs.depth_histograms_[i]) {
      if (depth_histograms_[i]) {
        depth_histograms_[i]->MergeWithHistogram(*rhs.depth_histograms_[i]);
      } else {
        depth_histograms_[i].reset(new VectorHistogram(*rhs.depth_histograms_[i]));
      }
    }
  }
}

RegionDescriptor* DepthDescriptor::Clone() const {
  DepthDescriptor* clone = new DepthDescriptor(100);
  clone->depth_histograms_.resize(depth_histograms_.size());

  for (vector<shared_ptr<VectorHistogram> >::const_iterator i = depth_histograms_.begin();
       i != depth_histograms_.end();
       ++i) {
    if (*i) {
      clone->depth_histograms_[i - depth_histograms_.begin()].reset(
          new VectorHistogram(**i));
    }
  }

  return clone;
}

}  // namespace Segment.

