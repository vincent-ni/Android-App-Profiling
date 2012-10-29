/*
 *  region_segmentation_graph.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef REGION_SEGMENTATION_GRAPH_H__
#define REGION_SEGMENTATION_GRAPH_H__

#include "segmentation_common.h"
#include "segmentation_graph.h"

namespace Segment {
  typedef hash_map<int, vector<int> > Skeleton;

class RegionSegmentationGraph : public FastSegmentationGraph {
public:
  RegionSegmentationGraph(float param_k);

  void AddRegionEdges(const RegionDistance& distance,
                      const RegionInfoList& list);
  void AddRegionEdgesConstrained(const RegionDistance& distance,
                                 const RegionInfoList& list,
                                 const vector<int>& constraint_ids,
                                 const Skeleton& skeleton);

  // Overriden function to use constant param_k (independent of region size).
  Region* MergeRegions(Region* rep_1, Region* rep_2, float edge_weight);

  // Uses old_regions to build new merged histograms for super-regions
  // specified by map.
  // Updates also parent_id member in old_regions.
  void FillChildrenAndHistograms(RegionInfoList& old_regions,
                                 const RegionInfoPtrMap* map);

  // Uses old_regions to build new merged histograms and merged scanline representations
  // specified by map.
  // Does not establish connection with old_regions, because this call will be usually
  // used to successively discard the lowest level of the hierarchy until
  // we have less than SegmentationOptions::max_region_num() regions.
  void MergeScanlineRepsAndHistograms(const RegionInfoList& old_regions,
                                      const RegionInfoPtrMap* map);

private:
  void AddRegionEdgesImpl(const RegionDistance& distance,
                          const RegionInfoList& list,
                          const vector<int>& constraint_ids);

private:
  vector<int> region_merges_;
};

// Implements true hierarchical agglomerative clustering.
class RegionAgglomerationGraph {
 public:
  // Internal classes, exposed for passing information between graphs for different
  // hierarchy levels.

  // An edge is always ordered by region ids, smallest first.
  struct Edge {
    inline Edge(int r1, int r2) {
      if (r1 < r2) {
        region_1 = r1;
        region_2 = r2;
      } else {
        region_1 = r2;
        region_2 = r1;
      }
    }

    bool operator==(const Edge& rhs) const {
      return region_1 == rhs.region_1 && region_2 == rhs.region_2;
    }

    int region_1;
    int region_2;
  };

  struct EdgeHasher {
    EdgeHasher(int neighbors_per_region_ = 23)
        : neighbors_per_region(neighbors_per_region_) {
    }

    size_t operator()(const Edge& edge) const {
      return edge.region_1 * neighbors_per_region + edge.region_2 % neighbors_per_region;
    }

    int neighbors_per_region;
  };

  typedef unordered_map<Edge, float, EdgeHasher> EdgeWeightMap;
 public:
  RegionAgglomerationGraph(float max_weight,
                           int num_buckets,
                           const RegionDistance* distance);

  void AddRegionEdges(const RegionInfoList& list,
                      const EdgeWeightMap* weight_map);  // optional.

  void AddRegionEdgesConstrained(const RegionInfoList& list,
                                 const EdgeWeightMap* weight_map,
                                 const vector<int>& constraint_ids,
                                 const Skeleton& skeleton);

  // Performs iteratively lowest cost merges until number of regions, drops below
  // fractional cutoff_fraction.
  // Returns number of performed merges.
  int SegmentGraph(bool merge_rasterization, float cutoff_fraction);

  void ObtainSegmentationResult(RegionInfoList* prev_level,
                                RegionInfoList* curr_level,
                                EdgeWeightMap* weight_map);

 protected:
  void AddRegion(int id, int constraint, int size, shared_ptr<RegionInformation> ptr);
  void AddRegionEdgesImpl(const RegionInfoList& region_list,
                          const vector<int>& constraint_ids,
                          const EdgeWeightMap* weight_map);

  // Inserts edge into corresponding bucket and creates entry in EdgePositionMap.
  // Does not check if dual edge is already present (graph edges are undirected).
  // Returns if edge is mergable, i.e. was inserted into a bucket in edge_buckets_.
  bool AddEdge(int region_id_1, int region_id_2, float weight);

  struct Region {
    Region(int id_,
           int constraint_id_,
           int sz_,
           shared_ptr<RegionInformation> region_info_) : id(id_),
                                                         constraint_id(constraint_id_),
                                                         sz(sz_),
                                                         region_info(region_info_) {
    }

    int id;
    int constraint_id;
    int sz;
    shared_ptr<RegionInformation> region_info;
  };

  Region* GetRegion(int region_id);

  // Returns new minimum edge weight. Invalidates all iterators to edges connecting rep_1
  // or rep_2 with other regions.
  float MergeRegions(Region* rep_1, Region* rep_2, bool merge_rasterization);

  bool AreRegionsMergable(const Region& region_1, const Region& region_2);

 private:
  // Maximum edge weight and scale for number of buckets.
  float max_weight_;
  int num_buckets_;
  float edge_scale_;  // Scale applied to edge weight to yield bucket idx.
  const RegionDistance* distance_;

  // List of edges in buckets ordered by edge weight;
  typedef vector<list<Edge> > EdgeBuckets;
  EdgeBuckets edge_buckets_;   // size = num_buckets + 1 (last bucket for virtual
                               //                         edges).
  struct EdgePosition {
    EdgePosition() : bucket(-1) { }
    EdgePosition(list<Edge>::iterator iter_, int bucket_) : iter(iter_), bucket(bucket_) {
    }

    list<Edge>::iterator iter;
    int bucket;
  };

  // Maps an edge to its position.
  typedef unordered_map<Edge, EdgePosition, EdgeHasher> EdgePositionMap;
  EdgePositionMap edge_position_map_;

  vector<Region> regions_;

  // Kept from SegmentGraph for ObtainSegmentationResult.
  bool merge_rasterization_;
};

}

#endif // REGION_SEGMENTATION_GRAPH_H__
