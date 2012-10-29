/*
 *  segmentation_graph.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "segmentation_graph.h"
#include <algorithm>
#include <iostream>

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

namespace Segment {

  FastSegmentationGraph::FastSegmentationGraph(float param_k,
                                               float max_weight,
                                               int num_buckets,
                                               int skip_buckets,
                                               int num_bucket_lists)
      : param_k_(param_k),
        max_weight_(max_weight),
        num_buckets_(num_buckets),
        skip_buckets_(skip_buckets) {
    bucket_lists_.resize(num_bucket_lists);
    for (int i = 0; i < num_bucket_lists; ++i) {
      // Last bucket is used for hard constraints (no-merges).
      bucket_lists_[i].resize(num_buckets + 1);
    }

    ASSURE_LOG(num_buckets > skip_buckets);
    scale_ = num_buckets / (max_weight + 1e-6);
  }

  void FastSegmentationGraph::SegmentGraph() {
    vector<int> all_bucket_lists(bucket_lists_.size());
    for (int i = 0; i < bucket_lists_.size(); ++i) { all_bucket_lists[i] = i; }
    SegmentGraph(all_bucket_lists, true, true);
  }

  void FastSegmentationGraph::SegmentGraph(const vector<int>& bucket_list_ids,
                                           bool use_skip_buckets,
                                           bool force_constraints) {
    // Run over buckets.
    // Record edges not considered for merging.
    std::cerr << "Segmenting graph ...";
    int bucket_idx = 0;
    const float inv_scale = 1.0 / scale_;

    num_forced_merges_ = 0;
    num_regular_merges_ = 0;
    num_small_region_merges_ = 0;

    // Do blind merges for the lowest skip_buckets of buckets.
    if (use_skip_buckets) {
      for (int bucket_idx = 0; bucket_idx < skip_buckets_; ++bucket_idx) {
        const float weight = bucket_idx * inv_scale;
        for (vector<int>::const_iterator bucket_list_idx = bucket_list_ids.begin();
             bucket_list_idx != bucket_list_ids.end();
             ++bucket_list_idx) {
          EdgeList remaining_edges;

          for (EdgeList::const_iterator e =
                   bucket_lists_[*bucket_list_idx][bucket_idx].begin(),
                   end_e = bucket_lists_[*bucket_list_idx][bucket_idx].end();
               e != end_e; ++e) {
            Region* rep_1 = GetRegion(e->region_1);
            Region* rep_2 = GetRegion(e->region_2);

            if (rep_1 != rep_2) {
              if (AreRegionsMergable(rep_1, rep_2)) {
                MergeRegions(rep_1, rep_2, weight);
                ++num_forced_merges_;
              } else {
                remaining_edges.push_back(*e);
              }
            }
          }

          bucket_lists_[*bucket_list_idx][bucket_idx].swap(remaining_edges);
        }
      }
    }

    const int start_bucket = use_skip_buckets ? skip_buckets_ : 0;
    for (int bucket_idx = start_bucket; bucket_idx < num_buckets_; ++bucket_idx) {
      for (vector<int>::const_iterator bucket_list_idx = bucket_list_ids.begin();
           bucket_list_idx != bucket_list_ids.end();
           ++bucket_list_idx) {
        const float weight = bucket_idx * inv_scale;

        for (EdgeList::const_iterator e =
             bucket_lists_[*bucket_list_idx][bucket_idx].begin(),
             end_e = bucket_lists_[*bucket_list_idx][bucket_idx].end();
             e != end_e; ++e) {
          Region* rep_1 = GetRegion(e->region_1);
          Region* rep_2 = GetRegion(e->region_2);

          if (rep_1 != rep_2) {
            // Test only in unconstraint or single constraint case.
            if (rep_1->constraint_id < 0 || rep_2->constraint_id < 0) {
              if (weight <= rep_1->rint && weight <= rep_2->rint) {
                MergeRegions(rep_1, rep_2, weight);
                ++num_regular_merges_;
              }
            } else if (rep_1->constraint_id == rep_2->constraint_id) {
              // Force merge.
              MergeRegions(rep_1, rep_2, weight);
              ++num_forced_merges_;
            }
            // else: both are constraint but have different ids: don't merge!
          }
        }
      }  // end bucket lists ids.
    }  // end bucket idx.

    LOG(INFO_V1) << "Forced merges " << num_forced_merges_
                 << " | Regular merges " << num_regular_merges_ << "\n";

    if (force_constraints) {
      // Last bucket process. Check only constrain ids.
      for (vector<int>::const_iterator bucket_list_idx = bucket_list_ids.begin();
           bucket_list_idx != bucket_list_ids.end();
           ++bucket_list_idx) {
        for (EdgeList::const_iterator e =
             bucket_lists_[*bucket_list_idx][num_buckets_].begin(),
             end_e = bucket_lists_[*bucket_list_idx][num_buckets_].end();
             e != end_e; ++e) {
          Region* rep_1 = GetRegion(e->region_1);
          Region* rep_2 = GetRegion(e->region_2);

          if (rep_1 != rep_2) {
            ASSERT_LOG(rep_1->constraint_id >=0 && rep_2->constraint_id >=0);
            if (rep_1->constraint_id == rep_2->constraint_id) {
              MergeRegions(rep_1, rep_2, 0);
              ++num_forced_merges_;
            }
          }
        }
      }
    }

    std::cerr << " done.\n";
  }

  void FastSegmentationGraph::MergeSmallRegions(int min_region_sz,
                                                const vector<int>& bucket_list_ids,
                                                bool discard_redundant_edges,
                                                const vector<int>* discard_bucket_lists) {
    const float inv_scale = 1.0 / scale_;
    int merges = 0;

    // Remove all edges but the lowest cost edges between two regions,
    // if discard_redundant_edges is set to true.
    vector< vector<int> > neighbors(regions_.size());

    // Convert discard_bucket_lists to vector of flags.
    vector<bool> is_discard_bucket_list(
        bucket_lists_.size(), discard_bucket_lists == NULL ? true : false);

    if (discard_bucket_lists) {
      for (vector<int>::const_iterator bl = discard_bucket_lists->begin();
           bl != discard_bucket_lists->end();
           ++bl) {
        if (*bl > bucket_lists_.size()) {
          LOG(WARNING) << "Passed neighbor_eval_bucket_list_ids is out of bounds";
          continue;
        }

        is_discard_bucket_list[*bl] = true;
      }
    }

    std::cerr << "Merge small regions ...";

    for (int bucket_idx = 0; bucket_idx < num_buckets_; ++bucket_idx) {
      const float weight = bucket_idx * inv_scale;
      for (vector<int>::const_iterator bucket_list_idx = bucket_list_ids.begin();
           bucket_list_idx != bucket_list_ids.end();
           ++bucket_list_idx) {
        EdgeList remaining_edges;

        for (EdgeList::const_iterator e =
             bucket_lists_[*bucket_list_idx][bucket_idx].begin(),
             end_e = bucket_lists_[*bucket_list_idx][bucket_idx].end();
             e != end_e; ++e) {
          // Get Regions representatives.
          Region* rep_1 = GetRegion(e->region_1);
          Region* rep_2 = GetRegion(e->region_2);

          if (rep_1 != rep_2) {
            if (AreRegionsMergable(rep_1, rep_2) &&
                (rep_1->sz < min_region_sz || rep_2->sz < min_region_sz)) {
              MergeRegions(rep_1, rep_2, weight);
              ++num_small_region_merges_;
              ++merges;
            } else if (discard_redundant_edges) {
              int r1_id = rep_1->my_id;
              int r2_id = rep_2->my_id;

              // The regions should not have the same constrain id larger than zero.
              ASSERT_LOG(rep_1->constraint_id < 0 || rep_2->constraint_id < 0 ||
                         rep_1->constraint_id != rep_2->constraint_id);

              if (r1_id > r2_id) {
                std::swap(r1_id, r2_id);
              }

              // Already connected?
              vector<int>::iterator n_pos = std::lower_bound(neighbors[r1_id].begin(),
                                                             neighbors[r1_id].end(),
                                                             r2_id);
              if (n_pos == neighbors[r1_id].end() ||
                  *n_pos != r2_id) {
                // Only add neighbors if current bucket_list is set by
                // discard_bucket_lists.
                if (is_discard_bucket_list[*bucket_list_idx]) {
                  // Add neighbor
                  neighbors[r1_id].insert(n_pos, r2_id);
                }
                // Add edge.
                remaining_edges.push_back(*e);
              }
            }
          }
        }  // end edge traversal.

        if (discard_redundant_edges) {
          bucket_lists_[*bucket_list_idx][bucket_idx].swap(remaining_edges);
        }
      }
    }


    std::cerr << "done.\n";
    const float total_merges = num_forced_merges_ +
                               num_regular_merges_ +
                               num_small_region_merges_;
    if(total_merges != 0) {
      LOG(INFO_V1) << "Total merges: " << total_merges << "\n"
                   << "Forced merges " << num_forced_merges_ << "("
                   << num_forced_merges_ / total_merges * 100.f << "%), "
                   << "Regular merges " << num_regular_merges_ << "("
                   << num_regular_merges_ / total_merges * 100.f << "%), "
                   << "Small region merges " << num_small_region_merges_ << "("
                   << num_small_region_merges_ / total_merges * 100.f<< "%)";
    }
  }

  void FastSegmentationGraph::AssignRegionIdAndDetermineNeighbors(
      RegionInfoList* region_list,
      const vector<int>* neighbor_eval_bucket_list_ids,
      RegionInfoPtrMap* map) {
    typedef RegionInfoPtrMap::iterator hash_map_iter_t;

    // Traverse all remaining edges.
    max_region_id_ = 0;

    // Convert bucket_list_idx to vector of flags.
    vector<bool> consider_for_neighbor_info(
        bucket_lists_.size(), neighbor_eval_bucket_list_ids == NULL ? true : false);

    if (neighbor_eval_bucket_list_ids) {
      for (vector<int>::const_iterator nb = neighbor_eval_bucket_list_ids->begin();
           nb != neighbor_eval_bucket_list_ids->end();
           ++nb) {
        if (*nb > bucket_lists_.size()) {
          LOG(WARNING) << "Passed neighbor_eval_bucket_list_ids is out of bounds";
          continue;
        }

        consider_for_neighbor_info[*nb] = true;
      }
    }

    // Last bucket is just for marker edges and NEEDS to be ignored.
    for (int bucket_idx = 0; bucket_idx < num_buckets_; ++bucket_idx) {
      for (int bucket_list_idx = 0;
           bucket_list_idx < bucket_lists_.size();
           ++bucket_list_idx) {
        const bool neighbor_eval = consider_for_neighbor_info[bucket_list_idx];

        for (EdgeList::const_iterator e =
             bucket_lists_[bucket_list_idx][bucket_idx].begin(),
             end_e = bucket_lists_[bucket_list_idx][bucket_idx].end();
             e != end_e; ++e) {
          const Region* r1 = GetRegion(e->region_1);
          const Region* r2 = GetRegion(e->region_2);

          const int r1_id = r1->my_id;
          const int r2_id = r2->my_id;

          if (r1_id == r2_id) {
            continue;
          }

          RegionInformation* r1_info, *r2_info;

          // Find region 1 in hash_map, insert if not present.
          hash_map_iter_t hash_iter = map->find(r1->my_id);
          if (hash_iter == map->end()) {  // Does not exists yet --> insert.
            RegionInformation* new_region_info = new RegionInformation;
            new_region_info->index = max_region_id_++;
            new_region_info->size = r1->sz;
            new_region_info->constrained_id = r1->constraint_id;

            // Add to list and hash_map.
            region_list->push_back(shared_ptr<RegionInformation>(new_region_info));
            map->insert(std::make_pair(r1_id, new_region_info));
            r1_info = new_region_info;
          } else {
            r1_info = hash_iter->second;
          }

          // Same for region 2.
          hash_iter = map->find(r2->my_id);
          if (hash_iter == map->end()) {
            RegionInformation* new_region_info = new RegionInformation;
            new_region_info->index = max_region_id_++;
            new_region_info->size = r2->sz;
            new_region_info->constrained_id = r2->constraint_id;

            region_list->push_back(shared_ptr<RegionInformation>(new_region_info));
            map->insert(std::make_pair(r2_id, new_region_info));
            r2_info = new_region_info;
          } else {
            r2_info = hash_iter->second;
          }

          if (neighbor_eval) {
            // Check if recognized as neighbors already.
            vector<int>::iterator insert_pos =
                std::lower_bound(r1_info->neighbor_idx.begin(),
                                 r1_info->neighbor_idx.end(),
                                 r2_info->index);

            if (insert_pos == r1_info->neighbor_idx.end() ||
                *insert_pos != r2_info->index) {          // Not present -> insert.
              r1_info->neighbor_idx.insert(insert_pos, r2_info->index);
            }

            // Same for other region.
            insert_pos = std::lower_bound(r2_info->neighbor_idx.begin(),
                                          r2_info->neighbor_idx.end(),
                                          r1_info->index);
            if (insert_pos == r2_info->neighbor_idx.end() ||
                *insert_pos != r1_info->index) {            // Not present -> insert.
              r2_info->neighbor_idx.insert(insert_pos, r1_info->index);
            }
          }
        }  // for edge loop.
      } // end bucket list loop.
    } // end bucket loop.

    if (max_region_id_ == 0) {
      std::cerr << "AssignRegionIdAndDetermineNeighbors: No boundary edges found. "
                << "This results in only one region and is probably not what was desired."
                << " Decrease tau or increase minimum number of hierarchy regions.\n";
      // Add one region so algorithm can proceed.
      const Region* r1 = GetRegion(0);    // Any region will do.
      const int r1_id = r1->my_id;

      RegionInformation* new_region_info = new RegionInformation;
      new_region_info->index = max_region_id_++;
      new_region_info->size = r1->sz;
      new_region_info->constrained_id = r1->constraint_id;

      // Add to list and hash_map.
      region_list->push_back(shared_ptr<RegionInformation>(new_region_info));
      map->insert(std::make_pair(r1_id, new_region_info));
    }
  }

  void FastSegmentationGraph::ReserveEdges(int num_edges) {
    int edges_per_bucket = num_edges / num_buckets_ / bucket_lists_.size() / 10;
    for (vector< vector<EdgeList> >::iterator bucket_list = bucket_lists_.begin();
         bucket_list != bucket_lists_.end();
         ++bucket_list) {
      for (vector<EdgeList>::iterator bucket = bucket_list->begin();
           bucket != bucket_list->end();
           ++bucket) {
        bucket->reserve(edges_per_bucket);
      }
    }
  }

  bool FastSegmentationGraph::CheckForIsolatedRegions() {
    // TODO(grundman)  PORT!
    /*
    // Map region id's to bool value indicated if region is connected to an edge.
    hash_map<int, bool> connected_regions;

    // Populate map with false entries. (possibly multiple times).
    for (int i = 0; i < regions_.size(); ++i) {
      connected_regions[GetRegion(i)->my_id] = false;
    }

    // Traverse edges, set incident regions to true.
    for (vector<EdgeList>::const_iterator bucket = buckets_.begin();
         bucket != buckets_.end();
         ++bucket) {
      for (EdgeList::const_iterator e = bucket->begin(); e != bucket->end(); ++e) {
        const Region* r1 = GetRegion(e->region_1 & INV_EDGE_MASK);
        const Region* r2 = GetRegion(e->region_2);

        hash_map<int, bool>::iterator r1_iter = connected_regions.find(r1->my_id);
        hash_map<int, bool>::iterator r2_iter = connected_regions.find(r2->my_id);

        ASSURE_LOG(r1_iter != connected_regions.end());
        ASSURE_LOG(r2_iter != connected_regions.end());

        r1_iter->second = true;
        r2_iter->second = true;
      }
    }

    // Look for false entries in connected_regions -> should not exists.
    bool is_constistent = true;
    for (hash_map<int, bool>::const_iterator i = connected_regions.begin();
         i != connected_regions.end();
         ++i) {
      if (i->second == false) {
        LOG(ERROR) << "Region " << i->first << " is isolated!";
        is_constistent = false;
      }
    }

    return is_constistent;
     */
  }

  void FastSegmentationGraph::FlattenUnionFind(bool separate_representatives) {
    const int region_offset = regions_.size();
    int new_region_id = region_offset;

    if (separate_representatives) {
      for (int i = 0; i < region_offset; ++i) {
        Region* r = GetRegion(i);
        int flatten_id = r->my_id;
        if (r->my_id < region_offset) {    // separate first.
          // Map representative.
          r->my_id = new_region_id;
          flatten_id = new_region_id;
          regions_.push_back(Region(new_region_id, r->rint, r->sz, r->constraint_id));
          // Don't use r anymore, possibly invalid reference.
          ++new_region_id;
        }

        regions_[i].my_id = flatten_id;
      }
      LOG(INFO) << "Inserted " << new_region_id - region_offset
                << " new representatives.";
    } else {
      for (int i = 0; i < region_offset; ++i) {
        Region* r = GetRegion(i);
        regions_[i].my_id = r->my_id;
      }

    }
  }

  inline FastSegmentationGraph::Region* FastSegmentationGraph::GetRegion(int id) {
    ASSERT_LOG(id >=0);
    ASSERT_LOG(id < regions_.size());

    Region* r = &regions_[id];
    const int parent_id = r->my_id;
    Region* parent = &regions_[parent_id];

    if (parent->my_id == parent_id) {
      return parent;
    } else {
      parent = GetRegion(parent_id);
      r->my_id = parent->my_id;
      return parent;
    }
  }

  inline FastSegmentationGraph::Region* FastSegmentationGraph::MergeRegions(
      Region* rep_1, Region* rep_2, float edge_weight) {
    Region* merged;

    // Assign smaller region new id.
    if(rep_1->sz > rep_2->sz) {
      // Update area.
      rep_1->sz += rep_2->sz;
      // Constraint max: if one constraint the other not -> act as sticky constraint.
      rep_1->constraint_id = std::max(rep_1->constraint_id, rep_2->constraint_id);
      // Point to larger region.
      rep_2->my_id = rep_1->my_id;
      merged = rep_1;
    } else {
      // Same as above for 2 regions.
      rep_2->sz += rep_1->sz;
      rep_2->constraint_id = std::max(rep_1->constraint_id, rep_2->constraint_id);
      rep_1->my_id = rep_2->my_id;
      merged = rep_2;
    }

    // Update rint.
    merged->rint = edge_weight + param_k_ / (float) merged->sz;
    return merged;
  }


  float SegmentationGraph::SegmentGraphOutOfCore(bool print_stat_output,
                                                 bool sort_edges) {
    // Consistency checks in debug mode!
#ifdef _DEBUG
    int idx = 0;
    // Is each region at its corresponding id?
    for (std::vector<Region>::const_iterator r = regions_.begin();
         r != regions_.end();
         ++r, ++idx) {
      if (idx != r->my_id) {
        std::cerr << "SegmentGraph: Inconsistent Graph to segment.\n";
        exit(1);
      }
    }

    // Test if there are any regions that would be definitely skipped based on
    // current edges. This should not be the case.
    // Flag all regions to be skipped.
    for (std::vector<Region>::iterator r = regions_.begin();
         r != regions_.end();
         ++r, ++idx) {
      r->skip = true;
    }

    // If incident to at least one non-boundary edge -> don't skip.
    for (vector<Edge>::const_iterator e = edges_.begin(); e != edges_.end(); ++e) {
      assert(e->region_1 >= 0);
      regions_[e->region_1].skip = false;
      if (e->region_2 >= 0)
        regions_[e->region_2].skip = false;
    }

    // Still regions, that will be skipped present? -> Error.
    for (std::vector<Region>::const_iterator r = regions_.begin();
         r != regions_.end();
         ++r, ++idx) {
      if (r->skip == true) {
        std::cerr << "Bad consitency error. Isolated region found. Aborting.\n";
        std::cerr << r->my_id << "\n";
        exit(1);
      }
    }

    // Edges flagged to be sorted but are not?
    if (!sort_edges) {
      for (vector<Edge>::const_iterator i = edges_.begin(); i != edges_.end() - 1; ++i) {
        if (i->weight > (i + 1)->weight) {
          std::cerr << "Inconsistency. List should be sorted but is not.\n";
          exit(1);
        }
      }
    }

#endif // _DEBUG

    if (sort_edges) {
      std::cerr << "SegmentGraphOutOfCoreSupport: Sorting " << edges_.size() << " edges\n";
      std::sort(edges_.begin(), edges_.end());
    }

    // Record edges not considered for merging.
    std::cerr << "Segmenting graph ...";
    EdgeList remaining;
    remaining.reserve(edges_.size() * 10 / 9);

    int decided = 0;
    int num_window_inner_edges = edges_.size();

    for (EdgeList::const_iterator e = edges_.begin(); e != edges_.end(); ++e) {
      // Out-of-core only test: Boundary edge? -> skip region.
      if (e->region_2 < 0) {
        assert(e->region_1 >=0);
        GetRegion(e->region_1)->skip = true;
        --num_window_inner_edges;
        continue;
      }

      // Get Regions representatives.
      Region* rep_1 = GetRegion(e->region_1);
      Region* rep_2 = GetRegion(e->region_2);

      // Skip flag is sticky.
      if (rep_1->skip) {
        rep_2->skip = true;
        remaining.push_back(*e);
        continue;
      }

      if (rep_2->skip) {
        rep_1->skip = true;
        remaining.push_back(*e);
        continue;
      }

      ++decided;
      if (rep_1 != rep_2) {
        if (e->weight <= rep_1->rint &&
            e->weight <= rep_2->rint) {
          MergeRegions(rep_1, rep_2, e->weight);
        } else {
          remaining.push_back(*e);
        }
      }
    }

    // Discard all internal edges.
    // edges_ is still sorted and contains only edges inside a segmentation graph.
    edges_.swap(remaining);
    std::cerr << " done.\n";

    float decision_ratio = (float)decided / (float)num_window_inner_edges;
    if (print_stat_output)
      std::cerr << "Decided on: " << decision_ratio << "\n";

    return decision_ratio;
  }

  void SegmentationGraph::SegmentGraphInCore(bool sort_edges) {
    if (sort_edges) {
      std::cerr << "SegmentGraphInCore: Sorting " << edges_.size() << " edges\n";
      std::sort(edges_.begin(), edges_.end());
    }

    // Record edges not considered for merging.
    std::cerr << "Segmenting graph ...";
    region_inner_edges_ = vector<bool>(edges_.size(), false);

    int edge_idx = 0;
    for (EdgeList::const_iterator e = edges_.begin(); e != edges_.end(); ++e, ++edge_idx) {
      // Get Regions representatives.
      Region* rep_1 = GetRegion(e->region_1);
      Region* rep_2 = GetRegion(e->region_2);

      if (rep_1 != rep_2) {
        if (e->weight <= rep_1->rint &&
            e->weight <= rep_2->rint) {
          MergeRegions(rep_1, rep_2, e->weight);
          region_inner_edges_[edge_idx] = true;
        }
      } else {
        region_inner_edges_[edge_idx] = true;
      }
    }

    std::cerr << " done.\n";
  }

  void SegmentationGraph::MergeSmallRegionsRemoveInnerEdges(int min_region_size) {
    EdgeList out_edges;
    out_edges.reserve (edges_.size() / 2);

    // Edges flagged for removal ?
    if (region_inner_edges_.size()) {
      int edge_idx = 0;
      int merged_edges = 0;
      int merges = 0;

      for (EdgeList::const_iterator e = edges_.begin();
           e != edges_.end();
           ++e, ++edge_idx) {

        if (region_inner_edges_[edge_idx]) {
          ++merged_edges;
          continue;
        }

        // Get Regions representatives.
        Region* rep_1 = GetRegion(e->region_1);
        Region* rep_2 = GetRegion(e->region_2);

        if (rep_1 != rep_2) {
          if (rep_1->sz < min_region_size || rep_2->sz < min_region_size) {
            MergeRegions(rep_1, rep_2, e->weight);
            ++merges;
          } else {
            out_edges.push_back(*e);
          }
        }
      }
      std::cerr << "SegmentGraph merged " << merged_edges << " edges.\n"
                << "MergeSmallRegions performed " << merges << " merges.\n"
                << "RemoveInnerEdges: " << out_edges.size() << " out of "
                << edges_.size() << " remaining. Ratio: "
                << (float)out_edges.size() / edges_.size() << "\n";
    } else {
      for (EdgeList::const_iterator e = edges_.begin();
           e != edges_.end();
           ++e) {
        // Get Regions representatives.
        Region* rep_1 = GetRegion(e->region_1);
        Region* rep_2 = GetRegion(e->region_2);

        if (rep_1 != rep_2) {
          if (rep_1->sz < min_region_size || rep_2->sz < min_region_size) {
            MergeRegions(rep_1, rep_2, e->weight);
          } else {
            out_edges.push_back(*e);
          }
        }
      }
    }
    // Discard all internal edges.
    edges_.swap(out_edges);
  }

  void SegmentationGraph::AssignRegionIdAndDetermineNeighbors(RegionInfoList* list,
                                                              RegionInfoPtrMap* map) {
    typedef RegionInfoPtrMap::iterator hash_map_iter_t;

    // Traverse all remaining edges.
    max_region_id_ = 0;

    for(EdgeList::const_iterator e = edges_.begin(); e != edges_.end(); ++e) {
      const Region* r1 = GetRegion(e->region_1);
      const Region* r2 = GetRegion(e->region_2);

      const int r1_id = r1->my_id;
      const int r2_id = r2->my_id;

      assert(r1_id != r2_id);

      RegionInformation* r1_info, *r2_info;

      // Find region 1 in hash_map, insert if not present.
      hash_map_iter_t hash_iter = map->find(r1->my_id);
      if (hash_iter == map->end()) {  // Does not exists yet --> insert.
        RegionInformation* new_region_info = new RegionInformation;
        new_region_info->index = max_region_id_++;
        new_region_info->size = r1->sz;

        // Add to list and hash_map.
        list->push_back(shared_ptr<RegionInformation>(new_region_info));
        map->insert(std::make_pair(r1_id, new_region_info));
        r1_info = new_region_info;
      } else {
        r1_info = hash_iter->second;
      }

      // Same for region 2.
      hash_iter = map->find(r2->my_id);
      if (hash_iter == map->end()) {
        RegionInformation* new_region_info = new RegionInformation;
        new_region_info->index = max_region_id_++;
        new_region_info->size = r2->sz;

        list->push_back(shared_ptr<RegionInformation>(new_region_info));
        map->insert(std::make_pair(r2_id, new_region_info));
        r2_info = new_region_info;
      } else {
        r2_info = hash_iter->second;
      }

      // Check if recognized as neighbors already.
      vector<int>::iterator insert_pos = std::lower_bound(r1_info->neighbor_idx.begin(),
                                                          r1_info->neighbor_idx.end(),
                                                          r2_info->index);
      if (insert_pos == r1_info->neighbor_idx.end() ||
          *insert_pos != r2_info->index) {            // Not present -> insert.
        r1_info->neighbor_idx.insert(insert_pos, r2_info->index);
      }

      // Same for other region.
      insert_pos = std::lower_bound(r2_info->neighbor_idx.begin(),
                                    r2_info->neighbor_idx.end(),
                                    r1_info->index);
      if (insert_pos == r2_info->neighbor_idx.end() ||
          *insert_pos != r1_info->index) {            // Not present -> insert.
        r2_info->neighbor_idx.insert(insert_pos, r1_info->index);
      }
    }  // for edge loop.

    if (max_region_id_ == 0) {
      std::cerr << "AssignRegionIdAndDetermineNeighbors: No boundary edges found. "
                << "This results in only one region and is probably not what was desired. "
                << "Decrease tau or increase minimum number of hierarchy regions.\n";
      // Add one region so algorithm can proceed.
      const Region* r1 = GetRegion(0);    // Any region will do.
      const int r1_id = r1->my_id;


      RegionInformation* new_region_info = new RegionInformation;
      new_region_info->index = max_region_id_++;
      new_region_info->size = r1->sz;

      // Add to list and hash_map.
      list->push_back(shared_ptr<RegionInformation>(new_region_info));
      map->insert(std::make_pair(r1_id, new_region_info));
    }
  }

  void SegmentationGraph::RemoveAllButSeparatingEdge() {
    // Remove all edges but the lowest cost edges between two regions.
    vector< vector<int> > neighbors(regions_.size());
    vector<Edge> new_edges;
    new_edges.reserve(edges_.size() / 4);

    for (vector<Edge>::const_iterator e = edges_.begin();
         e != edges_.end();
         ++e) {
      // Get region ids.
      int r1_id = GetRegion(e->region_1)->my_id;
      int r2_id = GetRegion(e->region_2)->my_id;

      if (r1_id == r2_id) {
        // Inner edge;
        continue;
      }

      // Always put lower edge id first.
      if (r1_id > r2_id)
        std::swap(r1_id, r2_id);

      // Already connected?
      vector<int>::iterator n_pos = std::lower_bound(neighbors[r1_id].begin(),
                                                     neighbors[r1_id].end(),
                                                     r2_id);

      if (n_pos == neighbors[r1_id].end() ||
          *n_pos != r2_id) {
        // Add neighbor
        neighbors[r1_id].insert(n_pos, r2_id);
        // Add edge with re-presentatives.
        new_edges.push_back(Edge(r1_id, r2_id, e->weight));
      } else {
        // Don't care. One edge of any weight will do.
      }
    }

    // Discard old edges_;
    edges_.swap(new_edges);
  }

  SegmentationGraph::Region* SegmentationGraph::GetRegion(int id) {
    assert(id >=0);
    assert(id < regions_.size());

    Region* r = &regions_[id];
    Region* last_r;

    // Get region representative with path compression.
    while (r->my_id != id) {
      id = r->my_id;
      last_r = r;
      r = &regions_[id];
      last_r->my_id = r->my_id;
    }

    return r;
  }

  SegmentationGraph::Region* SegmentationGraph::MergeRegions(
      Region* rep_1, Region* rep_2, float edge_weight) {
    Region* merged;

    // Assign smaller region new id.
    if(rep_1->sz > rep_2->sz) {
      rep_1->sz += rep_2->sz;
      rep_2->my_id = rep_1->my_id;
      merged = rep_1;
    } else {
      rep_2->sz += rep_1->sz;
      rep_1->my_id = rep_2->my_id;
      merged = rep_2;
    }

    // Update rint.
    merged->rint = edge_weight + param_k_ / (float) merged->sz;
    return merged;
  }

  bool SegmentationGraph::SegmentationGraphHeader::IsEqual(
      const GenericHeader& rhs_header) const {
    const SegmentationGraphHeader& rhs = dynamic_cast<const SegmentationGraphHeader&>(
                                             rhs_header);
    return graph_id_ == rhs.graph_id_;
  }

  string SegmentationGraph::SegmentationGraphHeader::ToString() const {
    return lexical_cast<string>(graph_id_);
  }

  void SegmentationGraph::SegmentationGraphBody::WriteToStream(std::ofstream& ofs) const {
    int edge_sz = edges_->size();
    ofs.write(reinterpret_cast<char*>(&edge_sz), sizeof(edge_sz));
    ofs.write(reinterpret_cast<char*>(&(*edges_)[0]), edge_sz * sizeof(Edge));

    // Write region size.
    int region_sz = regions_->size();
    ofs.write(reinterpret_cast<char*>(&region_sz), sizeof(region_sz));
    ofs.write(reinterpret_cast<char*>(&(*regions_)[0]), region_sz * sizeof(Region));
  }

  void SegmentationGraph::SegmentationGraphBody::ReadFromStream(std::ifstream& ifs) {
    int edge_sz;
    ifs.read(reinterpret_cast<char*>(&edge_sz), sizeof(edge_sz));

    edges_.reset(new vector<Edge>(edge_sz));
    ifs.read(reinterpret_cast<char*>(&(*edges_)[0]), edge_sz * sizeof(Edge));

    int region_sz;
    ifs.read(reinterpret_cast<char*>(&region_sz), sizeof(region_sz));

    regions_.reset(new vector<Region>(region_sz));
    ifs.read(reinterpret_cast<char*>(&(*regions_)[0]), region_sz * sizeof(Region));
  }

  bool SegmentationGraph::SegGraphEdgeListHeader::IsEqual(const GenericHeader& rhs_header) const {
    const SegGraphEdgeListHeader& rhs = dynamic_cast<const SegGraphEdgeListHeader&>(rhs_header);
    return id_ == rhs.id_;
  }

  string SegmentationGraph::SegGraphEdgeListHeader::ToString() const {
    return lexical_cast<string>(id_);
  }

  void SegmentationGraph::SegGraphEdgeListBody::WriteToStream(std::ofstream& ofs) const {
    int edge_sz = edges_->size();
    ofs.write(reinterpret_cast<char*>(&edge_sz), sizeof(edge_sz));
    ofs.write(reinterpret_cast<char*>(&(*edges_)[0]), edge_sz * sizeof(Edge));
  }

  void SegmentationGraph::SegGraphEdgeListBody::ReadFromStream(std::ifstream& ifs) {
    int edge_sz;
    ifs.read(reinterpret_cast<char*>(&edge_sz), sizeof(edge_sz));

    edges_.reset(new vector<Edge>(edge_sz));
    ifs.read(reinterpret_cast<char*>(&(*edges_)[0]), edge_sz * sizeof(Edge));
  }
}
