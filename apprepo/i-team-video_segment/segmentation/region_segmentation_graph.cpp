/*
 *  region_segmentation_graph.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "region_segmentation_graph.h"
#include "image_util.h"

namespace Segment {

RegionSegmentationGraph::RegionSegmentationGraph(float param_k)
    : FastSegmentationGraph(param_k, 1.001, 2048, 5) {
}

void RegionSegmentationGraph::AddRegionEdges(const RegionDistance& distance,
                                             const RegionInfoList& list) {
  AddRegionEdgesImpl(distance, list, vector<int>(list.size(), -1));
}

void RegionSegmentationGraph::AddRegionEdgesConstrained(
    const RegionDistance& distance,
    const RegionInfoList& list,
    const vector<int>& constraint_ids,
    const Skeleton& skeleton) {
  AddRegionEdgesImpl(distance, list, constraint_ids);

  // Add skeleton.
  for (Skeleton::const_iterator skel_iter = skeleton.begin();
       skel_iter != skeleton.end();
       ++skel_iter) {
    int prev_region_idx = skel_iter->second.front();

    for (vector<int>::const_iterator region_idx = skel_iter->second.begin() + 1;
         region_idx != skel_iter->second.end();
         ++region_idx) {
      AddEdge(prev_region_idx, *region_idx, 1e6f);
      prev_region_idx = *region_idx;
    }
  }
}

void RegionSegmentationGraph::AddRegionEdgesImpl(const RegionDistance& distance,
                                                 const RegionInfoList& region_list,
                                                 const vector<int>& constraint_ids) {
  // Allocate nodes.
  const int region_num = region_list.size();
  regions_.reserve(region_num);
  region_merges_.resize(region_num, 1);  // Set merges to 1 for every region.

  // Remember which edges have been added already.
  vector< vector<int> > processed_neighbors(region_num);
  vector<float> descriptor_distances(distance.NumDescriptors());

  int region_idx = 0;
  for (RegionInfoList::const_iterator r = region_list.begin();
       r != region_list.end();
       ++r, ++region_idx) {
    const RegionInformation& ri = **r;
    const int curr_id = ri.index;
    ASSERT_LOG (curr_id < region_num);
    ASSERT_LOG (curr_id == region_idx);

    AddRegion(curr_id, ri.size, param_k_, constraint_ids[region_idx]);

    // Make an edge to each neighbor.
    for (vector<int>::const_iterator n = ri.neighbor_idx.begin();
         n != ri.neighbor_idx.end();
         ++n) {
      const int r1_id = std::min(curr_id, *n);
      const int r2_id = std::max(curr_id, *n);

      // Already processed?
      vector<int>::iterator n_pos = std::lower_bound(processed_neighbors[r1_id].begin(),
                                                     processed_neighbors[r1_id].end(),
                                                     r2_id);
      if (n_pos == processed_neighbors[r1_id].end() ||
          *n_pos != r2_id) {
        // Add Edge.
        ri.DescriptorDistances(*region_list[*n], &descriptor_distances);
        const float region_dist = distance.Evaluate(descriptor_distances);
        AddEdge(curr_id, *n, region_dist);

        // Mark as processed.
        processed_neighbors[r1_id].insert(n_pos, r2_id);
      }
    }
  }
}

RegionSegmentationGraph::Region* RegionSegmentationGraph::MergeRegions(
    Region* rep_1, Region* rep_2, float edge_weight) {
  Region* merged;

  int rep_1_id = rep_1->my_id;
  int rep_2_id = rep_2->my_id;

  // Assign region with fewer merges new id.
  if (region_merges_[rep_1_id] > region_merges_[rep_2_id]) {
    rep_1->sz += rep_2->sz;
    rep_1->constraint_id = std::max(rep_1->constraint_id, rep_2->constraint_id);

    region_merges_[rep_1_id] += region_merges_[rep_2_id];
    rep_2->my_id = rep_1->my_id;
    merged = rep_1;
  } else {
    rep_2->sz += rep_1->sz;
    rep_2->constraint_id = std::max(rep_1->constraint_id, rep_2->constraint_id);

    region_merges_[rep_2_id] += region_merges_[rep_1_id];
    rep_1->my_id = rep_2->my_id;
    merged = rep_2;
  }

  // Update rint.
  merged->rint = edge_weight + param_k_ / merged->sz;  // region_merges_[merged->my_id];
  return merged;
}

void RegionSegmentationGraph::FillChildrenAndHistograms(RegionInfoList& old_regions,
                                                        const RegionInfoPtrMap* map) {
  for (int i = 0, sz = old_regions.size(); i < sz; ++i) {
    RegionInformation* ri_child = old_regions[i].get();
    ASSERT_LOG(ri_child->index == i);

    int parent_id = GetRegion(ri_child->index)->my_id;
    RegionInfoPtrMap::const_iterator parent_iter = map->find(parent_id);
    ASSERT_LOG(parent_iter != map->end());
    RegionInformation* ri_super = parent_iter->second;

    ri_child->parent_idx = ri_super->index;

    // Add Child, merge histograms.
    if (!ri_super->child_idx)
      ri_super->child_idx.reset(new vector<int>);

    // Insert children in sorted manner.
    vector<int>::iterator child_insert_pos =
    std::lower_bound(ri_super->child_idx->begin(), ri_super->child_idx->end(), i);
    ri_super->child_idx->insert(child_insert_pos, i);
    ri_super->MergeDescriptorsFrom(*ri_child);
  }
}

void RegionSegmentationGraph::MergeScanlineRepsAndHistograms(
    const RegionInfoList& old_regions,
    const RegionInfoPtrMap* map) {
  for (RegionInfoList::const_iterator i = old_regions.begin();
       i != old_regions.end();
       ++i) {
    RegionInformation* ri_child = i->get();
    ASSERT_LOG(ri_child->index == i - old_regions.begin());

    int parent_id = GetRegion(ri_child->index)->my_id;
    RegionInformation* ri_super = map->find(parent_id)->second;

    // Merge histograms and scanline representation.
    ri_super->MergeDescriptorsFrom(*ri_child);

    if(!ri_super->raster) {
      ri_super->raster.reset(new Rasterization3D());
    }

    shared_ptr<Rasterization3D> merged_raster(new Rasterization3D());
    MergeRasterization3D(*ri_child->raster, *ri_super->raster, merged_raster.get());
    ri_super->raster.swap(merged_raster);
  }
}

///////////////////////////////////
/// RegionAgglomerationGraph //////
RegionAgglomerationGraph::RegionAgglomerationGraph(float max_weight,
                                                   int num_buckets,
                                                   const RegionDistance* distance)
    : max_weight_(max_weight * 1.01),
      num_buckets_(num_buckets),
      distance_(distance) {
  edge_scale_ = (float)num_buckets / max_weight_;
  edge_buckets_.resize(num_buckets + 1);   // last bucket used for virtual edges.
}

void RegionAgglomerationGraph::AddRegionEdges(const RegionInfoList& list,
                                              const EdgeWeightMap* weight_map) {
  AddRegionEdgesImpl(list,
                     vector<int>(list.size(), -1),   // all regions are unconstrained.
                     weight_map);
}

void RegionAgglomerationGraph::AddRegionEdgesConstrained(
    const RegionInfoList& list,
    const EdgeWeightMap* weight_map,
    const vector<int>& constraint_ids,
    const Skeleton& skeleton) {
  AddRegionEdgesImpl(list, constraint_ids, weight_map);

  // Add skeleton.
  for (Skeleton::const_iterator skel_iter = skeleton.begin();
       skel_iter != skeleton.end();
       ++skel_iter) {
    int prev_region_idx = skel_iter->second.front();

    for (vector<int>::const_iterator region_idx = skel_iter->second.begin() + 1;
         region_idx != skel_iter->second.end();
         ++region_idx) {
      AddEdge(prev_region_idx, *region_idx, 1e6f);   // Add to last bucket.
      prev_region_idx = *region_idx;
    }
  }
}

int RegionAgglomerationGraph::SegmentGraph(bool merge_rasterization,
                                           float cutoff_fraction) {
  LOG(INFO) << "Segmenting graph.";
  merge_rasterization_ = merge_rasterization;

  ASSURE_LOG(cutoff_fraction > 0 &&
             cutoff_fraction <= 1) << "Cutoff fraction expected to be between 0 and 1.";
  int num_merges = regions_.size() * (1.0f - cutoff_fraction);

  // Anticipated number of forced merges from constraining skeleton.
  const int constraint_merges = edge_buckets_.back().size() * cutoff_fraction;

  // Reduce num_merges by additional forced merges due to constraints.
  const int requested_merges = num_merges;
  num_merges -= constraint_merges;

  LOG(INFO) << "Performing " << num_merges << " of requested " << requested_merges;

  // Guarantee that at least one region remains.
  num_merges = std::min<int>(num_merges, regions_.size() - 1);

  // Determines lowest, non-empty bucket.
  int lowest_bucket = 0;
  while (edge_buckets_[lowest_bucket].empty()) {
    ++lowest_bucket;
  }

  int actual_merges = 0;
  for (int merge = 0; merge < num_merges; ++merge) {
    if (lowest_bucket >= num_buckets_) {
      break;
    }

    // Grab lowest cost edge -> perform merge.
    bool merge_performed = false;

    while (!merge_performed) {
      ASSURE_LOG(lowest_bucket < num_buckets_);
      ASSURE_LOG(!edge_buckets_[lowest_bucket].empty());
      list<Edge>::iterator first_edge = edge_buckets_[lowest_bucket].begin();

      // LOG(INFO) << "Edge: " << first_edge->region_1 << " to " << first_edge->region_2
      //           << " at bucket " << lowest_bucket;

      // TODO: Redundant. Remove.
      Region* region_1 = GetRegion(first_edge->region_1);
      Region* region_2 = GetRegion(first_edge->region_2);
      ASSURE_LOG(region_1->id == first_edge->region_1);
      ASSURE_LOG(region_2->id == first_edge->region_2)
          << region_2->id << " vs " << first_edge->region_2;
      ASSURE_LOG(region_1 != region_2) << "Internal edge. Should not be present.";

      if (!AreRegionsMergable(*region_1, *region_2)) {
        // LOG(INFO) << "Regions not mergable.";
        // If regions have different constraints, remove from buckets and flag in
        // EdgePositionMap by pointing to end.
        ASSURE_LOG(edge_position_map_[*first_edge].bucket == lowest_bucket);
        edge_position_map_[*first_edge].iter = edge_buckets_[lowest_bucket].end();
        first_edge = edge_buckets_[lowest_bucket].erase(first_edge);
      } else {
        int min_bucket =
            MergeRegions(region_1, region_2, merge_rasterization) * edge_scale_;
        ++actual_merges;
        // Get next minimum edge.
        if (min_bucket < lowest_bucket) {
          lowest_bucket = min_bucket;
          break;
        }

        first_edge = edge_buckets_[lowest_bucket].begin();  // Edge erased, point to
                                                            // next one in this bucket.
        merge_performed = true;
      }

      if (first_edge == edge_buckets_[lowest_bucket].end()) {
        // Advance to next occupied bucket.
        do {
          ++lowest_bucket;
        } while (lowest_bucket < num_buckets_ && edge_buckets_[lowest_bucket].empty());
        // Delay virtual edge merges to the loop below.
        if (lowest_bucket >= num_buckets_) {
          break;
        }
      }
    }
  }

  // LOG(INFO) << "Skeleton merges.";

  // Perform forced merges from skeleton (last bucket = virtual edges).
  for (list<Edge>::const_iterator edge = edge_buckets_.back().begin();
       edge != edge_buckets_.back().end();
       ++edge) {
    Region* region_1 = GetRegion(edge->region_1);
    Region* region_2 = GetRegion(edge->region_2);
    if (region_1 != region_2) {
      ASSURE_LOG(region_1->constraint_id == region_2->constraint_id &&
                 region_1->constraint_id >= 0);
      MergeRegions(region_1, region_2, merge_rasterization);
      ++actual_merges;
      ++num_merges;
    }
  }

  LOG(INFO) << "Requested merges: " << requested_merges
            << ", performed merges: " << num_merges;

  return actual_merges;
}


void RegionAgglomerationGraph::ObtainSegmentationResult(RegionInfoList* prev_level,
                                                        RegionInfoList* curr_level,
                                                        EdgeWeightMap* weight_map) {
  LOG(INFO) << "Reading result.";

  int child_idx = 0;
  int next_assigned_idx = 0;

  // Assign ids and establish parent - child relationship.
  unordered_set<int> assigned_id;
  vector<int> representative_id;

  for (RegionInfoList::iterator child_region = prev_level->begin();
       child_region != prev_level->end();
       ++child_region, ++child_idx) {
    Region* result_region = GetRegion(child_idx);
    ASSURE_LOG(result_region - &regions_[0] == result_region->id)
        << result_region - &regions_[0];

    unordered_set<int>::iterator result_pos = assigned_id.find(result_region->id);
    if (result_pos == assigned_id.end()) {
      // Do we require a copy?
      if (result_region->region_info.get() == child_region->get()) {
        const RegionInformation& old_info = *result_region->region_info;
        RegionInformation* new_info = new RegionInformation;
        new_info->size = old_info.size;
        new_info->neighbor_idx = old_info.neighbor_idx;
        new_info->MergeDescriptorsFrom(old_info);
        if (merge_rasterization_) {
           new_info->raster.reset(new Rasterization3D(*old_info.raster));
        }

        result_region->region_info.reset(new_info);
      }

      // Assign id, set constraint and allocate child index array.
      result_region->region_info->index = next_assigned_idx++;
      result_region->region_info->constrained_id = result_region->constraint_id;
      result_region->region_info->child_idx.reset(new vector<int>);

      curr_level->push_back(result_region->region_info);
      representative_id.push_back(result_region->id);

      assigned_id.insert(result_region->id);
    }

    RegionInformation* result_info = result_region->region_info.get();
    ASSURE_LOG(result_info->index >= 0) << result_info->index;
    ASSURE_LOG(result_info->child_idx);
    result_info->child_idx->push_back(child_idx);
    (*child_region)->parent_idx = result_info->index;
  }

  if (weight_map) {
    weight_map->clear();
  }

  LOG(INFO) << "Remapping neighbors.";

  // After all representatives have their ids assigned, update neighbors and output
  // already computed edge weights.
  const float inv_edge_scale = 1.0f / edge_scale_;
  for (RegionInfoList::const_iterator region = curr_level->begin();
       region != curr_level->end();
       ++region) {
    RegionInformation* region_info = (*region).get();
    // Map neighbors to indices.
    vector<int> mapped_neighbors;
    for (vector<int>::const_iterator neighbor = region_info->neighbor_idx.begin();
         neighbor != region_info->neighbor_idx.end();
         ++neighbor) {
      Region* neighbor_region = GetRegion(*neighbor);
      int neighbor_idx = neighbor_region->region_info->index;
      ASSERT_LOG(neighbor_idx >= 0);

      if (weight_map) {
        Edge graph_edge(representative_id[region -  curr_level->begin()],
                        neighbor_region->id);
        Edge output_edge(region_info->index, neighbor_idx);
        (*weight_map)[output_edge] =
            inv_edge_scale * edge_position_map_[graph_edge].bucket;
      }

      vector<int>::iterator insert_pos =
          lower_bound(mapped_neighbors.begin(), mapped_neighbors.end(), neighbor_idx);
      if (insert_pos == mapped_neighbors.end() ||
          *insert_pos != neighbor_idx) {
        mapped_neighbors.insert(insert_pos, neighbor_idx);
      }
    }
    region_info->neighbor_idx.swap(mapped_neighbors);
  }
  LOG(INFO) << "DONE.";
}

void RegionAgglomerationGraph::AddRegionEdgesImpl(const RegionInfoList& region_list,
                                                  const vector<int>& constraint_ids,
                                                  const EdgeWeightMap* weight_map) {
  // Allocate nodes.
  const int region_num = region_list.size();
  regions_.reserve(region_num);

  // Determine average load for EdgePositionMap.
  int num_neighbors = 0;
  for (RegionInfoList::const_iterator r = region_list.begin();
       r != region_list.end();
       ++r) {
    num_neighbors += (*r)->neighbor_idx.size();
  }

  int av_neighbors_per_region = std::ceil(num_neighbors / region_list.size());
  LOG(INFO) << "Average neighbors per region: " << av_neighbors_per_region;

  // Reset EdgePositionMap.
  edge_position_map_ = EdgePositionMap(num_neighbors,  // twice anticipated load.
                                       EdgeHasher(av_neighbors_per_region));

  vector<float> descriptor_distances(distance_->NumDescriptors());

  int region_idx = 0;
  for (RegionInfoList::const_iterator r = region_list.begin();
       r != region_list.end();
       ++r, ++region_idx) {
    const RegionInformation& ri = **r;
    const int curr_id = ri.index;
    ASSERT_LOG (curr_id < region_num);
    ASSERT_LOG (curr_id == region_idx);

    AddRegion(curr_id, constraint_ids[region_idx], 1, *r);

    // Introduce an edge to each neighbor.
    for (vector<int>::const_iterator n = ri.neighbor_idx.begin();
         n != ri.neighbor_idx.end();
         ++n) {
      // Only add edge if not present yet.
      if (edge_position_map_.find(Edge(curr_id, *n)) == edge_position_map_.end()) {
        if (weight_map) {
          // Weight has been previously evaluated.
          EdgeWeightMap::const_iterator weight_iter = weight_map->find(Edge(curr_id, *n));
          ASSERT_LOG(weight_iter != weight_map->end());
          AddEdge(curr_id, *n, weight_iter->second);
        } else {
          // Explicitly evaluate distance.
          ri.DescriptorDistances(*region_list[*n], &descriptor_distances);
          const float region_dist = distance_->Evaluate(descriptor_distances);
          AddEdge(curr_id, *n, region_dist);
        }
      }
    }
  }

}

inline void RegionAgglomerationGraph::AddRegion(int id,
                                                int constraint_id,
                                                int size,
                                                shared_ptr<RegionInformation> ptr) {
  regions_.push_back(Region(id, constraint_id, size, ptr));
}

inline bool RegionAgglomerationGraph::AddEdge(int region_1, int region_2, float weight) {
  if (weight > 1.01) {
    ASSURE_LOG(weight > 10) << "Weight: " << weight;
  }
  int bucket = std::min(num_buckets_, (int)(weight * edge_scale_));
  const Edge e(region_1, region_2);

  list<Edge>::iterator iter = edge_buckets_[bucket].end();
  bool mergable = AreRegionsMergable(regions_[region_1], regions_[region_2]);
  if (mergable) {
    edge_buckets_[bucket].push_back(e);
    --iter;
  }

  // Only insert non-virtual edges to EdgePositionMap.
  if (bucket != num_buckets_) {
    pair<EdgePositionMap::iterator, bool> insert_pos =
        edge_position_map_.insert(std::make_pair(e, EdgePosition(iter, bucket)));

    ASSURE_LOG(insert_pos.second == true)
      << "Edge " << region_1 << " to " << region_2 << " exists.";
  } else {
    ASSURE_LOG(mergable) << regions_[region_1].constraint_id << " and "
                         << regions_[region_2].constraint_id;
  }

  return mergable;
}

inline RegionAgglomerationGraph::Region*
RegionAgglomerationGraph::GetRegion(int region_id) {
  ASSERT_LOG(region_id >=0);
  ASSERT_LOG(region_id < regions_.size());

  // Note, most regions directly point to root, so this will cause a single region
  // lookup.
  Region* r = &regions_[region_id];
  const int parent_id = r->id;
  Region* parent = &regions_[parent_id];

  if (parent->id == parent_id) {
    return parent;
  } else {
    // Path compression.
    parent = GetRegion(parent_id);
    r->id = parent->id;
    return parent;
  }
}

float RegionAgglomerationGraph::MergeRegions(Region* rep_1,
                                             Region* rep_2,
                                             bool merge_rasterization) {
  const RegionInformation& rep_info_1 = *rep_1->region_info;
  const RegionInformation& rep_info_2 = *rep_2->region_info;

  const int region_1 = rep_1->id;
  const int region_2 = rep_2->id;

  // LOG(INFO) << "Merging " << region_1 << " and " << region_2;

  // Merge neighbors.
  vector<int> merged_neighbors;
  merged_neighbors.reserve(rep_info_1.neighbor_idx.size() +
                           rep_info_2.neighbor_idx.size());
  // TODO: remove.
  vector<int> removed_neighbors;
  for (vector<int>::const_iterator n = rep_info_1.neighbor_idx.begin();
       n != rep_info_1.neighbor_idx.end();
       ++n) {
    // Map to representative.
    const int neighbor_idx = GetRegion(*n)->id;

    // Remove edge.
    EdgePositionMap::iterator edge_position =
        edge_position_map_.find(Edge(region_1, neighbor_idx));

    if (edge_position == edge_position_map_.end()) {
      // Edge was removed in previous iteration. Check.
      ASSURE_LOG(std::find(removed_neighbors.begin(),
                           removed_neighbors.end(),
                           neighbor_idx) != removed_neighbors.end())
      << region_1 << " and " << neighbor_idx;
      continue;
    }

    // Only remove, if not an unmergable edge.
    if (edge_position->second.iter != edge_buckets_[edge_position->second.bucket].end()) {
      edge_buckets_[edge_position->second.bucket].erase(edge_position->second.iter);
    }

    // Remove from map as well.
    edge_position_map_.erase(edge_position);
    removed_neighbors.push_back(neighbor_idx);

    // Skip the two involved regions.
    if (neighbor_idx == region_2) {
      continue;
    }

    // Only keep unique neighbors.
    vector<int>::iterator insert_pos =
        lower_bound(merged_neighbors.begin(), merged_neighbors.end(), neighbor_idx);
    if (insert_pos == merged_neighbors.end() ||
        *insert_pos != neighbor_idx) {
      merged_neighbors.insert(insert_pos, neighbor_idx);
    }
  }

  removed_neighbors.clear();
  for (vector<int>::const_iterator n = rep_info_2.neighbor_idx.begin();
       n != rep_info_2.neighbor_idx.end();
       ++n) {
    const int neighbor_idx = GetRegion(*n)->id;

    // Remove edge.
    EdgePositionMap::iterator edge_position =
        edge_position_map_.find(Edge(region_2, neighbor_idx));

    if (edge_position == edge_position_map_.end()) {
      // Edge was removed in previous iteration. Check.
      ASSURE_LOG(neighbor_idx == region_1 ||
                 std::find(removed_neighbors.begin(),
                           removed_neighbors.end(),
                           neighbor_idx) != removed_neighbors.end());
      continue;
    }

    if (edge_position->second.iter != edge_buckets_[edge_position->second.bucket].end()) {
      edge_buckets_[edge_position->second.bucket].erase(edge_position->second.iter);
    }

    // Remove from map as well.
    edge_position_map_.erase(edge_position);
    removed_neighbors.push_back(neighbor_idx);

    // Skip the two involved regions.
    if (neighbor_idx == region_1) {
      continue;
    }

    // Only keep unique neighbors.
    vector<int>::iterator insert_pos =
        lower_bound(merged_neighbors.begin(), merged_neighbors.end(), neighbor_idx);
    if (insert_pos == merged_neighbors.end() ||
        *insert_pos != neighbor_idx) {
      merged_neighbors.insert(insert_pos, neighbor_idx);
    }
  }

  Region* merged = rep_1->sz > rep_2->sz ? rep_1 : rep_2;
  merged->sz = rep_1->sz + rep_2->sz;
  rep_1->id = merged->id;
  rep_2->id = merged->id;
  merged->constraint_id = std::max(rep_1->constraint_id, rep_2->constraint_id);

  shared_ptr<RegionInformation> new_info(new RegionInformation());
  new_info->size = rep_info_1.size + rep_info_2.size;
  new_info->neighbor_idx.swap(merged_neighbors);

  // Merge descriptors.
  new_info->MergeDescriptorsFrom(rep_info_1);
  new_info->MergeDescriptorsFrom(rep_info_2);

  // Merge rasterizations, if requested.
  if (merge_rasterization) {
    new_info->raster.reset(new Rasterization3D());
    MergeRasterization3D(*rep_info_1.raster,
                         *rep_info_2.raster,
                         new_info->raster.get());
  }

  // Re-evaluate all incident edges.
  vector<float> descriptor_distances(distance_->NumDescriptors());
  float min_dist = 1e6;
  for (vector<int>::const_iterator neighbor_idx = new_info->neighbor_idx.begin();
       neighbor_idx != new_info->neighbor_idx.end();
       ++neighbor_idx) {
    // Already mapped to index, no GetRegion necessary.
    const Region& neighbor = regions_[*neighbor_idx];
    ASSERT_LOG(neighbor.id == *neighbor_idx);
    new_info->DescriptorDistances(*neighbor.region_info, &descriptor_distances);
    const float region_dist = distance_->Evaluate(descriptor_distances);
    if (AddEdge(merged->id, *neighbor_idx, region_dist)) {
      min_dist = std::min(min_dist, region_dist);
    }
  }

  merged->region_info = new_info;
  return min_dist;
}

inline bool RegionAgglomerationGraph::AreRegionsMergable(const Region& region_1,
                                                         const Region& region_2) {
  return region_1.constraint_id < 0 ||
         region_2.constraint_id < 0 ||
         region_1.constraint_id == region_2.constraint_id;
}

}  // namespace Segment.
