/*
 *  dense_segmentation_graph.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "dense_segmentation_graph.h"

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <google/protobuf/repeated_field.h>
using google::protobuf::RepeatedPtrField;
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

#include "common.h"
#include "image_util.h"
#include "segmentation.h"
#include "segmentation_util.h"

using namespace ImageFilter;

namespace Segment {

  using boost::get;

  DenseSegmentationGraph::DenseSegmentationGraph(float param_k,
                                                 int frame_width,
                                                 int frame_height,
                                                 int max_frames)
  : FastSegmentationGraph(param_k, 1.0, 2048, 5, 2 * max_frames - 1),
    num_frames_(0),
    frame_width_(frame_width),
    frame_height_(frame_height),
    max_frames_(max_frames) {
      region_ids_.create(cv::Size(frame_width_ + 2, frame_height_ + 2), CV_32S);
      frame_view_regions_.resize(frame_width * frame_height);
      orig_frame_view_ids_.resize(frame_width * frame_height);
      corresponding_id_.resize(frame_width * frame_height);
  }

  void DenseSegmentationGraph::AddNodes() {
    // Add batch of nodes first.
    const int base_idx = num_frames_ * frame_height_ * frame_width_;

    for (int i = 0; i < frame_height_; ++i) {
      int row_idx = i * frame_width_;
      for (int j = 0; j < frame_width_; ++j) {
        AddRegion(base_idx + row_idx + j, 1, param_k_);
      }
    }
  }

  void DenseSegmentationGraph::AddNodesConstrained(const SegmentationDesc& desc) {
    // Add batch of nodes first.
    const int base_idx = num_frames_ * frame_height_ * frame_width_;
    constrained_slices_.push_back(num_frames_);

    // Render to id image.
    cv::Mat id_view(region_ids_, cv::Rect(1, 1, frame_width_, frame_height_));
    SegmentationDescToIdImage(id_view.ptr<int>(0),
                              id_view.step[0],
                              frame_width_,
                              frame_height_,
                              0,
                              desc,
                              0);

    for (int i = 0; i < frame_height_; ++i) {
      const int* id_ptr = id_view.ptr<int>(i);
      int row_idx = i * frame_width_;
      for (int j = 0; j < frame_width_; ++j, ++id_ptr) {
        AddRegion(base_idx + row_idx + j, 1, param_k_, *id_ptr);
      }
    }
  }

  void DenseSegmentationGraph::AddSkeletonEdges(const SegmentationDesc& desc) {
    // Add ScanInterval skeleton (constrained regions might be spatially separated).
    // These are virtual edges (high-cost edges to be placed in last bucket)
    // to force constrained regions to merge.
    const int base_idx = frame_width_ * frame_height_ * num_frames_;
    int prev_idx = base_idx;
    const int bucket_list_idx = 2 * num_frames_;
    for (RepeatedPtrField<Region2D>::const_iterator r =
         desc.region().begin();
         r != desc.region().end();
         ++r) {
      for (RepeatedPtrField<ScanInterval>::const_iterator scan_inter =
           r->raster().scan_inter().begin();
           scan_inter != r->raster().scan_inter().end();
           ++scan_inter) {
        int curr_idx = base_idx + scan_inter->y() * frame_width_ +
        scan_inter->left_x();
        AddEdge(prev_idx, curr_idx, 1e6f, bucket_list_idx);
        prev_idx = curr_idx;
      }

      const int constraint_id = regions_[prev_idx].constraint_id;

      // Connect in time as well.
      hash_map<int, pair<int, int> >::const_iterator skel_iter =
      skeleton_temporal_map_.find(constraint_id);
      if (skel_iter != skeleton_temporal_map_.end() &&
          skel_iter->second.second < num_frames_) {
        ASSERT_LOG(bucket_list_idx > 0);
        AddEdge(skel_iter->second.first, prev_idx, 1e6f, bucket_list_idx - 1);
      }

      // Update map.
      skeleton_temporal_map_[constraint_id] = std::make_pair(prev_idx, num_frames_);
    }
  }

  DenseSegmentationGraph::Region* DenseSegmentationGraph::GetSpatialRegion(int id) {
    ASSERT_LOG(id >=0);
    ASSERT_LOG(id < frame_view_regions_.size());

    Region* r = &frame_view_regions_[id];
    const int parent_id = r->my_id;
    Region* parent = &frame_view_regions_[parent_id];

    if (parent->my_id == parent_id) {
      return parent;
    } else {
      parent = GetSpatialRegion(parent_id);
      r->my_id = parent->my_id;
      return parent;
    }
  }

  void DenseSegmentationGraph::FrameSizeHint(int frames) {
    const int nodes_per_frame = frame_width_ * frame_height_;
    regions_.reserve(nodes_per_frame * (frames + 1));  // One more for frame view.
    const int neighbors = nodes_per_frame * 26 / 2;    // number of regions times average
                                                       // average number of edges.
                                                       // Undirected edges -> divide by 2

    // Thats an upper bound. We have less edges at the border of an image.
    ReserveEdges(neighbors * frames);
  }

  void DenseSegmentationGraph::ComputeFrameView(int frame) {
    int start_region_id = frame * frame_width_ * frame_height_;

    int* id_backup_ptr = &orig_frame_view_ids_[0];
    Region* spatial_region_ptr = &frame_view_regions_[0];
    for (int i = 0, r_id = 0; i < frame_height_; ++i) {
      for (int j = 0;
           j < frame_width_;
           ++j, ++r_id, ++id_backup_ptr, ++spatial_region_ptr) {
        const Region* orig_region = GetRegion(r_id + start_region_id);
        const int region_id = orig_region->my_id;
        ASSURE_LOG(region_id >= num_frames_ * frame_width_ * frame_height_);

        const int constraint_id = orig_region->constraint_id;

        // Backup original id.
        *id_backup_ptr = region_id;

        // Connected component analysis here.
        // Test if 4 surrounding neighbors have same id.
        Region* connect_region = NULL;

        // Left neighbor.
        if (j > 0) {
          if (id_backup_ptr[-1] == region_id) {
            Region* frame_view_region = GetSpatialRegion(r_id - 1);
            ++frame_view_region->sz;
            spatial_region_ptr->my_id = frame_view_region->my_id;
            connect_region = frame_view_region;
          }
        }

        if (i > 0) {
          if (j > 0) {
            // Top-Left neighbor.
            if (id_backup_ptr[-frame_width_ - 1] == region_id) {
              Region* frame_view_region = GetSpatialRegion(r_id - frame_width_ - 1);
              if (connect_region && frame_view_region != connect_region) {
                connect_region = MergeRegions(connect_region, frame_view_region, 0);
              } else {
                ++frame_view_region->sz;
                spatial_region_ptr->my_id = frame_view_region->my_id;
                connect_region = frame_view_region;
              }
            }
          }

          // Top neighbor.
          if (id_backup_ptr[-frame_width_] == region_id) {
            Region* frame_view_region = GetSpatialRegion(r_id - frame_width_);
            if (connect_region && frame_view_region != connect_region) {
              connect_region = MergeRegions(connect_region, frame_view_region, 0);
            } else {
              ++frame_view_region->sz;
              spatial_region_ptr->my_id = frame_view_region->my_id;
              connect_region = frame_view_region;
            }
          }

          // Top-Right neighbor.
          if (j < frame_width_) {
            if (id_backup_ptr[-frame_width_ + 1] == region_id) {
              Region* frame_view_region = GetSpatialRegion(r_id - frame_width_ + 1);
              if (connect_region && frame_view_region != connect_region) {
                connect_region = MergeRegions(connect_region, frame_view_region, 0);
              } else {
                ++frame_view_region->sz;
                spatial_region_ptr->my_id = frame_view_region->my_id;
                connect_region = frame_view_region;
              }
            }
          }
        }

        // No connected region found, insert new one.
        if (connect_region == NULL) {
          // Insert new region.
          *spatial_region_ptr = Region(r_id, param_k_, 1, constraint_id);

          // Record correspondence.
          inv_corresponding_id_sets_[region_id].push_back(r_id);
          corresponding_id_[r_id] = region_id;
        }
      }
    }

    frame_view_frame_ = frame;
  }

  void DenseSegmentationGraph::ResetFrameView() {
    /*
    int start_region_id = frame_view_frame_ * frame_width_ * frame_height_;
    const int* id_backup_ptr = &orig_frame_view_ids_[0];
    Region* region_ptr = &regions_[start_region_id];

    for (int i = 0, sz = frame_width_ * frame_height_;
         i < sz;
         ++i, ++id_backup_ptr, ++region_ptr) {
      region_ptr->my_id = *id_backup_ptr;
    }

    // Discard newly created regions.
    regions_.erase(regions_.begin() + num_frames_ * frame_width_ * frame_height_,
                   regions_.end());

    corresponding_id_.clear();*/

    inv_corresponding_id_sets_.clear();
  }

  void DenseSegmentationGraph:: SegmentGraphSpatially() {
    vector<int> spatial_bucket_lists(num_frames_);
    for (int i = 0; i < num_frames_; ++i) {
      spatial_bucket_lists[i] = 2*i;
    }

    SegmentGraph(spatial_bucket_lists,
                 false,   // no enforced merges.
                 false);  // no constraint merges
  }


  void DenseSegmentationGraph::SpatialCleanupStepImpl(
      int min_region_size,
      int frame) {
//      hash_map<int, bool>& non_exhausted_bugets) {
    /*
    // We need to prevent temporal separations.
    // Compute max. number of allowed merges for each region, assure that a regions
    // budget never drops to zero, except if this is the first frame of this region.
    typedef hash_map<int, vector<int> > CorrList;
    hash_map<int, int> parent_budget;

    for (CorrList::iterator parent_id = inv_corresponding_id_sets_.begin();
         parent_id != inv_corresponding_id_sets_.end();
         ++parent_id) {
      // Create unified vector.
      vector<int> unified_ids;
      for (vector<int>::const_iterator spatial_id = parent_id->second.begin();
           spatial_id != parent_id->second.end();
           ++spatial_id) {
        int region_id = GetSpatialRegion(*spatial_id)->my_id;
        vector<int>::iterator n_pos = std::lower_bound(unified_ids.begin(),
                                                       unified_ids.end(),
                                                       region_id);
        if (n_pos == unified_ids.end() ||
            *n_pos != region_id) {
          unified_ids.insert(n_pos, region_id);
        }
      }

      parent_budget[parent_id->first] = unified_ids.size();
    }
    */

    // Map of spatial id -> parent id.
    hash_map<int, int> merge_map;

    const int id_offset = frame * frame_width_ * frame_height_;
    int bucket_idx = 0;
    const float inv_scale = 1.0 / scale_;

    for (vector<EdgeList>::iterator bucket = bucket_lists_[2 * frame].begin();
         bucket != bucket_lists_[2 * frame].end();
         ++bucket, ++bucket_idx) {
      EdgeList remaining_edges;
      const float weight = bucket_idx * inv_scale;

      for (EdgeList::const_iterator e = bucket->begin(), end_e = bucket->end();
           e != end_e;
           ++e) {
        Region* rep_1 = GetSpatialRegion(e->region_1 - id_offset);
        Region* rep_2 = GetSpatialRegion(e->region_2 - id_offset);

        if (rep_1->sz > rep_2->sz) {
          std::swap(rep_1, rep_2);
        }

        const int id_1 = rep_1->my_id;
        const int corr_id_1 = corresponding_id_[id_1];
        const int id_2 = rep_2->my_id;
        const int corr_id_2 = corresponding_id_[id_2];

        if (rep_1 != rep_2) {
          if (rep_1->sz < min_region_size && rep_2->sz >= min_region_size) {
            // First check if rep_1 is already flagged for merge.
            if (merge_map.find(id_1) == merge_map.end()) {
              Region* parent_region = GetRegion(corr_id_2);
              if (AreRegionsMergable(rep_1, parent_region)) {
             //   if (parent_budget[corr_id_1] > 1  ||
             //       non_exhausted_bugets.find(corr_id_1) == non_exhausted_bugets.end()) {
                  // Flag for merge.
                  merge_map[id_1] = parent_region->my_id;
                  parent_region->sz += rep_1->sz;
              //    --parent_budget[corr_id_1];
                }
              }
            //}
          }
        } else {
          remaining_edges.push_back(*e);
        }
      }

      //  bucket->swap(remaining_edges);
    }

    /*
    // Create new non_exhausted budget list.
    non_exhausted_bugets.clear();
    for (hash_map<int, int>::const_iterator budget_iter = parent_budget.begin();
         budget_iter != parent_budget.end();
         ++budget_iter) {
      ASSURE_LOG(budget_iter->second >= 0);
      if (budget_iter->second > 0) {
        non_exhausted_bugets[budget_iter->first] = true;
      }
    }
    */

    // Perform the actual merges.
    Region* spatio_temp_ptr = &regions_[id_offset];
    const int new_ids_start = num_frames_ * frame_width_ * frame_height_;

    for (int i = 0, sz = frame_width_ * frame_height_;
         i < sz;
         ++i, ++spatio_temp_ptr) {
      Region* spatial_region = GetSpatialRegion(i);
      if (spatial_region->sz >= min_region_size) {
        continue;
      }

      // Find in merge_map.
      hash_map<int, int>::const_iterator map_pos =
      merge_map.find(spatial_region->my_id);
      if (map_pos == merge_map.end()) {
        // LOG(INFO) << "Shouldn't happen too often " << i << " of " << sz;
      } else {
        spatio_temp_ptr->my_id = map_pos->second;
        //        spatio_temp_ptr->constraint_id = GetRegion(map_pos->second)->constraint_id;
      }
    }
  }

  void DenseSegmentationGraph::SpatialCleanupStep(int min_region_size) {
    // TODO (grundman): Fix!
    std::cerr << "Spatial cleanup ... ";

    FlattenUnionFind(true);     // separate representatives.

  //  hash_map<int, bool> non_exhausted_bugets;

    for (int frame = 0; frame < num_frames_; ++frame) {
      ComputeFrameView(frame);
//      hash_map<int, bool> non_exhausted_bugets;
      SpatialCleanupStepImpl(min_region_size, frame);
      ResetFrameView();
    }

    /*
    return;

    non_exhausted_bugets.clear();

    for (int frame = num_frames_ - 1; frame >= 0; --frame) {
      ComputeFrameView(frame);
      SpatialCleanupStepImpl(min_region_size, frame, non_exhausted_bugets);
      ResetFrameView();
    }
    */

    std::cerr << "done.";
  }

  void DenseSegmentationGraph::MergeSmallRegionsSpatially(int min_region_size) {
    std::cerr << "Merging small regions spatially ... ";

    for (int frame = 0; frame < num_frames_; ++frame) {
      ComputeFrameView(frame);

      const int id_offset = frame * frame_width_ * frame_height_;

      // Build adjaceny list by traversing spatial edges while pruning them.
      // This is faster than a multi-map (factor of ~1.5).
      vector< vector<int> > adjacency_list(frame_width_ * frame_height_);

      for (vector<EdgeList>::iterator bucket = bucket_lists_[frame].begin();
           bucket != bucket_lists_[frame].end();
           ++bucket) {
        EdgeList remaining_edges;
        for (EdgeList::const_iterator e = bucket->begin(), end_e = bucket->end();
             e != end_e;
             ++e) {
          // These are the new frame view representatives.
          Region* rep_1 = GetSpatialRegion(e->region_1 - id_offset);
          Region* rep_2 = GetSpatialRegion(e->region_2 - id_offset);

          if (rep_1 != rep_2) {
            int id_1 = rep_1->my_id;
            int id_2 = rep_2->my_id;
            if (id_1 > id_2) {
              std::swap(id_1, id_2);
            }

            vector<int>::iterator n_pos =
                std::lower_bound(adjacency_list[id_1].begin(),
                                 adjacency_list[id_1].end(),
                                 id_2);

            if (n_pos == adjacency_list[id_1].end() ||
                *n_pos != id_2) {
              adjacency_list[id_1].insert(n_pos, id_2);
              remaining_edges.push_back(*e);
            }
          }
        }

        bucket->swap(remaining_edges);
      }

      // Perform spatial region merges.
      int bucket_idx = 0;
      const float inv_scale = 1.0 / scale_;

      for (vector<EdgeList>::iterator bucket = bucket_lists_[frame].begin();
           bucket != bucket_lists_[frame].end();
           ++bucket, ++bucket_idx) {
        EdgeList remaining_edges;
        const float weight = bucket_idx * inv_scale;

        for (EdgeList::const_iterator e = bucket->begin(), end_e = bucket->end();
             e != end_e;
             ++e) {
          Region* rep_1 = GetSpatialRegion(e->region_1 - id_offset);
          Region* rep_2 = GetSpatialRegion(e->region_2 - id_offset);

          int id_1 = rep_1->my_id;
          int id_2 = rep_2->my_id;

          if (rep_1 != rep_2 && (
              rep_1->sz < min_region_size ||
              rep_2->sz < min_region_size)) {
            MergeRegions(rep_1, rep_2, weight);

            // Merge the corresponding spatio-temporal regions.
            int parent_id_1 = corresponding_id_[id_1];
            int parent_id_2 = corresponding_id_[id_2];

            Region* parent_rep_1 = GetRegion(parent_id_1);
            Region* parent_rep_2 = GetRegion(parent_id_2);

            int id_set_id_1 = parent_rep_1->my_id;
            int id_set_id_2 = parent_rep_2->my_id;

            // Parents should not be merged already.
            ASSURE_LOG(parent_rep_1 != parent_rep_2)
                << "Inconsistency. Shouldn't be merged.";

            Region* merged_parent; // = MergeRegions(parent_rep_1, parent_rep_2, weight);
            if (parent_rep_1 != parent_rep_2) {
              merged_parent = MergeRegions(parent_rep_1, parent_rep_2, weight);
             } else {
              continue;
             }

            // Merge corresponding id_sets_.
            vector<int>& id_set_1 = inv_corresponding_id_sets_[id_set_id_1];
            vector<int>& id_set_2 = inv_corresponding_id_sets_[id_set_id_2];

            ASSURE_LOG(id_set_1.size() > 0);
            ASSURE_LOG(id_set_2.size() > 0);

            // Perform additional spatial merges among neighbors.
            for (vector<int>::const_iterator id_1 = id_set_1.begin();
                 id_1 != id_set_1.end();
                 ++id_1) {
              for (vector<int>::const_iterator id_2 = id_set_2.begin();
                   id_2 != id_set_2.end();
                   ++id_2) {
                int test_id_1 = std::min(*id_1, *id_2);
                int test_id_2 = std::max(*id_1, *id_2);

                if (std::binary_search(adjacency_list[test_id_1].begin(),
                                       adjacency_list[test_id_1].end(),
                                       test_id_2)) {
                  Region* r_1 = GetSpatialRegion(test_id_1);
                  Region* r_2 = GetSpatialRegion(test_id_2);
                  if (r_1 != r_2) {
                    MergeRegions(r_1, r_2, weight);
                  }
                }
              }
            }

            if (merged_parent == parent_rep_1) {
              id_set_1.insert(id_set_1.end(), id_set_2.begin(), id_set_2.end());
              id_set_2.clear();
            } else {
              id_set_2.insert(id_set_2.end(), id_set_1.begin(), id_set_1.end());
              id_set_1.clear();
            }
          }
        }
      }

      ResetFrameView();
    }
    std::cerr << "done.\n";
  }

namespace {
  void AddIntervalToRasterization(int frame,
                                  int y,
                                  int left_x,
                                  int right_x,
                                  int region_id,
                                  const RegionInfoPtrMap& map) {
    // Look up region with id.
    RegionInfoPtrMap::const_iterator region_iter = map.find(region_id);
    ASSERT_LOG(region_iter != map.end()) << "Region not found in map.";

    RegionInformation* ri = region_iter->second;

    // Current frame-slice present?
    if (ri->raster->empty() || ri->raster->back().first < frame) {
      ri->raster->push_back(
          std::make_pair(frame, shared_ptr<Rasterization>(new Rasterization())));
    }

    Rasterization* raster = ri->raster->back().second.get();
    ScanInterval* scan_inter = raster->add_scan_inter();
    scan_inter->set_y(y);
    scan_inter->set_left_x(left_x);
    scan_inter->set_right_x(right_x);
  }

  void ThinStructureSwap(const int n8_offsets[8],
                         const int n4_offsets[4],
                         int* region_ptr,
                         hash_map<int, int>* size_adjust_map) {
    int num_neighbors = 0;
    const int region_id = *region_ptr;
    for (int k = 0; k < 8; ++k) {
      num_neighbors += (region_ptr[n8_offsets[k]] == region_id);
    }

    if (num_neighbors <= 2) {
      // Deemed a thin structure.
      vector<int> n4_vote;
      int swap_to = -1;
      // Compute N4-based swap.
      for (int k = 0; k < 4; ++k) {
        int n_id = region_ptr[n4_offsets[k]];
        if(n_id != -1 && n_id != region_id && !InsertSortedUniquely(n_id, &n4_vote)) {
          swap_to = n_id;
        }
      }

      if (swap_to >= 0) {
        // Perform swap.
        --(*size_adjust_map)[*region_ptr];
        ++(*size_adjust_map)[swap_to];
        *region_ptr = swap_to;

        // Recompute neighbors.
        int* neighbors[2];
        int n_id = 0;
        for (int k = 0; k < 8; ++k) {
          if (region_ptr[n8_offsets[k]] == region_id) {
            neighbors[n_id++] = region_ptr + n8_offsets[k];
          }
        }

        // Propagate to neighbors.
        for (int n = 0; n < num_neighbors; ++n) {
          ThinStructureSwap(n8_offsets, n4_offsets, neighbors[n], size_adjust_map);
        }
      }
    }
  }
}  // namespace.

  void DenseSegmentationGraph::ThinStructureSuppression(
      cv::Mat* id_image,
      hash_map<int, int>* size_adjust_map) {
    // Traverse image and find thin structures (connected to only two neighbors).
    const int lda = id_image->step1();
    const int n8_offsets[8] = { -lda - 1, -lda, -lda + 1,
                                      -1,              1,
                                 lda - 1, lda, lda + 1 };
    const int n4_offsets[8] = { -lda, -1, 1, lda };

    for (int i = 0; i < frame_height_ - 1; ++i) {
      int* region_ptr = id_image->ptr<int>(i);
      for (int j = 0; j < frame_width_; ++j, ++region_ptr) {
        ThinStructureSwap(n8_offsets, n4_offsets, region_ptr, size_adjust_map);
      }
    }
  }

  void DenseSegmentationGraph::EnforceN4Connectivity(
      cv::Mat* id_image,
      hash_map<int, int>* size_adjust_map) {
    ASSURE_LOG(size_adjust_map);
    const int lda = id_image->step1();
    for (int i = 0; i < frame_height_ - 1; ++i) {
      int* region_ptr = id_image->ptr<int>(i);
      for (int j = 0; j < frame_width_; ++j, ++region_ptr) {
        const int region_id = *region_ptr;
        if (region_ptr[lda - 1] == region_id &&
            region_ptr[-1] != region_id &&
            region_ptr[lda] != region_id) {
          --(*size_adjust_map)[region_ptr[lda]];
          ++(*size_adjust_map)[region_id];
          region_ptr[lda] = region_id;
        }

        if (region_ptr[lda + 1] == region_id &&
            region_ptr[1] != region_id &&
            region_ptr[lda] != region_id) {
          --(*size_adjust_map)[region_ptr[lda]];
          ++(*size_adjust_map)[region_id];
          region_ptr[lda] = region_id;
        }
      }
    }
  }

  void DenseSegmentationGraph::ObtainScanlineRepFromResults(
      const RegionInfoPtrMap& region_map,
      bool remove_thin_structure,
      bool enforce_n4_connections) {
    // Allocate rasterizations.
    for (RegionInfoPtrMap::const_iterator region = region_map.begin();
         region != region_map.end();
         ++region) {
      region->second->raster.reset(new Rasterization3D);
    }

    // Flag boundary with -1.
    region_ids_.row(0).setTo(-1);
    region_ids_.row(frame_height_ + 1).setTo(-1);
    region_ids_.col(0).setTo(-1);
    region_ids_.col(frame_width_ + 1).setTo(-1);

    cv::Mat id_view(region_ids_, cv::Rect(1, 1, frame_width_, frame_height_));

    // Maps region representative to change in region area due to thin structure
    // suppression and n4 connectivity.
    hash_map<int, int> size_adjust_map;
    // Traverse all frames.
    for (int t = 0; t < num_frames_; ++t) {
      // Base region index for current frame.
      const int base_idx = frame_width_ * frame_height_ * t;

      vector<int>::const_iterator constrained_iter =
        std::lower_bound(constrained_slices_.begin(),
                         constrained_slices_.end(),
                         t);
      bool is_constrained_frame = constrained_iter != constrained_slices_.end() &&
                                  *constrained_iter == t;

      for (int i = 0, idx = base_idx; i < frame_height_; ++i) {
        int* region_ptr = id_view.ptr<int>(i);
        for (int j = 0; j < frame_width_; ++j, ++idx, ++region_ptr) {
          *region_ptr = GetRegion(idx)->my_id;
        }
      }

      // Assure constrained frames yield exactly the same result.
      if (!is_constrained_frame) {
        if (remove_thin_structure) {
          ThinStructureSuppression(&id_view, &size_adjust_map);
        }

        if (enforce_n4_connections) {
          EnforceN4Connectivity(&id_view, &size_adjust_map);
        }
      }

      // Traverse region_id_image.
      for (int i = 0, idx = base_idx; i < frame_height_; ++i) {
        const int* region_ptr = id_view.ptr<int>(i);
        int prev_id = *(region_ptr++);
        int left_x = 0;
        for (int j = 1; j < frame_width_; ++j, ++region_ptr) {
          const int curr_id = *region_ptr;
          // Test for new interval.
          if (prev_id != curr_id) {
            AddIntervalToRasterization(t, i, left_x, j - 1, prev_id, region_map);
            // Reset current interval information.
            left_x = j;
            prev_id = curr_id;
          }

          // Add interval when end of scanline is reached.
          if (j + 1 == frame_width_) {
            AddIntervalToRasterization(t, i, left_x, j, prev_id, region_map);
          }
        }
      }
    }  // end image traversal.

    // Adjust sizes.
    for (hash_map<int, int>::const_iterator size_adjust = size_adjust_map.begin();
         size_adjust != size_adjust_map.end();
         ++size_adjust) {
      RegionInfoPtrMap::const_iterator pos = region_map.find(size_adjust->first);
      ASSERT_LOG(pos != region_map.end());
      pos->second->size += size_adjust->second;
    }
  }
}
