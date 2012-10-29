/*
 *  dense_segmentation_graph.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef DENSE_SEGMENTATION_GRAPH_H__
#define DENSE_SEGMENTATION_GRAPH_H__

#include "segmentation_graph.h"
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <opencv2/core/core.hpp>

namespace Segment {

  typedef boost::tuple<int, int, float> EdgeTuple;

  struct EdgeTupleComparator {
    bool operator()(const EdgeTuple& lhs, const EdgeTuple& rhs) {
      return boost::get<2>(lhs) < boost::get<2>(rhs);
    }
  };

  // Usage:
  // DenseSegmentationGraph graph;
  // SpatialDistance spatial_distance[N];      // supplied by user.
  // TemporalDistance temporal_distance[N-1];  // supplied by user.
  // // Add first nodes and spatial connections for two frames.
  // graph.AddNodesAndSpatialEdges(&spatial_distance[0]);
  // graph.AddNodesAndSpatialEdges(&spatial_distance[1]);
  // // Connect in time.
  // graph.AddTemporalEdges(&temporal_distance[0]);
  // // Add another slice.
  // graph.AddNodesAndSpatialEdges(&spatial_distance[2]);
  // // Connect in time.
  // graph.AddTemporalEdges(&temporal_distance[1]);
  // ...
  // graph.SegmentGraph();
  // graph.MergeSmallRegions(min_region_size);
  // // Obtain results.
  // RegionInfoList regions;
  // RegionInfoPtrMap id_to_region_map;    // Maps representative id to RegionInformation*
  // graph.AssignRegionIdAndDetermineNeighbors(&regions, &id_to_region_map);
  // // Read out rasterized results.
  // graph.ObtainScanlineRepFromResults(id_to_region_map);

  class DenseSegmentationGraph : public FastSegmentationGraph {
  public:
    // Spatial edges are put in even bucket lists, temporal in odd.
    DenseSegmentationGraph(float param_k,
                           int frame_width,
                           int frame_height,
                           int max_frames);

    template <class SpatialPixelDistance>
    void AddNodesAndSpatialEdges(SpatialPixelDistance* dist);

    template <class SpatialPixelDistance>
    void AddNodesAndSpatialEdgesConstrained(SpatialPixelDistance* dist,
                                            const SegmentationDesc& desc);

    template <class TemporalPixelDistance>
    void AddTemporalEdges(TemporalPixelDistance* distance);

    // Same as above, but displaces flow edges in time along flow.
    template <class TemporalPixelDistance>
    void AddTemporalFlowEdges(TemporalPixelDistance* distance,
                              const cv::Mat& flow_x,
                              const cv::Mat& flow_y);

    // Call after Construction for for faster Add*Edges performance.
    // Specify the EXPECTED amount of frames for this segmentation graph.
    void FrameSizeHint(int frames);

    // Computes scanline representation for each region.
    void ObtainScanlineRepFromResults(const RegionInfoPtrMap& map,
                                      bool remove_thin_structure,
                                      bool enforce_n4_connections);

    void IncFrameNumber() { ++num_frames_; }

    // Frame view mode. Obtains a view of the current spatio-temporal segmentation
    // constrained to single frame at parameter frame.
    // (Creates new regions, replaces and backups current ids.)
    void ComputeFrameView(int frame);

    // Releases newly created regions. Replace ids with original backuped ones.
    void ResetFrameView();

    void SegmentGraphSpatially();
    void SpatialCleanupStep(int min_region_size);
    void SpatialCleanupStepImpl(int min_region_size,
                                int frame);

    void MergeSmallRegionsSpatially(int min_region_size);

    int FrameNumber() const { return num_frames_; }
    int MaxFrames() const { return max_frames_; }

  protected:
    void ThinStructureSuppression(cv::Mat* id_image,
                                  hash_map<int, int>* size_adjust_map);

    // Detects violating N8 only connectivity and performs resolving pixel swaps.
    void EnforceN4Connectivity(cv::Mat* id_image,
                               hash_map<int, int>* size_adjust_map);

    template <class SpatialPixelDistance>
    void AddSpatialEdges(SpatialPixelDistance* dist);

    // Returns edges between curr_idx (at location x, y) and prev_idx based on N-9.
    template <class TemporalPixelDistance>
    void GetLocalEdges(int x, int y, int curr_idx, int prev_idx,
                       TemporalPixelDistance* distance, vector<EdgeTuple>* local_edges);

    // Create a new batch of nodes for current frame.
    void AddNodes();

    // Same as above but sets constraint from region_ids.
    void AddNodesConstrained(const SegmentationDesc& desc);

    // Adds virtual edges necessary to achieve consistent segmentation across chunks.
    void AddSkeletonEdges(const SegmentationDesc& desc);

    Region* GetSpatialRegion(int id);

    // Used to add constrained nodes.
    cv::Mat region_ids_;

    // Backup of original ids used to create frame view on the graph.
    vector<Region> frame_view_regions_;
    vector<int> orig_frame_view_ids_;

    // Maps constraint id to <last used region id, in last used frame>
    hash_map<int, pair<int, int> > skeleton_temporal_map_;

    // Maps a newly created spatial region to the spatio-temporal.
    vector<int> corresponding_id_;
    // Inverse of above map.
    hash_map<int, vector<int> > inv_corresponding_id_sets_;

    // Tracks constraint slices.
    vector<int> constrained_slices_;

    int frame_view_frame_;

    int num_frames_;
    int frame_width_;
    int frame_height_;
    int max_frames_;
  };

  template <class SpatialPixelDistance>
  void DenseSegmentationGraph::AddNodesAndSpatialEdges(
      SpatialPixelDistance* dist) {
    AddNodes();
    AddSpatialEdges(dist);
  }

  template <class SpatialPixelDistance>
  void DenseSegmentationGraph::AddNodesAndSpatialEdgesConstrained(
      SpatialPixelDistance* dist, const SegmentationDesc& desc) {
    AddNodesConstrained(desc);
    AddSpatialEdges(dist);
    AddSkeletonEdges(desc);
  }

  template <class SpatialPixelDistance>
  void DenseSegmentationGraph::AddSpatialEdges(SpatialPixelDistance* dist) {
    // Add edges based on 8 neighborhood.
    const int base_idx = num_frames_ * frame_height_ * frame_width_;
    const int bucket_list_idx = 2 * num_frames_;
    for (int i = 0, end_y = frame_height_ - 1, cur_idx = base_idx; i <= end_y; ++i) {
      dist->MoveAnchorTo(0, i);
      dist->MoveTestAnchorTo(0, i);

      for (int j = 0, end_x = frame_width_ - 1;
           j <= end_x;
           ++j, ++cur_idx, dist->IncrementAnchor(), dist->IncrementTestAnchor()) {

        if (j < end_x) { // Edge to right.
          AddEdge(cur_idx, cur_idx + 1, dist->PixelDistance(1, 0), bucket_list_idx);
        }

        if (i < end_y) {  // Edge to bottom.
          AddEdge(cur_idx, cur_idx + frame_width_, dist->PixelDistance(0, 1),
                  bucket_list_idx);

          if (j > 0) {  // Edge to bottom left.
            AddEdge(cur_idx, cur_idx + frame_width_ - 1, dist->PixelDistance(-1, 1),
                    bucket_list_idx);
          }

          if (j < end_x) { // Edge to bottom right
            AddEdge(cur_idx, cur_idx + frame_width_ + 1, dist->PixelDistance(1, 1),
                    bucket_list_idx);
          }
        }
      }
    }
  }

  template <class TemporalPixelDistance>
  void DenseSegmentationGraph::GetLocalEdges(
      int x, int y, int curr_idx, int prev_idx,
      TemporalPixelDistance* distance, vector<EdgeTuple>* local_edges) {
    if (y > 0) { // Edges to top.
      const int local_prev_idx = prev_idx - frame_width_;
      if (x > 0) {
        local_edges->push_back(boost::make_tuple(curr_idx,
                                                 local_prev_idx - 1,
                                                 distance->PixelDistance(-1, -1)));
      }

      local_edges->push_back(boost::make_tuple(curr_idx,
                                               local_prev_idx,
                                               distance->PixelDistance(0, -1)));

      if (x + 1 < frame_width_) {
        local_edges->push_back(boost::make_tuple(curr_idx,
                                                 local_prev_idx + 1,
                                                 distance->PixelDistance(1, -1)));
      }
    }

    // Edges left and right.
    const int local_prev_idx = prev_idx;
    if (x > 0) {
      local_edges->push_back(boost::make_tuple(curr_idx,
                                               local_prev_idx - 1,
                                               distance->PixelDistance(-1, 0)));
    }

    local_edges->push_back(boost::make_tuple(curr_idx,
                                             local_prev_idx,
                                             distance->PixelDistance(0, 0)));

    if (x + 1 < frame_width_) {
      local_edges->push_back(boost::make_tuple(curr_idx,
                                               local_prev_idx + 1,
                                               distance->PixelDistance(1, 0)));
    }

    if (y + 1 < frame_height_) {  // Edges to bottom.
      const int local_prev_idx = prev_idx + frame_width_;
      if (x > 0) {
        local_edges->push_back(boost::make_tuple(curr_idx,
                                                 local_prev_idx - 1,
                                                 distance->PixelDistance(-1, 1)));
      }

      local_edges->push_back(boost::make_tuple(curr_idx,
                                               local_prev_idx,
                                               distance->PixelDistance(0, 1)));

      if (x + 1 < frame_width_) {
        local_edges->push_back(boost::make_tuple(curr_idx,
                                                 local_prev_idx + 1,
                                                 distance->PixelDistance(1, 1)));
      }
    }
  }

  template <class TemporalPixelDistance>
  void DenseSegmentationGraph::AddTemporalEdges(TemporalPixelDistance* distance) {
    // Add temporal edges to previous frame.
    ASSERT_LOG(num_frames_ >= 2);
    const int base_idx = (num_frames_ - 1) * frame_width_ * frame_height_;
    const int base_diff = frame_width_ * frame_height_;
    const int bucket_list_idx = 2 * (num_frames_ - 1) - 1;
    vector<EdgeTuple> local_edges;

    // Add edges based on 9 neighborhood in time.
    for (int i = 0, end_y = frame_height_ - 1, curr_idx = base_idx; i <= end_y; ++i) {
      distance->MoveAnchorTo(0, i);
      distance->MoveTestAnchorTo(0, i);

      for (int j = 0, end_x = frame_width_ - 1;
           j <= end_x;
           ++j, ++curr_idx, distance->IncrementAnchor(),
           distance->IncrementTestAnchor()) {
        local_edges.clear();
        const int prev_idx = curr_idx - base_diff;
        GetLocalEdges(j, i, curr_idx, prev_idx, distance, &local_edges);

        // Use all edge currently.
        // TODO: Evaluate speedup by using only subset of edges
        // (smallest and largest cost edge?).
        for (vector<EdgeTuple>::const_iterator edge = local_edges.begin(),
             edge_end = local_edges.end();
             edge != edge_end;
             ++edge) {
          AddEdge(boost::get<0>(*edge),
                  boost::get<1>(*edge),
                  boost::get<2>(*edge),
                  bucket_list_idx);
        }
      }
    }
  }

  // Same as above, but displaces flow edges in time along flow.
  template <class TemporalPixelDistance>
  void DenseSegmentationGraph::AddTemporalFlowEdges(TemporalPixelDistance* distance,
                                                    const cv::Mat& flow_x,
                                                    const cv::Mat& flow_y) {
    ASSERT_LOG(num_frames_ >= 2);

    // Nodes were added by AddNodesAndSpatialEdges run.
    // Add temporal edges to previous frame.
    const int base_idx = (num_frames_ - 1) * frame_width_ * frame_height_;
    const int base_diff = frame_width_ * frame_height_;
    const int bucket_list_idx = 2 * (num_frames_ - 1) - 1;
    vector<EdgeTuple> local_edges;

    // Add edges based on 9 neighborhood in time.
    for (int i = 0, end_y = frame_height_ - 1, curr_idx = base_idx; i <= end_y; ++i) {
      distance->MoveAnchorTo(0, i);

      const float* flow_x_ptr = flow_x.ptr<float>(i);
      const float* flow_y_ptr = flow_y.ptr<float>(i);

      for (int j = 0, end_x = frame_width_ - 1;
           j <= end_x;
           ++j, ++curr_idx, distance->IncrementAnchor(), ++flow_x_ptr, ++flow_y_ptr) {
        local_edges.clear();

        int prev_x = j + *flow_x_ptr;
        int prev_y = i + *flow_y_ptr;

        prev_x = std::max(0, std::min(frame_width_ - 1, prev_x));
        prev_y = std::max(0, std::min(frame_height_ - 1, prev_y));

        distance->MoveTestAnchorTo(prev_x, prev_y);

        int prev_idx = base_idx - base_diff + prev_y * frame_width_ + prev_x;
        GetLocalEdges(prev_x, prev_y, curr_idx, prev_idx, distance, &local_edges);

        /*
         float w = get<2>(*max_element(local_edges.begin(), local_edges.end(),
         EdgeTupleComparator()));
         if (w == 0 || w > 1) {
         // Add all.
         for (vector<EdgeTuple>::const_iterator edge = local_edges.begin(),
         edge_end = local_edges.end();
         edge != edge_end;
         ++edge) {
         AddEdge(get<0>(*edge), get<1>(*edge), get<2>(*edge));
         }
         } else {
         */
        // Add best 3 elements.
        //   const vector<EdgeTuple>::const_iterator min_elem =
        // std::min_element(local_edges.begin(), local_edges.end(), EdgeTupleComparator());
        // AddEdge(get<0>(*min_elem), get<1>(*min_elem), get<2>(*min_elem));
        //std::nth_element(local_edges.begin(), local_edges.begin(), local_edges.end(),
        //                 EdgeTupleComparator());
        //AddEdge(get<0>(local_edges[0]), get<1>(local_edges[0]), get<2>(local_edges[0]));

        // std::nth_element(local_edges.begin() + 1, local_edges.begin() + 1,
        //                   local_edges.end(), EdgeTupleComparator());
        //  AddEdge(get<0>(local_edges[1]), get<1>(local_edges[1]), get<2>(local_edges[1]));

        //      const vector<EdgeTuple>::const_iterator max_elem =
        // std::max_element(local_edges.begin(), local_edges.end(), EdgeTupleComparator());
        // AddEdge(get<0>(*max_elem), get<1>(*max_elem), get<2>(*max_elem));

        // }

        //std::sort(local_edges.begin(), local_edges.end(), EdgeTupleComparator());
        for (vector<EdgeTuple>::const_iterator edge = local_edges.begin(),
             edge_end = local_edges.end();
             edge != edge_end;
             ++edge) {
          AddEdge(boost::get<0>(*edge),
                  boost::get<1>(*edge),
                  boost::get<2>(*edge),
                  bucket_list_idx);
        }
      }
    }
  }

}  // namespace Segment.

#endif // DENSE_SEGMENTATION_GRAPH_H__
