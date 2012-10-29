/*
 *  segmentation.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 10/30/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "segmentation.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
#include <ext/hash_set>
using __gnu_cxx::hash_set;
#include <iostream>
#include <list>

#include <google/protobuf/repeated_field.h>
using google::protobuf::RepeatedPtrField;
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

#include "assert_log.h"
#include "image_filter.h"
#include "image_util.h"
#include "region_segmentation_graph.h"
#include "segmentation_util.h"

// opencl
#include "simpleCL/simpleCL.h"

using namespace ImageFilter;

namespace Segment {

  typedef SegmentationDesc::Region2D SegRegion;
  typedef SegmentationDesc::CompoundRegion SegCompRegion;

  Segmentation::Segmentation(float param_k,
                             int frame_width,
                             int frame_height,
                             int chunk_id)
      : param_k_(param_k),
        frame_width_(frame_width),
        frame_height_(frame_height),
        chunk_id_(chunk_id),
        frame_number_(0) {
    num_base_hierarchies_ = 0;
    last_chunk_added_ = -1;
    is_constrained_hierarchical_segmentation_ = false;
    assigned_constrained_ids_ = false;
    assigned_unique_ids_ = false;
  }

  Segmentation::~Segmentation() {
  }

  void Segmentation::OverSegmentationSizeHint(int chunk_size) {
    if (dense_seg_graph_ == NULL) {
      dense_seg_graph_.reset(new DenseSegmentationGraph(param_k_,
                                                        frame_width_,
                                                        frame_height_,
                                                        chunk_size));
      dense_seg_graph_->FrameSizeHint(chunk_size);
    }
  }

  void Segmentation::RunOverSegmentation(int min_region_size,
                                         bool two_stage_segmentation,
                                         bool thin_structure_suppression,
                                         bool n4_connectivity,
                                         bool spatial_cleanup) {
    RegionInfoList* region_list = new RegionInfoList();

    DenseSegmentationGraph* dense_graph = dense_seg_graph_.get();
    ASSERT_LOG(dense_graph);

    vector<int> all_bucket_lists(dense_graph->NumBucketLists());
    for (int i = 0; i < all_bucket_lists.size(); ++i) {
      all_bucket_lists[i] = i;
    }

    // Skip temporal edges in first pass.
    if (two_stage_segmentation) {
      dense_graph->SegmentGraphSpatially();
      dense_graph->SegmentGraph(all_bucket_lists, true, true);
    } else {
      dense_graph->SegmentGraph(all_bucket_lists, true, true);
    }

    if (spatial_cleanup) {
      // TODO(grundman): Spatial cleanup does not account for changes in size!!!
      dense_graph->MergeSmallRegions(min_region_size, all_bucket_lists, false, NULL);
      dense_graph->SpatialCleanupStep(30);
    }

    RegionInfoPtrMap map;
    dense_graph->MergeSmallRegions(min_region_size, all_bucket_lists, true, NULL);
    dense_graph->AssignRegionIdAndDetermineNeighbors(region_list, NULL, &map);

    // Original segmentation order.
    /*
     RegionInfoPtrMap map;
     dense_graph->SegmentGraph();
     dense_graph->MergeSmallRegions(min_region_size);
     dense_graph->AssignRegionIdAndDetermineNeighbors(region_list, NULL, &map);
     */

    dense_graph->ObtainScanlineRepFromResults(map,
                                              thin_structure_suppression,
                                              n4_connectivity);
    // De-allocate dense_graph.
    dense_graph = 0;
    dense_seg_graph_.reset();
    region_infos_.clear();
    region_infos_.push_back(shared_ptr<RegionInfoList>(region_list));

    // Flag empty regions as to be removed.
    // TODO(grundman): Shouldn't be necessary (only due to spatial cleanup).
    ConstrainSegmentationToFrameInterval(0, frame_number_);
  }

  void Segmentation::RunHierarchicalSegmentation(const RegionDistance& distance,
                                                 float level_cutoff_fraction,
                                                 int min_region_num,
                                                 int max_region_num,
                                                 bool reduction) {

    ASSURE_LOG(region_infos_[0] != NULL) << "Nothing to segment";
    LOG(INFO_V1) << "Running hierarchical segmentation with "
                 << frame_number_ << " frames.";

    // Compact remaining histograms.
    for(RegionInfoList::iterator ri = region_infos_[0]->begin();
        ri != region_infos_[0]->end();
        ++ri) {
      (*ri)->PopulatingDescriptorsFinished();
    }

    // Counts number of total hierarchy levels that will be output to SegmentationDesc
    // protobuffer.
    int hierarchy_levels = 0;
    int curr_region_num = region_infos_[0]->size();
    RegionAgglomerationGraph::EdgeWeightMap edge_weight_map;

    while (curr_region_num > min_region_num) {
      // Setup new graph and segment.
      scoped_ptr<RegionAgglomerationGraph> region_graph(
          new RegionAgglomerationGraph(1.0f, 2048, &distance));

      // Setup constraints for this level.
      if (is_constrained_hierarchical_segmentation_) {
        vector<int> parent_constraint_ids;
        hash_map<int, vector<int> > skeleton;
        SetupRegionConstraints(hierarchy_levels, &parent_constraint_ids, &skeleton);
        region_graph->AddRegionEdgesConstrained(
            *region_infos_[hierarchy_levels],
            hierarchy_levels == 0 ? NULL : &edge_weight_map,
            parent_constraint_ids,
            skeleton);
      } else {
        region_graph->AddRegionEdges(*region_infos_[hierarchy_levels],
                                     hierarchy_levels == 0 ? NULL : &edge_weight_map);
      }

      bool shallow_copy_level = false;
      if (hierarchy_levels == 0 && reduction) {
        float cutoff = std::min(1.0f, (float)max_region_num / region_infos_[0]->size());
        region_graph->SegmentGraph(true, cutoff);
      } else {
        if (!region_graph->SegmentGraph(false, level_cutoff_fraction)) {
          // If no merge at all, warn and break.
          LOG(ERROR) << "No merge possible for current cut_off_fraction. "
                     << "Premature return.";
          break;
        }
      }

      region_infos_.push_back(shared_ptr<RegionInfoList>(new RegionInfoList()));
      if (!shallow_copy_level) {
        region_graph->ObtainSegmentationResult(region_infos_[hierarchy_levels].get(),
                                               region_infos_.back().get(),
                                               &edge_weight_map);
      } else {
        LOG(INFO) << "Shallow copy.";
        for (RegionInfoList::iterator region_info =
                 region_infos_[hierarchy_levels]->begin();
             region_info != region_infos_[hierarchy_levels]->end();
             ++region_info) {
          // Clone from prev. region, keep descriptors and rasterization as shallow copy.
          shared_ptr<RegionInformation> new_info(new RegionInformation(**region_info));
          // Establish parent-child relationship.
          const int region_idx = new_info->index;
          (*region_info)->parent_idx = region_idx;
          new_info->child_idx.reset(new vector<int>(1, region_idx));
          // Add.
          region_infos_.back()->push_back(new_info);
        }
      }

      if (region_infos_[hierarchy_levels + 1]->size() <= 1) {
        LOG(WARNING) << "Merging process resulted in only one region!\n";
      }

      curr_region_num = region_infos_[hierarchy_levels]->size();
      ++hierarchy_levels;

      LOG(INFO_V1) << "Level " << hierarchy_levels << ": "
                   << curr_region_num << " elems\n";
    }
  }

  void Segmentation::InitializeBaseHierarchyLevel(
      const HierarchyLevel& hierarchy_level,
      const DescriptorExtractorList& extractors,
      RegionMapping* input_mapping,
      RegionMapping* output_mapping) {
    typedef hash_map<int, RegionInformation*>::iterator RegionInfoMapIter;
    if (region_infos_.size() != 1) {
      region_infos_.resize(1);
      region_infos_[0].reset(new RegionInfoList());
    }

    // Before we start a new chunk, all regions added in the previous chunk that are
    // not present in the current one can be compacted.
    // First pass for compound regions:
    // Test for each region if it was present in previous chunk -> flag if so
    for (RepeatedPtrField<CompoundRegion>::const_iterator region =
             hierarchy_level.region().begin();
         region != hierarchy_level.region().end();
        ++region) {
      // Present in prev. chunk?
      hash_map<int, bool>::iterator map_pos = regions_added_to_chunk_.find(region->id());
      // Flag as present.
      if (map_pos != regions_added_to_chunk_.end()) {
        map_pos->second = false;
      }
    }

    // Compact all regions in previous chunk that are not present anymore.
    int num_compacted = 0;
    for (hash_map<int, bool>::const_iterator i = regions_added_to_chunk_.begin();
         i != regions_added_to_chunk_.end();
         ++i) {
      // Not present anymore.
      if (i->second == true) {
        region_info_map_[i->first]->PopulatingDescriptorsFinished();
        ++num_compacted;
      }
    }

    LOG(INFO) << "Compacted: " << num_compacted << " regions!\n";

    if (output_mapping) {
      output_mapping->clear();
    }

    // Clear flags.
    regions_added_to_chunk_.clear();

    // For each compound region, create new RegionInformation or
    // add to already existing one (ids and neighbors only, actual image
    // content is filled from Region2D by AddOversegmentation).
    for (RepeatedPtrField<CompoundRegion>::const_iterator region =
           hierarchy_level.region().begin();
         region != hierarchy_level.region().end();
         ++region) {
      const int region_id = region->id();

      // Flag as added.
      regions_added_to_chunk_[region_id] = true;

      // RegionInfo already exists?
      RegionInfoMapIter hash_iter = region_info_map_.find(region_id);
      RegionInformation* ri;
      if (hash_iter == region_info_map_.end()) {
        // This is a new region, create new RegionInformation.
        ri = new RegionInformation;

        ri->index = region_infos_[0]->size();
        ri->size = region->size();
        ri->raster.reset(new Rasterization3D);

        // Setup descriptors.
        for (DescriptorExtractorList::const_iterator extractor_ptr = extractors.begin();
             extractor_ptr != extractors.end();
             ++extractor_ptr) {
          ri->AddRegionDescriptor((*extractor_ptr)->CreateDescriptor());
        }

        // Set counterpart from input_mapping.
        if (input_mapping) {
          hash_map<int, RegionInformation*>::const_iterator counterpart =
              input_mapping->find(region_id);
          // Not guaranteed to be present.
          if (counterpart != input_mapping->end()) {
            ri->counterpart = counterpart->second;
          }
        }

        // Save in base level.
        region_infos_[0]->push_back(shared_ptr<RegionInformation>(ri));
        // Save to map of added regions.
        region_info_map_.insert(std::make_pair(region_id, ri));
      } else {
        // Exisiting region, add neighbors and size.
        ri = hash_iter->second;
        ri->size += region->size();
      }

      if (output_mapping) {
        (*output_mapping)[region_id] = ri;
      }
    } // end loop over compound regions

    // All regions in this chunk and potential overlap are added now.
    // Add neighbors if not existing yet.
    for (RepeatedPtrField<CompoundRegion>::const_iterator region =
         hierarchy_level.region().begin();
         region != hierarchy_level.region().end();
         ++region) {
      RegionInfoMapIter ri_iter = region_info_map_.find(region->id());
      ASSERT_LOG(ri_iter != region_info_map_.end());
      RegionInformation* ri = ri_iter->second;

      const int n_sz = region->neighbor_id_size();
      for (int n = 0; n < n_sz; ++n) {
        // Only add if it does not have our neighbors already.
        int n_id = region->neighbor_id(n);
        RegionInfoMapIter hash_iter = region_info_map_.find(n_id);
        ASSERT_LOG(hash_iter != region_info_map_.end());
        RegionInformation* neighbor_ri = hash_iter->second;
        const int n_idx = neighbor_ri->index;

        vector<int>::iterator n_idx_pos =
            std::lower_bound(ri->neighbor_idx.begin(), ri->neighbor_idx.end(), n_idx);
        if (n_idx_pos == ri->neighbor_idx.end() ||
            *n_idx_pos != n_idx) {
          ri->neighbor_idx.insert(n_idx_pos, n_idx);
        }
      }
    }
    ++num_base_hierarchies_;
  }

  void Segmentation::AddOverSegmentation(const SegmentationDesc& desc,
                                         const DescriptorExtractorList& extractors) {
    typedef hash_map<int, RegionInformation*>::iterator RegionInfoMapIter;

    // Add regions of desc to current set of regions.
    const RepeatedPtrField<SegRegion>& regions = desc.region();
    int region_index = 0;

    // Add Region2D (slices) to corresponding RegionInformation.
    for (RepeatedPtrField<SegRegion>::const_iterator r = regions.begin();
         r != regions.end();
         ++r, ++region_index) {
      const int region_id = r->id();

      // Needs to have corresponding RegionInformation at this point.
      RegionInfoMapIter hash_iter = region_info_map_.find(region_id);
      ASSURE_LOG(hash_iter != region_info_map_.end()) << "Region "
          << region_id << " should be present here! chunk: " << chunk_id_;
      RegionInformation* ri = hash_iter->second;

      // This ScanlineRep should not present at current frame.
      if (ri->raster->size() != 0) {
        ASSURE_LOG(ri->raster->back().first < frame_number_)
          << "Rasterization slice inconsistency.";
      }

      ri->raster->push_back(
          std::make_pair(frame_number_, shared_ptr<Rasterization>(new Rasterization())));
      ri->raster->back().second->CopyFrom(r->raster());

      // Fill descriptors.
      int num_descriptors = ri->NumDescriptors();
      ASSURE_LOG(num_descriptors == extractors.size());
      for (int d = 0; d < num_descriptors; ++d) {
        ri->GetDescriptorAtIdx(d)->AddFeatures(r->raster(),
                                               *extractors[d],
                                               frame_number_);
      }
    } // end region traversal.
    ++frame_number_;
  }

  void Segmentation::AddOverSegmentationConnection(int region_id_1,
                                                   int region_id_2,
                                                   int match_id_1,
                                                   int match_id_2,
                                                   float strength) {
    typedef hash_map<int, RegionInformation*>::iterator RegionInfoMapIter;
    ASSERT_LOG(strength >= 0 && strength <= 1);

    // Look up the two regions in the map.
    RegionInformation* ri_1 = 0;
    RegionInformation* ri_2 = 0;

    // Exists already in region_infos?
    RegionInfoMapIter hash_iter = region_info_map_.find(region_id_1);

    ASSURE_LOG(hash_iter != region_info_map_.end());
    ri_1 = hash_iter->second;

    hash_iter = region_info_map_.find(region_id_2);
    ASSURE_LOG(hash_iter != region_info_map_.end());
    ri_2 = hash_iter->second;

    // Add as explicit neighbors.
    vector<int>::iterator ri_1_insert = std::lower_bound(ri_1->neighbor_idx.begin(),
                                                         ri_1->neighbor_idx.end(),
                                                         ri_2->index);
    ASSERT_LOG(ri_1_insert == ri_1->neighbor_idx.end() ||
               *ri_1_insert != ri_2->index);
    ri_1->neighbor_idx.insert(ri_1_insert, ri_2->index);

    vector<int>::iterator ri_2_insert = std::lower_bound(ri_2->neighbor_idx.begin(),
                                                         ri_2->neighbor_idx.end(),
                                                         ri_1->index);
    ASSERT_LOG(ri_2_insert == ri_2->neighbor_idx.end() ||
               *ri_2_insert != ri_1->index);
    ri_2->neighbor_idx.insert(ri_2_insert, ri_1->index);
    /*
    // Add MatchDescriptors with entries.
    RegionDescriptor* desc_1 = ri_1->ReturnRegionDescriptor(MATCH_DESCRIPTOR);
    if (desc_1 == 0) {
      MatchDescriptor* new_desc = new MatchDescriptor;
      new_desc->Initialize(match_id_1);
      ri_1->AddRegionDescriptor(new_desc);
      desc_1 = new_desc;
    }

    dynamic_cast<MatchDescriptor*>(desc_1)->AddMatch(match_id_2, strength);

    RegionDescriptor* desc_2 = ri_2->ReturnRegionDescriptor(MATCH_DESCRIPTOR);
    if (desc_2 == 0) {
      MatchDescriptor* new_desc = new MatchDescriptor;
      new_desc->Initialize(match_id_2);
      ri_2->AddRegionDescriptor(new_desc);
      desc_2 = new_desc;
    }

    dynamic_cast<MatchDescriptor*>(desc_2)->AddMatch(match_id_1, strength);
     */
  }

  void Segmentation::PullCounterpartSegmentationResult(
      const Segmentation& prev_seg) {
    // Traverse all region information's added so far.
    const int levels = prev_seg.region_infos_.size();

    for (RegionInfoList::const_iterator region_iter = region_infos_[0]->begin();
         region_iter != region_infos_[0]->end();
         ++region_iter) {
      RegionInformation* ri = region_iter->get();
      // Ignore, if counterpart isn't set.
      if (ri->counterpart == NULL) {
        continue;
      }

      ri->constrained_id = ri->counterpart->region_id;

      // Pull hierarchical constraints.
      shared_ptr<vector<int> > constrained_ids(new vector<int>(levels - 1));

      int curr_idx = ri->counterpart->parent_idx;
      for (int l = 1; l < levels; ++l) {
        (*constrained_ids)[l - 1] = (*prev_seg.region_infos_[l])[curr_idx]->region_id;
        curr_idx = (*prev_seg.region_infos_[l])[curr_idx]->parent_idx;
      }

      ri->counterpart_region_ids.swap(constrained_ids);
    }

    is_constrained_hierarchical_segmentation_ = true;
  }

  void Segmentation::ConstrainSegmentationToFrameInterval(int lhs, int rhs) {
    // Traverse over all regions in over-segmentation.
    int num_removed_regions = 0;
    for (RegionInfoList::iterator r_iter = region_infos_[0]->begin();
         r_iter != region_infos_[0]->end();
         ++r_iter) {
      RegionInformation* ri = r_iter->get();
      // Is slice region out of bounds or empty?
      if (ri->raster == NULL || ri->raster->empty() ||
          ri->raster->front().first > rhs ||
          ri->raster->back().first < lhs) {
        ++num_removed_regions;
        ri->region_status = RegionInformation::FLAGGED_FOR_REMOVAL;
      }
    }

    // Process remaining levels.
    for (int level = 1; level < region_infos_.size(); ++level) {
      for (RegionInfoList::iterator region = region_infos_[level]->begin();
           region != region_infos_[level]->end();
           ++region) {
        RegionInformation::RegionStatus status = RegionInformation::FLAGGED_FOR_REMOVAL;
        RegionInformation* ri = region->get();
        for (vector<int>::const_iterator child = ri->child_idx->begin();
             child != ri->child_idx->end();
             ++child) {
          if (region_infos_[level - 1]->at(*child)->region_status ==
              RegionInformation::NORMAL) {
            status = RegionInformation::NORMAL;
            break;
          }
        }
        ri->region_status = status;
      }
    }
  }

  void Segmentation::AdjustRegionAreaToFrameInterval(int lhs, int rhs) {
    // Maps region id to how much we change in region area.
    hash_map<int, int> prev_size_adjust;
    // Process over-segmentation first.
    for (RegionInfoList::iterator r_iter = region_infos_[0]->begin();
         r_iter != region_infos_[0]->end();
         ++r_iter) {
      RegionInformation* ri = r_iter->get();
      int size_increment = 0;
      for (Rasterization3D::const_iterator slice = ri->raster->begin();
           slice != ri->raster->end();
           ++slice) {
        if (slice->first < lhs || slice->first > rhs) {
          size_increment -= RasterizationArea(*slice->second);
        }
      }
      ri->size += size_increment;
      prev_size_adjust[ri->index] = size_increment;
    }

    // Process remaining levels.
    for (int level = 1; level < region_infos_.size(); ++level) {
      hash_map<int, int> curr_size_adjust;
      for (RegionInfoList::iterator region = region_infos_[level]->begin();
           region != region_infos_[level]->end();
           ++region) {
        RegionInformation* ri = region->get();
        int size_increment = 0;
        for (vector<int>::const_iterator child = ri->child_idx->begin();
             child != ri->child_idx->end();
             ++child) {
          size_increment += prev_size_adjust[*child];
        }

        ri->size += size_increment;
        curr_size_adjust[ri->index] = size_increment;
      }
      prev_size_adjust.swap(curr_size_adjust);
    }
  }

  void Segmentation::RetrieveSegmentation2D(SegmentationDesc* desc) {
    // No constraints for 2D segmentation, no descriptor support currently.
    FillSegmentationDesc(0, false, desc);
  }

  void Segmentation::RetrieveSegmentation3D(int frame_number,
                                            bool save_descriptors,
                                            SegmentationDesc* desc) {
    FillSegmentationDesc(frame_number,
                         save_descriptors,
                         desc);
  }

namespace {
  int ConstraintIdFromRegion(const RegionInformation& region,
                             bool use_constraint_ids,
                             int id_offset) {
    if (use_constraint_ids && region.constrained_id >= 0) {
      return region.constrained_id;
    } else {
      return region.index + id_offset;
    }
  }
}  // namespace.

  void Segmentation::AssignUniqueRegionIds(bool use_constrained_ids,
                                           const vector<int>& region_id_offsets,
                                           vector<int>* max_region_ids) {
    assigned_constrained_ids_ = use_constrained_ids;
    assigned_unique_ids_ = true;

    ASSURE_LOG(region_id_offsets.size() >= region_infos_.size());

    if (max_region_ids) {
      ASSURE_LOG(max_region_ids->size() >= region_infos_.size());
    }

    // Traverse levels.
    for (int l = 0; l < region_infos_.size(); ++l) {
      // Traverse regions.
      int max_id = -1;
      for (RegionInfoList::iterator region = region_infos_[l]->begin();
           region != region_infos_[l]->end();
           ++region) {
        (*region)->region_id = ConstraintIdFromRegion(**region,
                                                      use_constrained_ids,
                                                      region_id_offsets[l]);
        max_id = std::max(max_id, (*region)->region_id);
      }

      if (max_region_ids) {
        // Guarantee max id is monotonically increasing.
        (*max_region_ids)[l] = std::max(region_id_offsets[l], max_id + 1);
      }
    }
  }

  void Segmentation::DiscardBottomLevel() {
    // Clear children in next level.
    for (RegionInfoList::iterator region = region_infos_[1]->begin();
           region != region_infos_[1]->end();
           ++region) {
      (*region)->child_idx.reset();
    }

    // Discard level 0.
    region_infos_.erase(region_infos_.begin());
  }

  void Segmentation::ForceMergeLevels(int num_levels) {
    for (int l = 0; l < num_levels; ++l) {
      LOG(INFO) << "Force merge level " << l << " of " << num_levels;
      if (region_infos_.size() == 1) {
        break;
      }

      const RegionInfoList& base_list = *(region_infos_[0]);

      // Traverse all super-regions @ level 1.
      for (RegionInfoList::iterator comp_region = region_infos_[1]->begin();
           comp_region != region_infos_[1]->end();
           ++comp_region) {
        // Merge in all children.
        typedef shared_ptr<Rasterization3D> Rasterization3DPtr;
        const int num_children = (*comp_region)->child_idx->size();

        vector<Rasterization3DPtr> raster_to_merge(num_children);
        int child_idx = 0;
        for(vector<int>::const_iterator c = (*comp_region)->child_idx->begin();
            c != (*comp_region)->child_idx->end();
            ++c, ++child_idx) {
          raster_to_merge[child_idx] = base_list[*c]->raster;
        }

        ASSURE_LOG(raster_to_merge.size() > 0);

        // Divide and conquer algorithm.
        vector<Rasterization3DPtr> merged_raster;
        while (raster_to_merge.size() > 1) {
          for (int i = 0, sz = raster_to_merge.size(); i < sz; i += 2) {
            if (i + 1 < sz) {
              Rasterization3DPtr tmp_raster(new Rasterization3D);
              MergeRasterization3D(*raster_to_merge[i],
                                   *raster_to_merge[i + 1],
                                   tmp_raster.get());
              merged_raster.push_back(tmp_raster);
            } else {
              merged_raster.push_back(raster_to_merge[i]);
            }
          }

          raster_to_merge.swap(merged_raster);
          merged_raster.clear();
        }

        ASSURE_LOG(raster_to_merge.size() == 1);
        (*comp_region)->raster = raster_to_merge[0];
      }

      // Discard level 0.
      region_infos_.erase(region_infos_.begin());
    }
    LOG(INFO) << "DONE.";
  }

  void Segmentation::SetupRegionConstraints(int level,
                                            vector<int>* output_ids,
                                            hash_map<int, vector<int> >* skeleton) {
    output_ids->clear();
    output_ids->reserve(region_infos_[level]->size());

    for (RegionInfoList::const_iterator region_iter = region_infos_[level]->begin();
         region_iter != region_infos_[level]->end();
         ++region_iter) {
      // Find one constraint child in base-level, if it exists.
      int child_idx = (*region_iter)->index;
      if (level > 0) {
        for (int l = level; l > 0; --l) {
          // Is there at least one constraint child?
          bool constraint_child_found = false;
          vector<int>& children_ref = *(*region_infos_[l])[child_idx]->child_idx;
          for (vector<int>::const_iterator child_iter = children_ref.begin();
               child_iter != children_ref.end();
               ++child_iter) {
            if ((*region_infos_[l - 1])[*child_iter]->constrained_id >= 0) {
              child_idx = *child_iter;
              constraint_child_found = true;
              break;     // Found one, break out.
            }
          }

          if (!constraint_child_found) {
            // There is no hope to find a constraint base-level child, i.e. this
            // region is unconstraint.
            child_idx = -1;

            // If this happens, it has to occur at the top-level, otherwise there
            // is some kind of inconsistency.
            ASSERT_LOG(l == level) << "Sudden switch from constraint to unconstraint "
                                   << "level. Inconsistency.";

            break;      // break out of level loop.
          }
        }
      } else {
        if ((*region_iter)->constrained_id < 0) {
          child_idx = -1;
        }
      }

      // Pull constraint, if applicable.
      if (child_idx >= 0) {
        RegionInformation* base_level_child = (*region_infos_[0])[child_idx].get();
        if (base_level_child->counterpart_region_ids != NULL) {
          // Only update if enough constraints are available.
          if (level < base_level_child->counterpart_region_ids->size()) {
            const int constraint_id = (*base_level_child->counterpart_region_ids)[level];
            output_ids->push_back(constraint_id);
            (*skeleton)[constraint_id].push_back((*region_iter)->index);
          } else {
            output_ids->push_back(-1);
          }
        } else {
          output_ids->push_back(-1);
        }
      } else {
        output_ids->push_back(-1);
      }
    }
  }

  void Segmentation::AddRegion2DToSegmentationDesc(const RegionInformation& ri,
                                                   int frame_number,
                                                   SegmentationDesc* desc) const {
    if (ri.region_status == RegionInformation::FLAGGED_FOR_REMOVAL) {
      return;
    }

    // Does current RegionInfo has scanline in current frame?
    ASSERT_LOG(ri.raster.get());

    Rasterization3D::const_iterator raster =
        LocateRasterization(frame_number, *ri.raster);
    if (raster == ri.raster->end() || raster->first != frame_number) {
      return;
    }

    // Should not be empty.
    ASSURE_LOG (raster->second->scan_inter_size() != 0);

    // Add info to desc.
    SegRegion* r = desc->add_region();
    r->set_id(ri.region_id);
    r->mutable_raster()->CopyFrom(*raster->second);
    SetShapeDescriptorFromRegion(r);
  }

  void Segmentation::AddCompoundRegionToSegmentationDesc(
      const RegionInformation& ri,
      int level,
      const IdToBoundsMap& prev_bound_map,
      IdToBoundsMap* curr_bound_map,
      HierarchyLevel* hier) const {
    ASSERT_LOG(curr_bound_map);

    if (ri.region_status == RegionInformation::FLAGGED_FOR_REMOVAL) {
      return;
    }

    SegCompRegion* r = hier->add_region();
    r->set_id(ri.region_id);
    r->set_size(ri.size);

    const RegionInfoList& curr_list = *region_infos_[level];
    for (vector<int>::const_iterator n = ri.neighbor_idx.begin();
         n != ri.neighbor_idx.end();
         ++n) {
      if (curr_list[*n]->region_status == RegionInformation::FLAGGED_FOR_REMOVAL) {
        continue;   // not present anymore.
      }

      r->add_neighbor_id(curr_list[*n]->region_id);
    }

    // We might have to sort neighbors.
    if (assigned_constrained_ids_) {
      std::sort(r->mutable_neighbor_id()->begin(), r->mutable_neighbor_id()->end());
    }

    const int levels = region_infos_.size();
    if(level < levels - 1) {
      r->set_parent_id(
          (*region_infos_[level + 1])[ri.parent_idx]->region_id);
    }

    // Set children and determine minimum and maximum frame bounds.
    int min_frame = 1 << 30;
    int max_frame = 0;
    if (level > 0) {       // no children present in oversegmentation.
      ASSURE_LOG(ri.child_idx != NULL);
      for (vector<int>::const_iterator c = ri.child_idx->begin();
           c != ri.child_idx->end();
           ++c) {
        if ((*region_infos_[level - 1])[*c]->region_status ==
            RegionInformation::FLAGGED_FOR_REMOVAL) {
          continue;   // not present anymore.
        }

        r->add_child_id((*region_infos_[level - 1])[*c]->region_id);

        // Update bounds.
        IdToBoundsMap::const_iterator child_bound_pos =
            prev_bound_map.find(*c);
        ASSURE_LOG(child_bound_pos != prev_bound_map.end())
            << "Child bound " << *c << " not found.";

        min_frame = std::min(min_frame, child_bound_pos->second.first);
        max_frame = std::max(max_frame, child_bound_pos->second.second);
      }

      if (assigned_constrained_ids_) {
        std::sort(r->mutable_child_id()->begin(), r->mutable_child_id()->end());
      }
    } else {
      // Populate and write frame bounds.
      ASSERT_LOG(ri.raster);
      min_frame = ri.raster->front().first;
      max_frame = ri.raster->back().first;
    }

    // Set bounds and save for next level.
    r->set_start_frame(min_frame);
    r->set_end_frame(max_frame);
    (*curr_bound_map)[ri.index] = std::make_pair(min_frame, max_frame);
  }

  void Segmentation::FillSegmentationDesc(int frame_number,
                                          bool save_descriptors,
                                          SegmentationDesc* desc) {
    // If no explicit assignment requested, use region's index as region_id.
    if (!assigned_unique_ids_) {
      AssignUniqueRegionIds(false, vector<int>(1, 0), NULL);
    }

    const RegionInfoList& curr_list = *region_infos_[0];

    const int levels = region_infos_.size();

    desc->set_frame_width(frame_width_);
    desc->set_frame_height(frame_height_);
    desc->set_chunk_id(chunk_id_);

    // Add 2d oversegmenation regions at current frames.
    for (RegionInfoList::const_iterator ri_iter = curr_list.begin();
         ri_iter != curr_list.end();
         ++ri_iter) {
      AddRegion2DToSegmentationDesc(**ri_iter,
                                    frame_number,
                                    desc);
    }

    // Sort regions by id.
    if (assigned_constrained_ids_) {
      SortRegions2DById(desc);
    }

    // Save descriptors only in first frame if requested.
    if (frame_number == 0 && save_descriptors) {
      for (RegionInfoList::const_iterator i = curr_list.begin();
           i != curr_list.end();
           ++i) {
        const RegionInformation& ri = **i;
        if (ri.region_status == RegionInformation::FLAGGED_FOR_REMOVAL) {
          continue;
        }

        RegionFeatures* features = desc->add_features();
        features->set_id(ri.region_id);
        ri.OutputRegionFeatures(features);
      }
    }

    // Add rest of levels. Write hierarchy only for first frame.
    if (frame_number == 0) {
      IdToBoundsMap prev_bound_map;
      IdToBoundsMap curr_bound_map;
      for (int l = 0; l < levels; ++l) {
        const RegionInfoList& hier_list = *region_infos_[l];
        HierarchyLevel* hier = desc->add_hierarchy();
        curr_bound_map.clear();

        for (RegionInfoList::const_iterator ri_iter = hier_list.begin();
             ri_iter != hier_list.end();
             ++ri_iter) {
          AddCompoundRegionToSegmentationDesc(**ri_iter,
                                              l,
                                              prev_bound_map,
                                              &curr_bound_map,
                                              hier);
        }

        prev_bound_map.swap(curr_bound_map);

        if (assigned_constrained_ids_) {
          SortCompoundRegionsById(desc, l);
        }
      }  // end hierarchy level traversal.
    }  // end write hierarchy only on first frame.
  }
}
