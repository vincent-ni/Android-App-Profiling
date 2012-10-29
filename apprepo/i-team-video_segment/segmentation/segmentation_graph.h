/*
 *  segmentation_graph.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef SEGMENTATION_GRAPH_H__
#define SEGMENTATION_GRAPH_H__

#include "common.h"

#include "segmentation_common.h"
#include "generic_swap.h"

namespace Segment {
  // Common interface all SegmentationGraphs have to support.
  // This is to abstract even common data-types as Edges and Nodes.
  class AbstractSegmentationGraph {
  public:
    virtual void SegmentGraph() =0;
    virtual void MergeSmallRegions(int min_region_sz) =0;
    virtual void AssignRegionIdAndDetermineNeighbors(RegionInfoList* region_list,
                                                     RegionInfoPtrMap* map) =0;

    virtual void AddRegion(int region_id, int size) =0;
    virtual void AddEdge(int region_id_1, int region_id_2, float weight) =0;

    virtual void ReserveEdges(int num_edges) =0;
  };

  // Reduced complexity version for segmentation algorithm by
  // replacing sorting stage of the original algorithm with bucket-sorting
  class FastSegmentationGraph : public AbstractSegmentationGraph {
  public:
    // Segments all bucket lists using skip buckets and forcing constraints.
    void SegmentGraph();

    // Considers only edges in the specified bucket lists.
    void SegmentGraph(const vector<int>& bucket_list_ids, bool use_skip_buckets,
                      bool force_constraints);

    void MergeSmallRegions(int min_region_sz) {
      vector<int> all_bucket_lists(bucket_lists_.size());
      for (int i = 0; i < bucket_lists_.size(); ++i) all_bucket_lists[i] = i;
      MergeSmallRegions(min_region_sz, all_bucket_lists, true, NULL);
    }

    // If discard_redundant_edges is set, one edge per region pair is kept.
    // You can constrain this remaining edge to lie within one of the
    // discard_bucket_lists (NULL is all lists should be considered).
    void MergeSmallRegions(int min_region_sz,
                           const vector<int>& bucket_list_ids,
                           bool discard_redundant_edges,
                           const vector<int>* discard_bucket_lists);

    void AssignRegionIdAndDetermineNeighbors(
        RegionInfoList* region_list,
        const vector<int>* neighbor_eval_bucket_list_ids,
        RegionInfoPtrMap* map);

    void AssignRegionIdAndDetermineNeighbors(RegionInfoList* region_list,
                                             RegionInfoPtrMap* map) {
      AssignRegionIdAndDetermineNeighbors(region_list, NULL, map);
    }

    void AddRegion(int region_id, int size, float tau) {
      regions_.push_back(Region(region_id, tau, size));
    }

    void AddRegion(int region_id, int size, float tau, int constraint_id) {
      regions_.push_back(Region(region_id, tau, size, constraint_id));
    }

    void AddRegion(int region_id, int size) {
      regions_.push_back(Region(region_id, param_k_, size));
    }

    void AddEdge(int region_id_1, int region_id_2, float weight) {
      AddEdge(region_id_1, region_id_2, weight, 0);   // only one bucket list by standard.
    }

    void AddEdge(int region_id_1, int region_id_2, float weight, int bucket_list) {
      const int bucket_index = (int)(weight * scale_);
      bucket_lists_[bucket_list][std::min(
          bucket_index, num_buckets_)].push_back(Edge(region_id_1, region_id_2));
    }

    void ReserveEdges(int num_edges);

    int NumBucketLists() const { return bucket_lists_.size(); }

    static int SizeOfEdge() { return sizeof(Edge); }
    static int SizeOfRegion() { return sizeof(Region); }
  protected:
    // Don't use this class directly, use to implement SegmentationGraph's for concrete
    // use case.
    // Create Segmentation graph discretizing the range of weights [0, max_weight] into
    // num_buckets. The first skip_buckets will be merged regardless of their weight.
    // Set to zero if not desired.
    // Edges can be added to different lists of buckets. During segmentation, only
    // a specific range of buckets can then be processed.
    FastSegmentationGraph(float param_k,
                          float max_weight,
                          int buckets,
                          int skip_buckets,
                          int num_bucket_lists = 1);

    // Costly implementation test function. Checks for regions that are present but don't
    // edges with other regions (isolated regions). Those should not exists. Use only for
    // debugging purposes.
    bool CheckForIsolatedRegions();

    // Traverses all regions, replacing the id of each region with the representative
    // id, therefore flatting the union find. If separate_representatives is set, each
    // representative will be re-created with an id >= regions_.size, therefore represent-
    // atives and nodes are separated. This should only be applied after some initial
    // segmentation otherwise too many representatives have to be re-created. This is
    // helpful to safely implement functions that carve parts away from regions to merge
    // them with other parts, without having to worry about overwriting representatives
    // or vital connections of the internal union find structure.
    void FlattenUnionFind(bool separate_representatives);

  protected:
    struct Edge {
      Edge (int r1, int r2) : region_1(r1), region_2(r2) {}
      int region_1;
      int region_2;
    };

    typedef vector<Edge> EdgeList;

    struct Region {
      Region(int _id, float _rint) : my_id(_id), rint(_rint), sz(1), constraint_id(-1) {}
      Region(int _id, float _rint, int _sz)
          : my_id(_id), rint(_rint), sz(_sz), constraint_id(-1) {}
      Region(int _id, float _rint, int _sz, int _const_id)
          : my_id(_id), rint(_rint), sz(_sz), constraint_id(_const_id) {}
      Region() : my_id(-1), rint(0), sz(0), constraint_id(-1) {}

      std::string ToString() {
        std::ostringstream os;
        os << "[id: " << my_id << " constraint: " << constraint_id
           << "] size: " << sz << "@" << rint;
        return os.str();
      }

      int my_id;           // id of representative of region (union find structure).
      float rint;          // Relaxed internal energy ... see paper.
      int sz;              // Size in pixels.
      int constraint_id;
    };

    Region* GetRegion(int start_id);

    bool AreRegionsMergable(const Region* lhs, const Region* rhs) {
      return (lhs->constraint_id < 0 ||
              rhs->constraint_id < 0 ||  // one is unconstraint.
              lhs->constraint_id == rhs->constraint_id);
    }

    virtual Region* MergeRegions(Region* rep_1, Region* rep_2, float edge_weight);

    const float param_k_;
    const float max_weight_;
    const int num_buckets_;
    const int skip_buckets_;
    float scale_;
    int max_region_id_;

    int num_forced_merges_;
    int num_regular_merges_;
    int num_small_region_merges_;

    vector<Region> regions_;

    // Edges can be arranged in different BucketLists, e.g. to separate different kinds
    // of connections.
    vector< vector<EdgeList> > bucket_lists_;
  };

  // SegmentationGraph with explicit weight per edge.
  // Has support for swapping framework.
  class SegmentationGraph : public AbstractSegmentationGraph {
    // Allow Segmentation to create Edges, Regions and call ColorDiffRGB;
    friend class Segmentation;
  public:
    // Public interface.
    virtual void SegmentGraph() { SegmentGraphInCore(true); }
    virtual void MergeSmallRegions(int min_region_sz) {
      MergeSmallRegionsRemoveInnerEdges(min_region_sz);
    }

    void AddRegion(int region_id, int size, float tau) {
      regions_.push_back(Region(region_id, tau, size));
    }

    void AddRegion(int region_id, int size) {
      regions_.push_back(Region(region_id, param_k_, size));
    }

    void AddEdge(int region_id_1, int region_id_2, float weight) {
      edges_.push_back(Edge(region_id_1, region_id_2, weight));
    }

    // Segments graph based on edge weights and regions rint.
    // Removes edges that caused a merge: inner region edges!
    // If sort_edges is true, edges will be sorted prior to segmentation.
    // Output flag set to true will print statistic about percentage of edges
    // that could be decided on.
    float SegmentGraphOutOfCore(bool print_stat_output,
                                bool sort_edges);

    // Same as above, but flags edges for removal and uses a lighter logic
    // not required for in core algorithm.
    void SegmentGraphInCore(bool sort_edges);

    // Merges all regions below min_region_sz and removes all inner region edges.
    void MergeSmallRegionsRemoveInnerEdges(int min_region_sz);

    // Traveres all edges, assigns id's and random color to each region and
    // determines neighbors of each region.
    virtual void AssignRegionIdAndDetermineNeighbors(RegionInfoList* list,
                                                     RegionInfoPtrMap* map);

    void RemoveAllButSeparatingEdge();

    void ReserveEdges(int num_edges) { edges_.reserve(num_edges); }

    static int SizeOfEdge() { return sizeof(Edge); }
    static int SizeOfRegion() { return sizeof(Region); }
  protected:
    // Don't allow SegmentationGraph to be constructed directly.
    SegmentationGraph(float param_k) : param_k_(param_k) {}
    virtual ~SegmentationGraph() {}

  protected:
    struct Edge {
      Edge (int r1, int r2, float w) : region_1(r1), region_2(r2), weight(w) {}
      Edge () : region_1(-1), region_2(-1), weight(0) {}  // for I/O.
      int region_1;
      int region_2;
      float weight;
    };

    friend bool operator<(const Edge&, const Edge&);

    typedef vector<Edge> EdgeList;

    struct Region {
      Region(int _id, float _rint) : my_id(_id), rint(_rint), sz(1), skip(false) {}
      Region(int _id, float _rint, float _sz) : my_id(_id), rint(_rint), sz(_sz), skip(false) {}
      Region() : my_id(-1), rint(0), sz(0), skip(false) {}

      // TODO: skip to global vector<bool> for all regions.
      int my_id;           // id of representative of region (union find structure).
      float rint;          // Relaxed internal energy ... see paper.
      int sz;              // Size in pixels.
      bool skip;           // Skip flag if incident to a boundary-edge.
    };

  protected:
    // Support for swapping graph's or parts of a graph to file.
    class SegmentationGraphHeader : public GenericHeader {
    public:
      SegmentationGraphHeader(int graph_id) : graph_id_(graph_id) {}
      virtual bool IsEqual(const GenericHeader&) const;
      virtual string ToString() const;
    protected:
      int graph_id_;
    };

    class SegmentationGraphBody : public GenericBody {
    public:
      SegmentationGraphBody(shared_ptr<vector<Edge> > edges,
                            shared_ptr<vector<Region> > regions) :
          edges_(edges), regions_(regions) {}

      SegmentationGraphBody() : edges_(shared_ptr<vector<Edge> >()),
                                regions_(shared_ptr<vector<Region> >()) {}

      virtual void WriteToStream(std::ofstream& ofs) const;
      virtual void ReadFromStream(std::ifstream& ifs);

      shared_ptr<vector<Edge> > GetEdges() const { return edges_; }
      shared_ptr<vector<Region> > GetRegions() const { return regions_; }
    protected:
      shared_ptr<vector<Edge> > edges_;
      shared_ptr<vector<Region> > regions_;
    };

    class SegGraphEdgeListHeader : public GenericHeader {
    public:
      SegGraphEdgeListHeader(int id) : id_(id) {}
      virtual bool IsEqual(const GenericHeader& rhs) const;
      virtual string ToString() const;
    protected:
      int id_;
    };

    class SegGraphEdgeListBody : public GenericBody {
    public:
      SegGraphEdgeListBody(shared_ptr< vector<Edge> > edges = shared_ptr< vector<Edge> >())
      : edges_(edges) {}

      virtual void WriteToStream(std::ofstream& ofs) const;
      virtual void ReadFromStream(std::ifstream& ifs);

      shared_ptr< vector<Edge> > GetEdges() const { return edges_; }
    private:
      shared_ptr< vector<Edge> > edges_;
    };

    static float ColorDiffRGB(const float* p, const float* q) {
      const float b_diff = *p++ - *q++;
      const float g_diff = *p++ - *q++;
      const float r_diff = *p - *q;
      const float ret = sqrt(b_diff*b_diff + g_diff*g_diff + r_diff*r_diff);
      return ret;
    }

    static float RegionWeight(const int* region_1, const int* region_2, const float weight) {
      return (*region_1 == *region_2) ? weight : (100.0f * (1.0f - weight));
    }

    Region* GetRegion(int start_id);
    virtual Region* MergeRegions(Region* rep_1, Region* rep_2, float edge_weight);

  protected:
    const float param_k_;
    int max_region_id_;

    EdgeList edges_;
    // Used to flag inner edges during SegmentGraph.
    vector<bool> region_inner_edges_;
    vector<Region> regions_;
  };

  inline
  bool operator<(const SegmentationGraph::Edge& lhs, const SegmentationGraph::Edge& rhs) {
    // Place boundary edges last in each equivalence class.
    if (lhs.weight != rhs.weight)
      return lhs.weight < rhs.weight;
    else {
      if (lhs.region_2 >= 0 && rhs.region_2 < 0)
        return true;
      else
        return false;
    }
  }
}

#endif // SEGMENTATION_GRAPH_H__
