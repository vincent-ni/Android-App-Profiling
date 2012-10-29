/*
 *  joint_segmentation_unit.cpp
 *  segmentation
 *
 *  Created by Matthias Grundmann on 9/22/10.
 *  Copyright 2010 Matthias. All rights reserved.
 *
 */

#include "joint_segmentation_unit.h"

#include <core/core_c.h>
#include <imgproc/imgproc_c.h>

#include "image_util.h"
#include "segmentation_util.h"
#include "segmentation_unit.h"

using namespace ImageFilter;

namespace Segment {
JointSegmentationUnit::JointSegmentationUnit(const string& video_stream_name,
                                             const string& sift_stream_name,
                                             const string& segment_stream_name,
                                             const string& video_out_stream_name,
                                             const string& segment_out_stream_name) 
    : video_stream_name_(video_stream_name),
      sift_stream_name_(sift_stream_name),
      segment_stream_name_(segment_stream_name),
      video_out_stream_name_(video_out_stream_name), 
      segment_out_stream_name_(segment_out_stream_name) {
}
 
void JointSegmentationUnit::OpenPoolStreams(const vector<StreamSet>& stream_sets) {
  ASSURE_LOG(InputUnits() == 2) << "Can only process two input units.";
  
  video_stream_indices_.resize(InputUnits());
  sift_stream_indices_.resize(InputUnits());
  segment_stream_indices_.resize(InputUnits());
  region_id_images_.resize(InputUnits());
  frame_buffer_.resize(InputUnits());
  
  frame_widths_.resize(InputUnits());
  frame_heights_.resize(InputUnits());
  
  for (int i = 0; i < InputUnits(); ++i) {
    video_stream_indices_[i] = FindStreamIdx(video_stream_name_, &stream_sets[i]);
    ASSURE_LOG(video_stream_indices_[i] >= 0) << "Could not locate video stream";
    
    // Get resolutions.
    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
       stream_sets[i][video_stream_indices_[i]].get());
    ASSERT_LOG(vid_stream);
    
    frame_widths_[i] = vid_stream->get_frame_width();
    frame_heights_[i] = vid_stream->get_frame_height();
    
    if (i > 0) {
      ASSURE_LOG(frame_widths_[i] == frame_widths_[i-1])
          << "Only same sized input videos supported.";
      ASSURE_LOG(frame_heights_[i] = frame_heights_[i-1])
          << "Only same sized input videos supported.";
    }
    
    region_id_images_[i] = cvCreateImageShared(frame_widths_[i], frame_heights_[i],
                                               IPL_DEPTH_32S, 1);
    frame_buffer_[i] = cvCreateImageShared(frame_widths_[i], frame_heights_[i],
                                           IPL_DEPTH_8U, 3); 
    
    // Locate sift and segmentation streams.
    sift_stream_indices_[i] = FindStreamIdx(sift_stream_name_, &stream_sets[i]);
    ASSURE_LOG(sift_stream_indices_[i]) << "Could not locate sift stream";
    
    segment_stream_indices_[i] = FindStreamIdx(segment_stream_name_, &stream_sets[i]);
    ASSURE_LOG(segment_stream_indices_[i]) << "Could not locate segmentation stream name";
  }
  
  output_width_ = frame_widths_[0];
  output_width_step_ = output_width_ * 3;
  output_height_ = 2 * frame_heights_[0];
}
  
void JointSegmentationUnit::ProcessAllPoolFrames(
    const vector< vector<FrameSetPtr> >& frame_sets) {
  vector<RegionMatch> region_matches;
  ComputeAffinityMatrix(frame_sets, &region_matches);
  
  PerformJointSegmentation(frame_sets, region_matches);
}
 
bool JointSegmentationUnit::OpenRootStream(StreamSet* set) {
  // Add output video stream.
  VideoStream* render_stream = new VideoStream(output_width_,
                                               output_height_,
                                               output_width_step_,
                                               20,
                                               0,
                                               PIXEL_FORMAT_RGB24,
                                               video_out_stream_name_);
  set->push_back(shared_ptr<DataStream>(render_stream));
  
  // Add segmentation stream.
  SegmentationStream* seg_stream = new SegmentationStream(segment_out_stream_name_);
  set->push_back(shared_ptr<DataStream>(seg_stream));
  
  output_drained_ = false;
  return true;
}
  
void JointSegmentationUnit::RootPostProcess(list<FrameSetPtr>* append) {
  // Simply output all VideoFrames.
  if (!output_drained_) {
   /* for (vector<shared_ptr<VideoFrame> >::const_iterator vid_frame = 
             output_frames_.begin();
         vid_frame != output_frames_.end();
         ++vid_frame) {
      FrameSetPtr new_frameset(new FrameSet());
      new_frameset->push_back(*vid_frame);
      append->push_back(new_frameset);
    }
    */
    output_drained_ = true;
    output_frames_.clear();
  }
}
  
void JointSegmentationUnit::ComputeAffinityMatrix(
    const vector< vector<FrameSetPtr> >& frame_sets,
    vector<RegionMatch>* region_matches) {
  // Build affinity matrix, sort features by region identity.
  const int num_inputs = frame_sets.size();
  vector< vector<SortedSift> > sorted_sift(num_inputs);
  
  vector<shared_ptr<SegmentationDesc> > hierarchies(num_inputs);
  vector<int> levels(num_inputs);
  vector<int> num_regions(num_inputs);
  
  const int max_frames = 120;
  
  for (int unit_id = 0; unit_id < num_inputs; ++unit_id) {
    int frame_idx = 0;
    for (vector<FrameSetPtr>::const_iterator frame_set = frame_sets[unit_id].begin();
         frame_set != frame_sets[unit_id].begin() + max_frames;
         ++frame_set, ++frame_idx) {
      // Get Sift frame.
      const VideoFramework::SiftFrame* sift_frame =
      dynamic_cast<const VideoFramework::SiftFrame*>(
          (*frame_set)->at(sift_stream_indices_[unit_id]).get());
      ASSURE_LOG(sift_frame);
      
      // Get Segmentation frame.
      const DataFrame* seg_frame = (*frame_set)->at(segment_stream_indices_[unit_id]).get();
      ASSURE_LOG(seg_frame);
      
      shared_ptr<SegmentationDesc> desc(new SegmentationDesc);
      desc->ParseFromArray(seg_frame->get_data(), seg_frame->get_size());
      
      // Initialize and update hierarchy.
      if (hierarchies[unit_id] == NULL) {        
        levels[unit_id] = desc->hierarchy_size() / 2;
        if (levels[unit_id] > 0) {
          hierarchies[unit_id] = desc;
        }
        num_regions[unit_id] = desc->hierarchy(levels[unit_id]).max_id();
      }
      
      if (desc->hierarchy_size() > 0) {
        hierarchies[unit_id] = desc;
      }
      
      // Render segmentation.
      SegmentationDescToIdImage((int*)region_id_images_[unit_id]->imageData,
                                region_id_images_[unit_id]->widthStep,
                                frame_widths_[unit_id],
                                frame_heights_[unit_id],
                                levels[unit_id],
                                *desc,
                                hierarchies[unit_id].get());
      
      // Insert sift features into sorted_sift.
      const vector<SiftFrame::SiftFeature>& sift_features = sift_frame->Features();
      for (int i = 0; i < sift_features.size(); ++i) {
        const int region_id = *(RowPtr<int>(region_id_images_[unit_id],
                                            (int)sift_features[i].y) + 
                                (int)sift_features[i].x);
        SortedSift ss(&sift_features[i], region_id, frame_idx);
        sorted_sift[unit_id].insert(std::lower_bound(sorted_sift[unit_id].begin(),
                                                     sorted_sift[unit_id].end(),
                                                     ss),
                                    ss);
      }
    }
  }
  
  // Construct affinity matrix.
  LOG(INFO_V1) << "Creating affinity matrix of size "
               << sorted_sift[0].size() << "x" << sorted_sift[1].size();
  
  shared_ptr<CvMat> affinity_matrix = cvCreateMatShared(sorted_sift[0].size(), 
                                                        sorted_sift[1].size(),
                                                        CV_MAT32F);
  
  for (int i = 0; i < sorted_sift[0].size(); ++i) {
    if (i % 50 == 0) {
      LOG(INFO_V1) << "Processing row " << i;
    }
    
    float* aff_row_ptr = RowPtr<float>(affinity_matrix, i);
    for (int j = 0; j < sorted_sift[1].size(); ++j, ++aff_row_ptr) {
      *aff_row_ptr = sorted_sift[0][i].feature->L2Diff(*sorted_sift[1][j].feature);
    }
  }
  
  // Prune affinity matrix (Per frame constraint, d_max / d_{max-1} < 0.6),
  // and establish region affinity matix.
  shared_ptr<CvMat> region_affinity = cvCreateMatShared(num_regions[0],
                                                        num_regions[1],
                                                        CV_MAT32F);
  cvSet(region_affinity.get(), cvScalar(0));
  
  vector<BestSiftMatch> best_matches;
  for (int i = 0; i < sorted_sift[0].size(); ++i) {
    float* aff_row_ptr = RowPtr<float>(affinity_matrix, i);
    float* const orig_aff_row_ptr = aff_row_ptr;
    
    const int num_matches = frame_sets[1].size();
    best_matches = vector<BestSiftMatch>(num_matches);
    for (int j = 0; j < sorted_sift[1].size(); ++j, ++aff_row_ptr) {
      // Test for minimum.
      const int frame_idx = sorted_sift[1][j].frame;
      if (*aff_row_ptr < best_matches[frame_idx].min_dist) {
        best_matches[frame_idx].second_min_dist = best_matches[frame_idx].min_dist;
        best_matches[frame_idx].min_dist = *aff_row_ptr;
        best_matches[frame_idx].min_dist_idx = j;
      } else if (*aff_row_ptr < best_matches[frame_idx].second_min_dist) {
        best_matches[frame_idx].second_min_dist = *aff_row_ptr;
      }
      
      aff_row_ptr[0] = 1;
    }
    
    // Only keep this feature if it satisfies constraint.
    const int region_id_0 = sorted_sift[0][i].region_id;
    for (int f = 0; f < num_matches; ++f) {
      if (best_matches[f].min_dist / best_matches[f].second_min_dist < 0.6) {
        // Restore value.
        orig_aff_row_ptr[best_matches[f].min_dist_idx] = best_matches[f].min_dist;
        // Increment frame-match counter.
        const int region_id_1 = sorted_sift[1][best_matches[f].min_dist_idx].region_id;
        ++(RowPtr<float>(region_affinity, region_id_0)[region_id_1]);
      }
    }
  }
  
  // Look for high feature count region matches.
  for (int i = 0; i < num_regions[0]; ++i) {
    const float* region_aff_ptr = RowPtr<float>(region_affinity, i);
    // Determine sum of matching features.
    const float* sum_ptr = region_aff_ptr;
    float aff_sum = 0;
    for (int j = 0; j < num_regions[1]; ++j, ++sum_ptr) {
      aff_sum += *sum_ptr;
    }
    
    for (int j = 0; j < num_regions[1]; ++j, ++region_aff_ptr) {
      if (*region_aff_ptr > 20) {
        RegionMatch match;
        match.region_id_1 = i;
        match.level_1 = levels[0];
        match.region_id_2 = j;
        match.level_2 = levels[1];
        match.strength = *region_aff_ptr / aff_sum;
        
        region_matches->push_back(match);
      }
    }
  }
  
  // Save to file.
  LOG(INFO_V1) << "Writing affinity matrix";
  std::ofstream aff_off("affinity_matrix.txt");
  CvMat* out_matrix = region_affinity.get();
  for (int i = 0; i < out_matrix->rows; ++i) {
    const float* aff_row_ptr = RowPtr<float>(out_matrix, i);
    for (int j = 0; j < out_matrix->cols; ++j, ++aff_row_ptr) {
      aff_off << *aff_row_ptr;
      if (j + 1 != out_matrix->cols) {
        aff_off << " ";
      }
    }
    
    if (i + 1 != out_matrix->rows) {
      aff_off << "\n";
    }
  }
}
  
void JointSegmentationUnit::VisualizeAndOutputSift(const FrameSetPtrPool& frame_sets) {
  // Simply combine frames into one image and save for later.
  int max_frames = 0;
  for (int i = 0; i < InputUnits(); ++i) {
    max_frames = std::max<int>(max_frames, frame_sets[i].size());
  }
  
  for (int frame_idx = 0; frame_idx < max_frames; ++frame_idx) {
    // Create new VideoFrame.
    VideoFrame* render_frame = new VideoFrame(output_width_, output_height_, 3,
                                              3 * output_width_, 0);
    // Initialize frame to black.
    memset(render_frame->get_mutable_data(), 0, render_frame->get_size());
    
    IplImage render_view;
    render_frame->ImageView(&render_view);
    
    const int per_frame_width = output_width_;
    const int per_frame_height = output_height_ / InputUnits();
        
    for (int unit_id = 0; unit_id < InputUnits(); ++unit_id) {
      // Set ROI and resize inputs to fit.
      cvSetImageROI(&render_view, cvRect(0, unit_id * per_frame_height,
                                         per_frame_width, per_frame_height));
      
      // Resize first input to fit, if enough frames are available.
      if (frame_sets[unit_id].size() > frame_idx) {
        const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
            frame_sets[unit_id][frame_idx]->at(video_stream_indices_[unit_id]).get());
        ASSERT_LOG(frame);
        
        IplImage frame_view;
        frame->ImageView(&frame_view);
        
        // Render sift features on it.
        const VideoFramework::SiftFrame* sift_frame =
        dynamic_cast<const VideoFramework::SiftFrame*>(
            frame_sets[unit_id][frame_idx]->at(sift_stream_indices_[unit_id]).get());
         
        ASSURE_LOG(sift_frame);
         
        const vector<SiftFrame::SiftFeature>& sift_features = sift_frame->Features();
        for (int i = 0; i < sift_features.size(); ++i) {
          // Draw ellipse around feature location.
          cvDrawEllipse(&frame_view, 
                        cvPoint(sift_features[i].x, sift_features[i].y),
                        cvSize(sift_features[i].scale, sift_features[i].scale),
                        0, 0, 360, cvScalar(50, 255, 255));
        }
        
        cvResize(&frame_view, &render_view);
        cvResetImageROI(&render_view);
      }
    }  // end unit processing.
    
    // Save for output.
    output_frames_.push_back(shared_ptr<VideoFrame>(render_frame));
  }
}
  
void JointSegmentationUnit::VisualizeAndOutputRegionMatches(
  const FrameSetPtrPool& frame_sets, const vector<RegionMatch>& region_matches) {
  vector<int> levels(InputUnits());  
  vector<shared_ptr<SegmentationDesc> > hierarchies(InputUnits());
  const int max_frames = 120;
  
  for (int frame_idx = 0; frame_idx < max_frames; ++frame_idx) {
    // Create new VideoFrame.
    VideoFrame* render_frame = new VideoFrame(output_width_, output_height_, 3,
                                              3 * output_width_, 0);
    // Initialize frame to black.
    memset(render_frame->get_mutable_data(), 0, render_frame->get_size());
    
    IplImage render_view;
    render_frame->ImageView(&render_view);
    
    const int per_frame_width = output_width_;
    const int per_frame_height = output_height_ / InputUnits();
    
    vector<shared_ptr<SegmentationDesc> > segmentations;
    int max_region_id = 0;
    
    for (int unit_id = 0; unit_id < InputUnits(); ++unit_id) {
      // Set ROI and resize inputs to fit.
      cvSetImageROI(&render_view, cvRect(0, unit_id * per_frame_height,
                                         per_frame_width, per_frame_height));
      
      // Resize first input to fit, if enough frames are available.
      if (frame_sets[unit_id].size() > frame_idx) {
        const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
            frame_sets[unit_id][frame_idx]->at(video_stream_indices_[unit_id]).get());
        ASSERT_LOG(frame);
        
        IplImage frame_view;
        frame->ImageView(&frame_view);
        
        // Get Segmentation frame.
        const DataFrame* seg_frame = 
        frame_sets[unit_id][frame_idx]->at(segment_stream_indices_[unit_id]).get();
        ASSURE_LOG(seg_frame);
        
        shared_ptr<SegmentationDesc> desc(new SegmentationDesc);
        desc->ParseFromArray(seg_frame->get_data(), seg_frame->get_size());
        
        // Initialize and update hierarchy.
        if (hierarchies[unit_id] == NULL) {        
          levels[unit_id] = desc->hierarchy_size() / 2;
          if (levels[unit_id] > 0) {
            hierarchies[unit_id] = desc;
          }
        }
        
        if (desc->hierarchy_size() > 0) {
          hierarchies[unit_id] = desc;
        }
        
        // Render segmentation.
        RenderRegionsRandomColor(frame_buffer_[unit_id]->imageData,
                                 frame_buffer_[unit_id]->widthStep,
                                 frame_buffer_[unit_id]->width,
                                 frame_buffer_[unit_id]->height,
                                 3,
                                 levels[unit_id],
                                 true,
                                 false,
                                 *desc,
                                 hierarchies[unit_id].get());
        
        segmentations.push_back(desc);
        
        cvResize(frame_buffer_[unit_id].get(), &render_view);
        cvResetImageROI(&render_view);
      }
    }  // end unit processing.
    
    typedef hash_map<int, vector<const Region*> > ParentMap;
    ParentMap parent_map_0;
    ParentMap parent_map_1;
    GetParentMap(levels[0], *segmentations[0], hierarchies[0].get(), &parent_map_0);
    GetParentMap(levels[1], *segmentations[1], hierarchies[1].get(), &parent_map_1);
    
    // Show matches in video.
    for(vector<RegionMatch>::const_iterator match = region_matches.begin();
        match != region_matches.end();
        ++match) {
      ParentMap::const_iterator region_iter_0 = parent_map_0.find(match->region_id_1);
      ParentMap::const_iterator region_iter_1 = parent_map_1.find(match->region_id_2);
      
      if (region_iter_0 != parent_map_0.end() &&
          region_iter_1 != parent_map_1.end()) {
        // Get center location for both matches.
        ShapeDescriptor shape_desc_0, shape_desc_1;
        GetShapeDescriptorFromRegions(region_iter_0->second, &shape_desc_0);
        GetShapeDescriptorFromRegions(region_iter_1->second, &shape_desc_1);
        
        // Scale center locations to points on output frame.
        CvPoint center_0 = cvPoint(shape_desc_0.center_x / frame_widths_[0] * per_frame_width,
                                   shape_desc_0.center_y / frame_heights_[0] * per_frame_height);
        
        CvPoint center_1 = cvPoint(shape_desc_1.center_x / frame_widths_[1] * per_frame_width,
                                   shape_desc_1.center_y / frame_heights_[1] * per_frame_height + 
                                   per_frame_height);
        
        srand(match->region_id_1);
        cvLine(&render_view, center_0, center_1, cvScalar(rand() % 128,
                                                          rand() % 128,
                                                          rand() % 128),
               std::min<int>(match->strength / 500, 5), CV_AA);
      }
    }
    
    // Save for output.
    output_frames_.push_back(shared_ptr<VideoFrame>(render_frame));
  }  
}

void JointSegmentationUnit::PerformJointSegmentation(
    const FrameSetPtrPool& frame_sets,
    const vector<RegionMatch>& region_matches) {
  vector<shared_ptr<SegmentationDesc> > hierarchies(InputUnits());
  
  // Perform joint segmentation.
  // TODO: Currently both videos need to be of the same size.
  ASSURE_LOG(frame_widths_[0] == frame_widths_[1]);
  ASSURE_LOG(frame_heights_[0] == frame_heights_[1]);
  segment_.reset(new Segmentation(0.01, frame_widths_[0], frame_heights_[0], 0));
  
  shared_ptr<IplImage> lab_image = cvCreateImageShared(frame_widths_[0],
                                                       frame_heights_[0],
                                                       IPL_DEPTH_8U, 
                                                       3);
  // Region id shift between successive input (max id in prev. segmentation).
  vector<int> id_shift(InputUnits(), 0);
  // Frame shift between successive input units.
  vector<int> frame_shift(InputUnits(), 0);
  
  size_t max_frames = 0;
  
  for (int unit_id = 0; unit_id < 2; ++unit_id) {
    max_frames = std::max(frame_sets[unit_id].size(), max_frames);
    
    if (unit_id > 0) {
      frame_shift[unit_id] = frame_shift[unit_id - 1] + 
          frame_sets[unit_id - 1].size();
    }
    
    for (vector<FrameSetPtr>::const_iterator frame_set_iter = 
             frame_sets[unit_id].begin(); 
         frame_set_iter != frame_sets[unit_id].end();
         ++frame_set_iter) {
      const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
          (*frame_set_iter)->at(video_stream_indices_[unit_id]).get());
      ASSURE_LOG(frame);
        
      IplImage frame_view;
      frame->ImageView(&frame_view);
        
      // Get Segmentation frame.
      const DataFrame* seg_frame = 
          (*frame_set_iter)->at(segment_stream_indices_[unit_id]).get();
      ASSURE_LOG(seg_frame);
        
      shared_ptr<SegmentationDesc> desc(new SegmentationDesc);
      desc->ParseFromArray(seg_frame->get_data(), seg_frame->get_size());
      
      if (unit_id > 0) {
        id_shift[unit_id] = id_shift[unit_id - 1] + desc->max_id();
        // Shift current segmentation.
        DisplaceRegionIds(id_shift[unit_id - 1], desc.get());
      } else {
        id_shift[unit_id] = desc->max_id();
      }
        
      // Add this to over-segmentation.
      cvCvtColor(&frame_view, lab_image.get(), CV_RGB2Lab);
      segment_->AddOverSegmentation(*desc,
                                    lab_image.get(),
                                    10, 20, 16, 0, 0, 0, 0);        
        
      // Initialize and update hierarchy.
      if (hierarchies[unit_id] == NULL ||
          desc->hierarchy_size() > 0) {        
        hierarchies[unit_id] = desc;
      }
    }
    
    segment_->ResetChunkBoundary();
  } // end unit processing.
  
  // Add inter-video arcs from region matches.
  for(vector<RegionMatch>::const_iterator match = region_matches.begin();
      match != region_matches.end();
      ++match) {
    vector<int> children_1;
    GetChildrenIds(match->region_id_1, 
                   match->level_1,
                   0,
                   *hierarchies[0], 
                   &children_1);
    
    vector<int> children_2;
    GetChildrenIds(match->region_id_2, 
                   match->level_2,
                   0,
                   *hierarchies[1], 
                   &children_2);
    
    // For now, connect all children in 1 with all children in 2.
    for (vector<int>::const_iterator child_1 = children_1.begin();
         child_1 != children_1.end();
         ++child_1) {
      for (vector<int>::const_iterator child_2 = children_2.begin();
           child_2 != children_2.end();
           ++child_2) {
        segment_->AddOverSegmentationConnection(*child_1, *child_2 + id_shift[0],
                                                match->region_id_1,
                                                match->region_id_2,
                                                match->strength);
      }
    }
  }
  
  hierarchies.clear();

  // Run joint segmentation.
  segment_->RunHierarchicalSegmentation(200,
                                        10, 5000);
  
  // Fetch results and visualize results.
  for (int frame_idx = 0; frame_idx < max_frames; ++frame_idx) {
    // Create new VideoFrame.
    VideoFrame* render_frame = new VideoFrame(output_width_, output_height_, 3,
                                              3 * output_width_, 0);
    // Initialize frame to black.
    memset(render_frame->get_mutable_data(), 0, render_frame->get_size());
    
    IplImage render_view;
    render_frame->ImageView(&render_view);
    
    const int per_frame_width = output_width_;
    const int per_frame_height = output_height_ / InputUnits();
    
    shared_ptr<SegmentationDesc> output_segment(new SegmentationDesc);
    for (int unit_id = 0; unit_id < InputUnits(); ++unit_id) {
      // Set ROI and resize inputs to fit.
      cvSetImageROI(&render_view, cvRect(0, unit_id * per_frame_height,
                                         per_frame_width, per_frame_height));
      
      // Resize first input to fit, if enough frames are available.
      if (frame_sets[unit_id].size() > frame_idx) {
        const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
            frame_sets[unit_id][frame_idx]->at(video_stream_indices_[unit_id]).get());
        ASSERT_LOG(frame);
        
        IplImage frame_view;
        frame->ImageView(&frame_view);
        cvCopy(&frame_view, &render_view);
        
        // Get Segmentation frame.
        shared_ptr<SegmentationDesc> desc(new SegmentationDesc);
        segment_->RetrieveSegmentation3D(
            frame_idx + frame_shift[unit_id],
            false,
            vector<int>(segment_->ComputedHierarchyLevels(), 0),
            false,
            desc.get());

        // Simply copy on first id.
        if (unit_id == 0) {
          output_segment.swap(desc);
        } else {
          // Merge with previous segmentation.
          // This essentially means adding all regions from the over-segmentation
          // to output_segment as hierarchy is global.
          for (RepeatedPtrField<SegmentationDesc::Region>::const_iterator region =
                   desc->region().begin();
               region != desc->region().end();
               ++region) {
            // Add region to output_segment.
            if (ContainsRegion(region->id(), *output_segment)) {
              // Only add scanlines.
              SegmentationDesc::Region* existing_region = 
                  GetMutableRegionFromId(region->id(), output_segment.get());
              
              int my_top_y = region->top_y() + unit_id * per_frame_height;
              // Add empty scanlines until we reach top_y;
              while (existing_region->top_y() + 
                     existing_region->scanline_size() < my_top_y) {
                existing_region->add_scanline();
              }
              
              // Copy scanlines now.
              for (int s = 0; s < region->scanline_size(); ++s) {
                SegmentationDesc::Region::Scanline* scanline =
                    existing_region->add_scanline();
                *scanline = region->scanline(s);
              }
              
              SetShapeDescriptorFromRegion(existing_region);
              
            } else {
              SegmentationDesc::Region* add_region = output_segment->add_region();
              *add_region = *region;
              add_region->set_top_y(region->top_y() + unit_id * per_frame_height);
              SetShapeDescriptorFromRegion(add_region);
            }
          }
        }
      }
    }  // end unit processing.
    
    // Immediate output to children.
    FrameSetPtr new_frameset(new FrameSet());
    new_frameset->push_back(shared_ptr<VideoFrame>(render_frame));
    DataFrame* seg_frame = new DataFrame(output_segment->ByteSize());
    output_segment->SerializeToArray(seg_frame->get_mutable_data(), 
                                     seg_frame->get_size());
    new_frameset->push_back(shared_ptr<DataFrame>(seg_frame));
    list<FrameSetPtr> append;
    append.push_back(new_frameset);
    
    ImmediatePushToChildren(&append);
  }  // end frame processing.
}

}