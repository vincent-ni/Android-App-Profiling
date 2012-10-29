/*
 *  segmentation_unit.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 10/30/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "segmentation_unit.h"

#include <boost/random/uniform_int.hpp>
#include <boost/random/additive_combine.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/lexical_cast.hpp>
#include <google/protobuf/repeated_field.h>
#include <iostream>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core_c.h>
#include <opencv2/core/core.hpp>

#include "assert_log.h"
#include "flow_reader.h"
#include "image_filter.h"
#include "image_util.h"
#include "pixel_distance.h"
#include "segmentation_render.h"

using namespace ImageFilter;

namespace Segment {
  typedef unsigned char uchar;

  DenseSegmentUnit::DenseSegmentUnit(const string& options) {
    // Basic setup.
    seg_options_.ParseFromString(options);
    SetRateBufferSize(200);
  }

  DenseSegmentUnit::~DenseSegmentUnit() {

  }

  bool DenseSegmentUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(seg_options_.video_stream_name(), set);

    if (video_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find video stream!\n";
      return false;
    }

    // Get video stream info.
    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
        set->at(video_stream_idx_).get());

    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    if (vid_stream->pixel_format() != PIXEL_FORMAT_BGR24) {
      LOG(ERROR) << "Expecting video format to be BGR24.\n";
      return false;
    }

    // Flow stream present?
    if (!seg_options_.flow_stream_name().empty()) {
      flow_stream_idx_ = FindStreamIdx(seg_options_.flow_stream_name(), set);
      if (flow_stream_idx_ < 0) {
        LOG(ERROR) << "Flow stream specified but not present";
        return false;
      }
    } else {
      flow_stream_idx_ = -1;
    }

    // Add segmentation stream.
    SegmentationStream* seg_stream =
        new SegmentationStream(frame_width_,
                               frame_height_,
                               seg_options_.segment_stream_name());

    set->push_back(shared_ptr<DataStream>(seg_stream));

    chunk_size_ = seg_options_.chunk_size();
    ASSURE_LOG(chunk_size_ > 0) << "Specify positive chunk_size > 1";

    if (seg_options_.max_chunk_memory_consumption() > 0) {
      const int total_mem = seg_options_.max_chunk_memory_consumption() * 1024;
      const int num_nodes = frame_width_ * frame_height_ *
                            (1 + seg_options_.chunk_overlap_ratio()) / 1024;
      // Approx. 50% of the edges have to be buffered
      // temporary in MergeSmallRegionsRemoveInnerEdges.
      const int size_of_node = 26 / 2 * 1.5 * DenseSegmentationGraph::SizeOfEdge() +
                               DenseSegmentationGraph::SizeOfRegion();

      int max_chunk_size = total_mem / (num_nodes * size_of_node);
      chunk_size_ = std::min(chunk_size_, max_chunk_size);
    }

    // At least 2 frames per chunk.
    chunk_size_ = std::max(chunk_size_, 2);
    overlap_frames_ = seg_options_.chunk_overlap_ratio() * (float)chunk_size_;
    // At least 1 frames overlap.
    overlap_frames_ = std::max(overlap_frames_, 1);
    // At most 10 frames overlap.
    overlap_frames_ = std::min(overlap_frames_, 10);
    seg_options_.set_chunk_size(chunk_size_);

    constraint_frames_ = seg_options_.num_constraint_frames();

    LOG(INFO_V1) << "Performing oversegmentation in chunks of " << chunk_size_
                 << " frames with " << overlap_frames_ << " frames overlap.";

    // Compute resolution dependent min region size.
    min_region_size_ = seg_options_.frac_min_region_size() * frame_width_ *
                       seg_options_.frac_min_region_size() * frame_height_ *
                       chunk_size_;

    // Allocate temporary images.
    smoothing_flow_x_ = cvCreateMatShared(frame_height_, frame_width_, CV_32FC1);
    smoothing_flow_y_ = cvCreateMatShared(frame_height_, frame_width_, CV_32FC1);

    scaled_image_.reset(new cv::Mat(frame_height_, frame_width_, CV_32FC3));

    seg_.reset(new Segmentation(seg_options_.parameter_k(),
                                frame_width_,
                                frame_height_,
                                0));   // chunk_id.

    seg_->OverSegmentationSizeHint(chunk_size_);

    processed_chunks_ = 0;
    input_frames_ = 0;
    max_region_id_ = 0;
    dense_segmentation_filter_finished_ = false;

    return OpenFeatureStreams(set);
  }

  bool DenseSegmentUnit::OpenFeatureStreams(StreamSet* set) {
    return true;
  }

  void DenseSegmentUnit::ExtractFrameSetFeatures(FrameSetPtr input,
                                                 int buffer_size) {
    // Get IplView and pass to Segmentation object.
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
        input->at(video_stream_idx_).get());

    ASSURE_LOG(frame);
    cv::Mat image_view;
    frame->MatView(&image_view);
    image_view.convertTo(*scaled_image_, CV_32FC3, 1.0 / 255.0);

    // Smooth image based on our options.
    shared_ptr<cv::Mat> smooth_frame(new cv::Mat(frame_height_, frame_width_, CV_32FC3));
    SmoothImage(*scaled_image_, smooth_frame.get());

    // Buffer.
    buffered_feature_images_.insert(buffered_feature_images_.begin(), smooth_frame);

    // Truncate buffer if necessary.
    while (buffered_feature_images_.size() > buffer_size) {
      buffered_feature_images_.pop_back();
    }
  }

  void DenseSegmentUnit::AddFrameToSegmentation(
      int curr_frame,
      const SegmentationDesc* constrained_segmentation,
      Segmentation* seg) {
    ASSURE_LOG(curr_frame < buffered_feature_images_.size());
    if (seg_options_.color_distance() == DenseSegmentOptions::COLOR_DISTANCE_L2) {
      SpatialCvMatDistance3L2 spatial_distance(*buffered_feature_images_[curr_frame]);
      if (constrained_segmentation) {
        seg->AddGenericImageConstrained(*constrained_segmentation, &spatial_distance);
      } else {
        seg->AddGenericImage(&spatial_distance);
      }
    } else {
      SpatialCvMatDistance3L1 spatial_distance(*buffered_feature_images_[curr_frame]);
      if (constrained_segmentation) {
        seg->AddGenericImageConstrained(*constrained_segmentation, &spatial_distance);
      } else {
        seg->AddGenericImage(&spatial_distance);
      }
    }
  }

  void DenseSegmentUnit::ConnectSegmentationTemporally(int curr_frame,
                                                       int prev_frame,
                                                       const cv::Mat* flow_x,
                                                       const cv::Mat* flow_y,
                                                       Segmentation* seg) {
    if (seg_options_.color_distance() == DenseSegmentOptions::COLOR_DISTANCE_L2) {
      TemporalCvMatDistance3L2 temporal_distance(
          *buffered_feature_images_[curr_frame],
          *buffered_feature_images_[prev_frame]);
      if (flow_x) {
        ASSERT_LOG(flow_y) << "Both flow fields need to be present";
        seg->ConnectTemporallyAlongFlow(*flow_x,
                                        *flow_y,
                                        &temporal_distance);
      } else {
        seg->ConnectTemporally(&temporal_distance);
      }
    } else {
       TemporalCvMatDistance3L1 temporal_distance(
          *buffered_feature_images_[curr_frame],
          *buffered_feature_images_[prev_frame]);
      if (flow_x) {
        ASSERT_LOG(flow_y) << "Both flow fields need to be present";
        seg->ConnectTemporallyAlongFlow(*flow_x,
                                        *flow_y,
                                        &temporal_distance);
      } else {
        seg->ConnectTemporally(&temporal_distance);
      }
    }
  }

  void DenseSegmentUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    // Previously reached end of chunk?
    if (input_frames_ > 0 && input_frames_ % chunk_size_ == 0) {
      int overlap_start = chunk_size_;
      int actual_chunk_size = chunk_size_ + overlap_frames_;

      // No left constraints for first chunk.
      if (processed_chunks_ == 0) {
        overlap_start = chunk_size_ - overlap_frames_;
        actual_chunk_size = chunk_size_;
      }

      SegmentAndOutputChunk(actual_chunk_size, overlap_start, output);

      // Initialize new Segmentation.
      LOG(INFO_V1) << "Creating new segmentation object at "
                   << input_frames_ << " frames \n";

      seg_.reset(new Segmentation(seg_options_.parameter_k(),
                                  frame_width_,
                                  frame_height_,
                                  processed_chunks_));
      seg_->OverSegmentationSizeHint(chunk_size_ + overlap_frames_);

      ASSURE_LOG(overlap_segmentations_.size() == constraint_frames_);

      // Add overlap images with constraints.
      for (int i = 0; i < overlap_frames_; ++i) {
        const int buffer_idx = overlap_frames_ - 1 - i;
        // Add with constraints.
        if (i < constraint_frames_) {
          AddFrameToSegmentation(buffer_idx, overlap_segmentations_[i].get(), seg_.get());
        } else {
          AddFrameToSegmentation(buffer_idx, NULL, seg_.get());
        }

        // Connect in time.
        if (i > 0) {
          if (flow_stream_idx_ >= 0) {
            cv::Mat flow_x(overlap_flow_x_[i].get());
            cv::Mat flow_y(overlap_flow_y_[i].get());
            ConnectSegmentationTemporally(buffer_idx,
                                          buffer_idx + 1,
                                          &flow_x,
                                          &flow_y,
                                          seg_.get());
          } else {
            ConnectSegmentationTemporally(buffer_idx,
                                          buffer_idx + 1,
                                          NULL,
                                          NULL,
                                          seg_.get());
          }
        }
      }
      // Reset overlap information.
      overlap_segmentations_.clear();
      overlap_flow_x_.clear();
      overlap_flow_y_.clear();
    }  // end new chunk.

    LOG(INFO_V1) << "Processing frame #" << input_frames_;

    // By default buffer only two frames.
    int buffer_length = 2;

    // If in prev. frame in overlap buffer all frames until end of overlap is reached.
    if (input_frames_ % chunk_size_ >= chunk_size_ - overlap_frames_) {
      buffer_length = std::max(overlap_frames_, 2);  // Keep at least 2 frames around.
    }

    ExtractFrameSetFeatures(input, buffer_length);
    AddFrameToSegmentation(0, NULL, seg_.get());

    IplImage optical_flow_x, optical_flow_y;
    if (input_frames_ > 0) {
      if (flow_stream_idx_ >=0) {
        const DenseFlowFrame* flow_frame = dynamic_cast<const DenseFlowFrame*>(
            input->at(flow_stream_idx_).get());
        ASSERT_LOG(flow_frame);

        flow_frame->ImageView(&optical_flow_x, true);
        flow_frame->ImageView(&optical_flow_y, false);

        cv::Mat flow_x(&optical_flow_x);
        cv::Mat flow_y(&optical_flow_y);
        ConnectSegmentationTemporally(0, 1, &flow_x, &flow_y, seg_.get());
      } else {
        // Add without flow.
        ConnectSegmentationTemporally(0, 1, NULL, NULL, seg_.get());
      }
    }

    // Within new overlap region -> buffer frames.
    // TODO: Seems redundant for flow (present in frame_set_buffer_ ....)
    if (input_frames_ % chunk_size_ >= chunk_size_ - overlap_frames_) {
      LOG(INFO_V2) << "Saving overlap frames @ frame idx " << input_frames_;

      // Buffer optical flow as well?
      if (flow_stream_idx_ >=0) {
        overlap_flow_x_.push_back(cvCloneImageShared(&optical_flow_x));
        overlap_flow_y_.push_back(cvCloneImageShared(&optical_flow_y));
      }
    }

    // Advance frame number.
    ++input_frames_;
    frame_set_buffer_.push_back(input);
  }

  void DenseSegmentUnit::SegmentAndOutputChunk(int sz,
                                               int overlap_start,
                                               list<FrameSetPtr>* append) {
    LOG(INFO_V1) << "Segmenting chunk of size " << sz << ", overlap starts at "
                 << overlap_start;

    seg_->RunOverSegmentation(min_region_size_,
                              seg_options_.two_stage_oversegment(),
                              seg_options_.thin_structure_suppression(),
                              seg_options_.enforce_n4_connectivity(),
                              seg_options_.spatial_cleanup_step());

    const int last_frame = overlap_start + constraint_frames_;
    seg_->ConstrainSegmentationToFrameInterval(0, last_frame - 1);
    seg_->AdjustRegionAreaToFrameInterval(0, overlap_start - 1);

    vector<int> new_max_region_id(1);
    seg_->AssignUniqueRegionIds(processed_chunks_ > 0,
                                vector<int>(1, max_region_id_),
                                &new_max_region_id);
    max_region_id_ = new_max_region_id[0];

    for (int frame_idx = 0; frame_idx < last_frame; ++frame_idx) {
      // Retrieve segmentations and output.
      shared_ptr<SegmentationDesc> desc(new SegmentationDesc());
      seg_->RetrieveSegmentation3D(frame_idx,
                                   false,       // no descriptors saved.
                                   desc.get());

      desc->set_chunk_size(last_frame);
      desc->set_overlap_start(overlap_start);

      // Output to FrameSet.
      if (frame_idx < overlap_start) {
        DataFrame* seg_frame = new DataFrame(desc->ByteSize());
        desc->SerializeToArray(seg_frame->mutable_data(), seg_frame->size());

        // Fetch prev. FrameSet from buffer.
        FrameSetPtr output = frame_set_buffer_.front();
        frame_set_buffer_.pop_front();
        output->push_back(shared_ptr<DataFrame>(seg_frame));

        append->push_back(output);
      } else {
        // Save this one for later.
       overlap_segmentations_.push_back(desc);
      }
    }

    ++processed_chunks_;
  }

  void DenseSegmentUnit::PostProcess(list<FrameSetPtr>* append) {
    // Process last chunk of frames.
    if (!dense_segmentation_filter_finished_ && input_frames_ > 0) {
      int remaining_frames = input_frames_ % chunk_size_;
      if (remaining_frames == 0) {
        remaining_frames = chunk_size_;     // One complete remaining chunk possible.
      }

      int actual_chunk_size = remaining_frames + overlap_frames_;

      // No left constraints for first chunk.
      if (processed_chunks_ == 0) {
        actual_chunk_size = remaining_frames;
      }

      SegmentAndOutputChunk(actual_chunk_size, actual_chunk_size, append);
      dense_segmentation_filter_finished_ = true;
    }
  }

  void DenseSegmentUnit::SmoothImage(const cv::Mat& src, cv::Mat* dst) const {
    const IplImage ipl_src = src;
    IplImage ipl_dst = *dst;
    switch(seg_options_.presmoothing()) {
      case DenseSegmentOptions::PRESMOOTH_GAUSSIAN:
        cv::GaussianBlur(src, *dst, cv::Size(3, 3), 1.5);
        break;
      case DenseSegmentOptions::PRESMOOTH_BILATERAL:
        ImageFilter::BilateralFilter(&ipl_src, 1.5, 0.15, &ipl_dst);
        break;
      case DenseSegmentOptions::PRESMOOTH_FLOW_BILATERAL:
        ImageFilter::EdgeTangentFlow(&ipl_src, 3, 3,
                                     smoothing_flow_x_.get(),
                                     smoothing_flow_y_.get(),
                                     smoothing_step_.get());

        ImageFilter::BilateralFlowFilter(&ipl_src,
                                         smoothing_flow_x_.get(),
                                         smoothing_flow_y_.get(),
                                         3,                        // sigma space tangent
                                         0.7,                      // sigma space normal
                                         0.2,                      // sigma color tangent
                                         0.05,                     // sigma color normal
                                         3,                        // iterations
                                         &ipl_dst);
        break;
    }
  }

  RegionSegmentUnit::RegionSegmentUnit(const string& options) {
    // Basic setup.
    seg_options_.ParseFromString(options);
    SetRateBufferSize(300);
  }

  RegionSegmentUnit::~RegionSegmentUnit() {

  }

  bool RegionSegmentUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(seg_options_.video_stream_name(), set);

    if (video_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find video stream!\n";
      return false;
    }

    // Get video stream info.
    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
        set->at(video_stream_idx_).get());

    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    if (vid_stream->pixel_format() != PIXEL_FORMAT_BGR24) {
      LOG(ERROR) << "Expecting video format to be BGR24.\n";
      return false;
    }

    // Flow stream present?
    if (!seg_options_.flow_stream_name().empty()) {
      flow_stream_idx_ = FindStreamIdx(seg_options_.flow_stream_name(), set);
      if (flow_stream_idx_ < 0) {
        LOG(ERROR) << "Flow stream specified but not present";
        return false;
      }
    } else {
      flow_stream_idx_ = -1;
    }

    // Get segmentation stream.
    seg_stream_idx_ = FindStreamIdx(seg_options_.segment_stream_name(), set);

    if (seg_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find Segmentation stream!\n";
      return false;
    }

    seg_.reset(new Segmentation(0,
                                frame_width_,
                                frame_height_,
                                0));   // chunk_id.

    // Allocate temporary images.
    lab_input_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 3);

    region_segmentation_filter_finished_ = false;
    input_frames_ = 0;
    processed_chunk_sets_ = 0;
    read_chunks_ = -1;
    num_output_frames_ = 0;
    last_overlap_start_ = -1;
    last_lookahead_start_ = -1;

    chunk_set_size_ = seg_options_.chunk_set_size();
    ASSURE_LOG(chunk_set_size_ > 1)
        << "At least to chunks per chunk_set required.";

    chunk_set_overlap_ = seg_options_.chunk_set_overlap();
    ASSURE_LOG(chunk_set_overlap_ > 0)
        << "At least one chunk in overlap expected.";

    ASSURE_LOG(chunk_set_overlap_ < chunk_set_size_)
        << "Overlap has to be strictly smaller than a chunk set.";

    constraint_chunks_ = seg_options_.constraint_chunks();
    ASSURE_LOG(constraint_chunks_ <= chunk_set_overlap_)
        << "Constraints must be smaller or equal to overlap";

    return OpenFeatureStreams(set);
  }

  void RegionSegmentUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    // Retrieve Segmentation.
    const DataFrame* seg_frame = input->at(seg_stream_idx_).get();
    SegmentationDesc desc;

    desc.ParseFromArray(seg_frame->data(), seg_frame->size());

    LOG(INFO_V1) << "Processing frame #" << input_frames_;

    bool initialize_base_level_hierarchy = false;
    if (desc.hierarchy_size() > 0) {
      ++read_chunks_;
      initialize_base_level_hierarchy = true;
    }

    // Get DescriptorExtractorList.
    DescriptorExtractorList extractor_list;
    GetDescriptorExtractorList(input, &extractor_list);

    // Is this the first frame in a new chunk set? -> Segment last chunk_set
    if (read_chunks_ > 0 &&
        read_chunks_ % chunk_set_size_ == 0 &&
        initialize_base_level_hierarchy) {
      int look_ahead = last_lookahead_start_ > 0 ? last_lookahead_start_
                                                 : seg_->NumFramesAdded();
      SegmentAndOutputChunk(last_overlap_start_, look_ahead, output);
      last_overlap_start_ = -1;
      last_lookahead_start_ = -1;

      seg_.swap(new_seg_);
      new_seg_.reset();
    }

    // Is this an overlap chunk?
    if (read_chunks_ % chunk_set_size_ >= chunk_set_size_ - chunk_set_overlap_) {
      if (new_seg_ == NULL) {
        new_seg_.reset(new Segmentation(0,
                                        frame_width_,
                                        frame_height_,
                                        processed_chunk_sets_ + 1));
      }

      if (last_overlap_start_ < 0) {
        last_overlap_start_ = seg_->NumFramesAdded();
      }

      if (initialize_base_level_hierarchy) {
        // Use as constraints?
        RegionMapping mapping;
        RegionMapping* mapping_ptr = NULL;
        if (read_chunks_ % chunk_set_size_ <
            chunk_set_size_ - chunk_set_overlap_ + constraint_chunks_) {
          mapping_ptr = &mapping;
        }

        // Discard overlap in hierarchy on last chunk in set.
        bool last_chunk_set = (read_chunks_ + 1) % chunk_set_size_ == 0;
        if (last_chunk_set) {
          SegmentationDesc::HierarchyLevel constraint_level;
          ConstrainHierarchyToFrameInterval(0,
                                            desc.overlap_start() - 1,
                                            desc.hierarchy(0),
                                            &constraint_level);

          seg_->InitializeBaseHierarchyLevel(constraint_level,
                                             extractor_list,
                                             NULL,
                                             mapping_ptr);
        } else {
          seg_->InitializeBaseHierarchyLevel(desc.hierarchy(0),
                                             extractor_list,
                                             NULL,
                                             mapping_ptr);
        }

        new_seg_->InitializeBaseHierarchyLevel(desc.hierarchy(0),
                                               extractor_list,
                                               mapping_ptr,
                                               NULL);
      }

      seg_->AddOverSegmentation(desc,
                                extractor_list);

      new_seg_->AddOverSegmentation(desc,
                                    extractor_list);
    } else {
      // Not in an overlap.
      if (initialize_base_level_hierarchy) {
        seg_->InitializeBaseHierarchyLevel(desc.hierarchy(0),
                                           extractor_list,
                                           NULL,      // no constraints.
                                           NULL);
      }

      seg_->AddOverSegmentation(desc,
                                extractor_list);
    }

    // Erase segmentation. Will be replaced by hierarchical segmentation.
    input->at(seg_stream_idx_).reset();

    // Free flow or video frames?
    if (seg_options_.free_video_frames()) {
      input->at(video_stream_idx_).reset();
    }

    if (flow_stream_idx_ >= 0 && seg_options_.free_flow_frames()) {
      input->at(flow_stream_idx_).reset();
    }

    // First frame in lookahead region (outside of contraints)? If so, remember.
    if (read_chunks_ % chunk_set_size_ >=
        chunk_set_size_ - chunk_set_overlap_ + constraint_chunks_ &&
        last_lookahead_start_ < 0) {
      last_lookahead_start_ = input_frames_;
    }

    // Advance frame number.
    ++input_frames_;
    frame_set_buffer_.push_back(input);
  }

  void RegionSegmentUnit::PostProcess(list<FrameSetPtr>* append) {
    if (region_segmentation_filter_finished_ == false && input_frames_ > 0) {
      SegmentAndOutputChunk(seg_->NumFramesAdded(), seg_->NumFramesAdded(), append);
      region_segmentation_filter_finished_ = true;
    }
  }

  void RegionSegmentUnit::GetDescriptorExtractorList(
      FrameSetPtr input,
      DescriptorExtractorList* extractors) {
    ASSURE_LOG(extractors);

    // By default we only use appearance and flow features.
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(
        video_stream_idx_).get());
    ASSERT_LOG(frame);
    cv::Mat image_view;
    frame->MatView(&image_view);

    shared_ptr<AppearanceExtractor> appearance_extractor(
        new AppearanceExtractor(seg_options_.luminance_bins(),
                                seg_options_.color_bins(),
                                image_view));
    extractors->push_back(appearance_extractor);

    if (flow_stream_idx_ >=0) {
      if (input_frames_ > 0 ) {
        const DenseFlowFrame* flow_frame = dynamic_cast<const DenseFlowFrame*>(
            input->at(flow_stream_idx_).get());
        IplImage optical_flow_x, optical_flow_y;

        ASSERT_LOG(flow_frame);
        flow_frame->ImageView(&optical_flow_x, true);
        flow_frame->ImageView(&optical_flow_y, false);
        shared_ptr<FlowExtractor> flow_extractor(
            new FlowExtractor(seg_options_.flow_bins(),
                              &optical_flow_x,
                              &optical_flow_y));
        extractors->push_back(flow_extractor);
      } else {
        extractors->push_back(shared_ptr<FlowExtractor>(
            new FlowExtractor(seg_options_.flow_bins())));
      }
    }
  }

  RegionDistance* RegionSegmentUnit::GetRegionDistance() {
    return new SquaredORDistance<2>();
  }

  void RegionSegmentUnit::SegmentAndOutputChunk(int overlap_start,
                                                int lookahead_start,
                                                list<FrameSetPtr>* append) {
    // Compute min region size from added regions so far.
    int min_region_size = seg_options_.frac_min_region_size() * frame_width_ *
                          seg_options_.frac_min_region_size() * frame_height_ *
                          seg_->NumFramesAdded() / seg_->NumBaseHierarchiesAdded();

    // Segment all frames hierarchically and output result.
    shared_ptr<RegionDistance> distance(GetRegionDistance());
    seg_->RunHierarchicalSegmentation(*distance,
                                      seg_options_.level_cutoff_fraction(),
                                      seg_options_.min_region_num(),
                                      seg_options_.max_region_num(),
                                      true);
    int computed_levels = seg_->ComputedHierarchyLevels();

    if (computed_levels > max_region_ids_.size()) {
      max_region_ids_.resize(computed_levels, 0);
    }

    // Chunk set processing cleanup.
    seg_->ConstrainSegmentationToFrameInterval(0, lookahead_start - 1);
    seg_->AdjustRegionAreaToFrameInterval(0, overlap_start - 1);

    vector<int> new_max_region_ids(max_region_ids_.size());
    seg_->AssignUniqueRegionIds(processed_chunk_sets_ > 0,
                                max_region_ids_,
                                &new_max_region_ids);

    max_region_ids_.swap(new_max_region_ids);

    // Pull segmentation result in overlap to new segmentation object.
    if (new_seg_ != NULL) {
      new_seg_->PullCounterpartSegmentationResult(*seg_);
    }

    // Discard bottom hierarchy level here.
    seg_->DiscardBottomLevel();

    LOG(INFO_V1) << "Outputting hierarchical segmentation of size "
                 << overlap_start << " frames.";
    const int hierarchy_frame_idx = num_output_frames_;

    for (int frame_idx = 0; frame_idx < overlap_start; ++frame_idx) {
      SegmentationDesc desc;
      seg_->RetrieveSegmentation3D(frame_idx,
                                   seg_options_.save_descriptors(),
                                   &desc);
      desc.set_hierarchy_frame_idx(hierarchy_frame_idx);

      DataFrame* seg_hier_frame = new DataFrame(desc.ByteSize());
      desc.SerializeToArray(seg_hier_frame->mutable_data(),
                            seg_hier_frame->size());

      // Fetch prev. FrameSet from buffer.
      ASSURE_LOG(frame_set_buffer_.size() > 0);
      FrameSetPtr output = frame_set_buffer_.front();
      frame_set_buffer_.pop_front();
      output->at(seg_stream_idx_).reset(seg_hier_frame);
      append->push_back(output);
      ++num_output_frames_;
    }

    ++processed_chunk_sets_;
  }

  SegmentationWriterUnit::SegmentationWriterUnit(const string& filename,
                                                 bool strip_to_essentials,
                                                 const string& vid_stream_name,
                                                 const string& seg_stream_name,
                                                 int chunk_size)
    : filename_(filename),
      strip_to_essentials_(strip_to_essentials),
      vid_stream_name_(vid_stream_name),
      seg_stream_name_(seg_stream_name),
      writer_(filename),
      chunk_size_(chunk_size) {
  }

  bool SegmentationWriterUnit::OpenStreams(StreamSet* set) {
    vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);

    if (vid_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find Video stream!\n";
      return false;
    }

    // Get segmentation stream.
    seg_stream_idx_ = FindStreamIdx(seg_stream_name_, set);

    if (seg_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find Segmentation stream!\n";
      return false;
    }

    frame_number_ = 0;
    return writer_.OpenFile();
  }

  void SegmentationWriterUnit::ProcessFrame(FrameSetPtr input,
                                            list<FrameSetPtr>* output) {
    // Retrieve Segmentation.
    const DataFrame* seg_frame = input->at(seg_stream_idx_).get();
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
        input->at(vid_stream_idx_).get());
    ASSERT_LOG(frame);

    if (strip_to_essentials_) {
      SegmentationDesc desc;

      // We use a different encoding scheme here. Binary, without using protobuffers.
      // The segmentation is stripped to its essentials. The use-case is visualization
      // in flash UI.
      desc.ParseFromArray(seg_frame->data(), seg_frame->size());
      string stripped_data;
      StripToEssentials(desc, &stripped_data);
      writer_.AddSegmentationDataToChunk(&stripped_data[0],
                                         stripped_data.size(),
                                         frame->pts());
    } else {
      writer_.AddSegmentationDataToChunk((const char*)seg_frame->data(),
                                         seg_frame->size(),
                                         frame->pts());
    }

    output->push_back(input);
    ++frame_number_;

    if (frame_number_ % chunk_size_ == 0) {
      writer_.WriteChunk();
      // Report current streaming size
      std::cout << "__STREAMING_SIZE__: " << frame_number_ << "\n";
    }
  }

  void SegmentationWriterUnit::PostProcess(list<FrameSetPtr>* append) {
    writer_.WriteTermHeaderAndClose();
  }

  SegmentationReaderUnit::SegmentationReaderUnit(const string& filename,
                                                 const string& seg_stream_name)
  : seg_stream_name_(seg_stream_name), reader_(filename) {

  }

  SegmentationReaderUnit::~SegmentationReaderUnit() {

  }

  bool SegmentationReaderUnit::OpenStreams(StreamSet* set) {
    bool res = reader_.OpenFileAndReadHeaders();
    reader_.SegmentationResolution(&frame_width_, &frame_height_);

    // Add segmentation stream.
    SegmentationStream* seg_stream = new SegmentationStream(frame_width_,
                                                            frame_height_,
                                                            seg_stream_name_);
    set->push_back(shared_ptr<DataStream>(seg_stream));
    seg_stream_index_ = set->size() - 1;
    return res;
  }

  void SegmentationReaderUnit::ProcessFrame(FrameSetPtr input,
                                            list<FrameSetPtr>* output) {
    // Read frame.
    int frame_sz = reader_.ReadNextFrameSize();

    DataFrame* seg_frame = new DataFrame(frame_sz);
    reader_.ReadNextFrame(seg_frame->mutable_data());

    input->push_back(shared_ptr<DataFrame>(seg_frame));
    output->push_back(input);
  }

  void SegmentationReaderUnit::PostProcess(list<FrameSetPtr>* append) {
    // If reader used as source, add frames until empty.
    if (reader_.RemainingFrames()) {
      ASSURE_LOG(seg_stream_index_ == 0)
        << "Reader encountered remaining frames but not used as source.";
      int frame_sz = reader_.ReadNextFrameSize();
      DataFrame* seg_frame = new DataFrame(frame_sz);
      reader_.ReadNextFrame(seg_frame->mutable_data());

      FrameSetPtr input(new FrameSet);
      input->push_back(shared_ptr<DataFrame>(seg_frame));
      append->push_back(input);
    }
  }

  bool SegmentationReaderUnit::SeekImpl(int64_t pts) {
    vector<int64_t>::const_iterator pos =
        std::lower_bound(reader_.TimeStamps().begin(),
                         reader_.TimeStamps().end(),
                         pts);
    if (pos == reader_.TimeStamps().end() || *pos != pts)
      return false;

    reader_.SeekToFrame(pos - reader_.TimeStamps().begin());
    return true;
  }

  SegmentationRenderUnit::SegmentationRenderUnit(float blend_alpha,
                                                 float hierarchy_level,
                                                 bool highlight_edges,
                                                 bool draw_shape_descriptors,
                                                 bool concat_with_source,
                                                 const string& vid_stream_name,
                                                 const string& seg_stream_name,
                                                 const string& out_stream_name)
     :  vid_stream_name_(vid_stream_name),
        seg_stream_name_(seg_stream_name),
        out_stream_name_(out_stream_name),
        blend_alpha_(blend_alpha),
        hierarchy_level_(hierarchy_level),
        highlight_edges_(highlight_edges),
        draw_shape_descriptors_(draw_shape_descriptors),
        concat_with_source_(concat_with_source) {
  }

  bool SegmentationRenderUnit::OpenStreams(StreamSet* set) {
    frame_width_ = 0;
    frame_height_ = 0;
    frame_width_step_ = 0;

    // Find video stream idx.
    float fps = 0;
    int frame_count = 0;
    if (vid_stream_name_.empty()) {
      if (blend_alpha_ != 1.f) {
        blend_alpha_ = 1.f;
        LOG(WARNING) << "No video stream request. Fixing blend alpha to 1.";
      }
      fps = 25;         // Standard values.
      frame_count = 0;  // Unknown.
      vid_stream_idx_ = -1;
    } else {
      vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);

      if (vid_stream_idx_ < 0) {
        LOG(ERROR) << "Could not find Video stream!\n";
        return false;
      }

      // Get video stream info.
      const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                          set->at(vid_stream_idx_).get());
      assert(vid_stream);

      frame_width_ = vid_stream->frame_width();
      frame_height_ = vid_stream->frame_height();
      frame_width_step_ = vid_stream->width_step();
      fps = vid_stream->fps();
      frame_count = vid_stream->frame_count();
    }

    // Get segmentation stream.
    seg_stream_idx_ = FindStreamIdx(seg_stream_name_, set);

    if (seg_stream_idx_ < 0) {
      LOG(ERROR) << "SegmentationRenderUnit::OpenStreams: "
                 << "Could not find Segmentation stream!\n";
      return false;
    }

    if (frame_width_ == 0) {
      // Read dimensions from segmentation stream.
      const SegmentationStream* seg_stream =
          dynamic_cast<const SegmentationStream*>(set->at(seg_stream_idx_).get());
      frame_width_ = seg_stream->frame_width();
      frame_height_ = seg_stream->frame_height();
      frame_width_step_ = frame_width_ * 3;
      if (frame_width_step_ % 4) {
        frame_width_step_ += 4 - frame_width_step_ % 4;
      }
    }

    const int actual_height = frame_height_ * (concat_with_source_ ? 2 : 1);

    // Add region output stream.
    VideoStream* region_stream =
        new VideoStream(frame_width_,
                        actual_height,
                        frame_width_step_,
                        fps,
                        frame_count,
                        PIXEL_FORMAT_RGB24,
                        out_stream_name_);

    set->push_back(shared_ptr<DataStream>(region_stream));
    out_stream_idx_ = set->size() - 1;

    // Allocate render buffer.
    render_frame_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 3);
    prev_chunk_id_ = -1;
    frame_number_ = 0;
    return true;
  }

  void SegmentationRenderUnit::ProcessFrame(FrameSetPtr input,
                                           list<FrameSetPtr>* output) {
    // Get IplView.
    IplImage frame_view;
    // Dummy pts, if no video frame present.
    float pts = frame_number_ * 100;

    if (vid_stream_idx_ >= 0) {
      const VideoFrame* frame =
          dynamic_cast<const VideoFrame*>(input->at(vid_stream_idx_).get());
      assert(frame);
      frame->ImageView(&frame_view);
      pts = frame->pts();
    }

    // Retrieve Segmentation.
    const DataFrame* seg_frame = input->at(seg_stream_idx_).get();
    SegmentationDesc desc;

    desc.ParseFromArray(seg_frame->data(), seg_frame->size());

    // If it is the first frame, save it into seg_hier_;
    if (!seg_hier_) {
      seg_hier_.reset(new SegmentationDesc(desc));

      if (hierarchy_level_ == floor(hierarchy_level_)) {
        hierarchy_level_ = std::min<int>(hierarchy_level_,
                                         seg_hier_->hierarchy_size() - 1);
      } else {
        hierarchy_level_ =
          (int)(hierarchy_level_ * (seg_hier_->hierarchy_size() - 1.f));
      }
    }

    SegmentationDesc* seg_hier = 0;
    // Update hierarchy pointer when one present.
    if (desc.hierarchy_size() > 0) {
      *seg_hier_ = desc;
    }

    if (seg_hier_->hierarchy_size() > 0) {
      seg_hier = seg_hier_.get();
    }

    // Fractional or direct specification for hierarchy-level?
    int level = hierarchy_level_;

    // Render to temporary frame.
    RenderRegionsRandomColor(render_frame_->imageData,
                             render_frame_->widthStep,
                             render_frame_->width,
                             render_frame_->height,
                             3,
                             level,
                             highlight_edges_,
                             draw_shape_descriptors_,
                             desc,
                             &seg_hier_->hierarchy());

    // Allocate new output frame.
    VideoFrame* region_frame =
        new VideoFrame(frame_width_,
                       frame_height_ * (concat_with_source_ ? 2 : 1),
                       3,
                       frame_width_step_,
                       pts);
    IplImage region_view;
    region_frame->ImageView(&region_view);

    if (concat_with_source_) {
      ASSURE_LOG(vid_stream_idx_ >= 0)
        << "Request concatenation with source but no video stream present.";

      cvSetImageROI(&region_view, cvRect(0, 0, frame_width_, frame_height_));
      cvCopy(render_frame_.get(), &region_view);

      cvResetImageROI(render_frame_.get());
      cvSetImageROI(&region_view,
                    cvRect(0, frame_height_, frame_width_, frame_height_));
      cvCopy(&frame_view, &region_view);

      cvResetImageROI(render_frame_.get());
    } else {
      if (vid_stream_idx_ >= 0) {
        // Blend with input.
        cvAddWeighted(&frame_view,
                      1.0 - blend_alpha_,
                      render_frame_.get(),
                      blend_alpha_,
                      0,              // Offset.
                      &region_view);
      } else {
        cvCopy(render_frame_.get(), &region_view);
      }
    }

    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 0.8, 0.8);

    if (desc.chunk_id() != prev_chunk_id_) {
      prev_chunk_id_ = desc.chunk_id();
      std::string output_text = std::string("Change to chunk id ") +
          lexical_cast<std::string>(desc.chunk_id());

      cvPutText(&region_view,
                output_text.c_str(),
                cvPoint(5, frame_height_ - 30),
                &font,
                cvScalar(255, 255, 255));
    }

    std::string output_text = std::string("Frame #") +
        lexical_cast<std::string>(frame_number_);

    cvPutText(&region_view,
              output_text.c_str(),
              cvPoint(5, frame_height_ - 10),
              &font,
              cvScalar(255, 255, 255));

    input->push_back(shared_ptr<DataFrame>(region_frame));
    output->push_back(input);
    ++frame_number_;
  }

  void SetupOptionsDescription(
      boost::program_options::options_description* options) {
    namespace po = boost::program_options;
    options->add_options()
      ("help", "produce this help message")
      ("i", po::value<string>(), "input video file, use CAMERA to capture from camera")
      ("noflow", "do not use optical flow even if present")
      ("oversegment", "only use oversegmentation, no hierarchical segmentation")
      ("display_level", po::value<float>(), "displays the segmentation at the "
       "specified level")
      ("render_and_save",
       "renders the created segmentations at 3 levels and writes them to file")
      ("write_to_file", "write the segmentations to file")
      ("extract_sift", "Attaches Sift unit and extracts sift features")
      ("save_descriptors", "Saves region descriptors to segmentation protobuffer")
      ("trim_to", po::value<int>(), "Stops processing the video after specified "
           "frames")
      ("strip", "Strips written segmentation files to hierarchy and scanlines")
      ("klt_flow", "Extracts KLT flow features (multi-frame) and saves to file.")
      ("smoothing", po::value<string>(),
           "Pre-smoothing of images: bilateral or gaussian")
      ("color_dist", po::value<string>(), "Oversegmentatino color distance: l1 or l2")
      ("overseg_tau", po::value<float>(), "Oversegmentation tau")
      ("overseg_minsz", po::value<float>(),
           "Minimum size of regions in over-segmentation")
      ("overlap_ratio", po::value<float>(0),
           "Ratio of overlap to clip-size. Must be < 1")
      ("region_connectivity", po::value<string>(), "Connectivity of Regions: n4 or n8")
      ("thin_structure_suppression", po::value<string>(),
       "Removes small 1 pixel structures and offchutes: on or off.")
      ("min_region_num", po::value<int>(), "Minimum number of regions per hierarchy")
      ("max_region_num", po::value<int>(), "Maximum number of regions per hierarchy")
      ("level_cutoff_fraction", po::value<float>(), "Desired number of regions in current"
                                                    " level w.r.t. previous one.")
      ("lum_bins", po::value<int>(), "Luminance bins in region descriptor")
      ("color_bins", po::value<int>(), "Color bins in region descriptor")
      ("flow_bins", po::value<int>(), "Flow bins in region descriptor")
      ("depth", po::value<string>(), "input depth video file")
    ;
  }

  bool SetOptionsFromVariableMap(const boost::program_options::variables_map& vm,
                                 Segment::DenseSegmentOptions* seg_options,
                                 Segment::RegionSegmentOptions* region_options) {
    if (vm.count("smoothing")) {
      if (vm["smoothing"].as<string>() == "bilateral") {
        seg_options->set_presmoothing(Segment::DenseSegmentOptions::PRESMOOTH_BILATERAL);
      } else if (vm["smoothing"].as<string>() == "gaussian") {
        seg_options->set_presmoothing(Segment::DenseSegmentOptions::PRESMOOTH_GAUSSIAN);
      } else {
        std::cout << "Unknown smoothing mode specified.";
        return false;
      }
    }

    if (vm.count("overseg_tau")) {
      const float tau = vm["overseg_tau"].as<float>();
      if (tau < 0) {
        std::cout << "Only positive weights are allowed.";
        return false;
      }
      seg_options->set_parameter_k(tau);
    }

    if (vm.count("color_dist")) {
      if (vm["color_dist"].as<string>() == "l1") {
        seg_options->set_color_distance(Segment::DenseSegmentOptions::COLOR_DISTANCE_L1);
      } else if (vm["color_dist"].as<string>() == "l2") {
        seg_options->set_color_distance(Segment::DenseSegmentOptions::COLOR_DISTANCE_L2);
      } else {
        std::cout << "Unknown color distance specified.";
        return false;
      }
    }

    if (vm.count("overseg_minsz")) {
      const float overseg_minsz = vm["overseg_minsz"].as<float>();
      seg_options->set_frac_min_region_size(overseg_minsz);
      region_options->set_frac_min_region_size(overseg_minsz);
    }

    if (vm.count("overlap_ratio")) {
      const float overlap_ratio = vm["overlap_ratio"].as<float>();
      if (overlap_ratio < 0 || overlap_ratio > 0.8) {
        std::cout << "Overlap ratio must be between [0, 0.8]";
        return false;
      }
      seg_options->set_chunk_overlap_ratio(overlap_ratio);
    }

    if (vm.count("region_connectivity")) {
      if (vm["region_connectivity"].as<string>() == "n4") {
        seg_options->set_enforce_n4_connectivity(true);
      } else if (vm["region_connectivity"].as<string>() == "n8") {
        seg_options->set_enforce_n4_connectivity(false);
      } else {
        std::cout << "Unknown region connectivity specified.";
        return false;
      }
    }

    if (vm.count("thin_structure_suppression")) {
      if (vm["thin_structure_suppression"].as<string>() == "on") {
        seg_options->set_thin_structure_suppression(true);
      } else if (vm["thin_structure_suppression"].as<string>() == "off") {
        seg_options->set_thin_structure_suppression(false);
      } else {
        std::cout << "Unknown thin_structure suppression specified.";
        return false;
      }
    }

    if (vm.count("min_region_num")) {
      const int min_region_num = vm["min_region_num"].as<int>();
      if (min_region_num < 5) {
        std::cout << "Minimum region number must be larger than 5.";
        return false;
      } else {
        region_options->set_min_region_num(min_region_num);
      }
    }

    if (vm.count("max_region_num")) {
      const int max_region_num = vm["max_region_num"].as<int>();
      if (max_region_num < 20) {
        std::cout << "Max region number must be larger than 20.";
        return false;
      } else {
        region_options->set_max_region_num(max_region_num);
      }
    }

    if (vm.count("level_cutoff_fraction")) {
      const float level_cutoff_fraction = vm["level_cutoff_fraction"].as<float>();
      if (level_cutoff_fraction < 0 || level_cutoff_fraction >= 1) {
        std::cout << "Level cutoff fraction must be in (0, 1)";
        return false;
      } else {
        region_options->set_level_cutoff_fraction(level_cutoff_fraction);
      }
    }

    if (vm.count("lum_bins")) {
      const int lum_bins = vm["lum_bins"].as<int>();
      if (lum_bins < 1 || lum_bins > 30) {
        std::cout << "Luminance bins must be within [1, 30].";
      } else {
        region_options->set_luminance_bins(lum_bins);
      }
    }

    if (vm.count("color_bins")) {
      const int color_bins = vm["color_bins"].as<int>();
      if (color_bins < 1 || color_bins > 30) {
        std::cout << "Color bins must be within [1, 30].";
      } else {
        region_options->set_color_bins(color_bins);
      }
    }

    if (vm.count("flow_bins")) {
      const int flow_bins = vm["flow_bins"].as<int>();
      if (flow_bins < 1 || flow_bins > 32) {
        std::cout << "Flow bins must be within [1, 32].";
      } else {
        region_options->set_flow_bins(flow_bins);
      }
    }

    if (vm.count("save_descriptors")) {
      region_options->set_save_descriptors(true);
    }
    return true;
  }
}

