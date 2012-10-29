/*
 *  segmentation.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 10/30/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef SEGMENTATION_H__
#define SEGMENTATION_H__

#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include "segmentation.pb.h"
#include "segmentation_common.h"
#include "segmentation_graph.h"
#include "dense_segmentation_graph.h"
#include "segmentation_util.h"

typedef struct _IplImage IplImage;

namespace Segment {

  using boost::shared_array;
  using boost::shared_ptr;
  using boost::scoped_ptr;
  using std::string;
  using std::vector;
  typedef SegmentationDesc::HierarchyLevel HierarchyLevel;

  class Segmentation {
  public:
    Segmentation(float param_k,
                 int frame_width,
                 int frame_height,
                 int chunk_id);

    ~Segmentation();

    // Call before any AddImage* function to allocate memory of sufficient size.
    // Does nothing in case AddImage* was called before.
    void OverSegmentationSizeHint(int chunk_size);

    template <class SpatialPixelDistance>
    void AddGenericImage(SpatialPixelDistance* distance);

    template <class SpatialPixelDistance>
    void AddGenericImageConstrained(const SegmentationDesc& desc,
                                    SpatialPixelDistance* distance);

    template <class TemporalPixelDistance>
    void ConnectTemporally(TemporalPixelDistance* distance);

    template <class TemporalPixelDistance>
    void ConnectTemporallyAlongFlow(const cv::Mat& flow_x,
                                    const cv::Mat& flow_y,
                                    TemporalPixelDistance* distance);


    // If input_mapping is specified, the RegionMapping is used to set the counterpart
    // member in each newly added RegionInformation. Conversly, the order of added
    // RegionInformation's is reported in output_mapping if specified.
    // If remove_overlap_regions is set to true, all regions in hierarchy_level that
    // are completely contained in the overlap are ignored.
    void InitializeBaseHierarchyLevel(const HierarchyLevel& hierarchy_level,
                                      const DescriptorExtractorList& extractors,
                                      RegionMapping* input_mapping,
                                      RegionMapping* output_mapping);

    // Adds oversegmentation from obtained from DenseSegmentationUnit to perform
    // hierarchical segmentation. Optical flow ptr are optional.
    // Note: InitializeBaseHierarchyLevel has to be called first for each chunk.
    void AddOverSegmentation(const SegmentationDesc& desc,
                             const DescriptorExtractorList& extractors);

    // Adds an explicit connection between two over-segmented regions.
    // It is expected that regions were added via AddOverSegmentation beforehand.
    void AddOverSegmentationConnection(int region_id_1,
                                       int region_id_2,
                                       int match_id_1,
                                       int match_id_2,
                                       float strength);

    void ResetChunkBoundary() { last_chunk_added_ = -1; }

    // Enables chunk set based hierarchical segmentation by pulling the segmentation
    // result from each's region counterpart that will be used as constraint.
    void PullCounterpartSegmentationResult(const Segmentation& prev_seg);

    // Runs the actual over-segmentation. Enfocrces all regions to be of minimum size
    // min_region_size. Additionally you can specify the start of an overlap section
    // at the end of the volume (in frames).
    void RunOverSegmentation(int min_region_size,
                             bool two_stage_segmentation,
                             bool thin_structure_suppression,
                             bool n4_connectivity,
                             bool spatial_cleanup);

    // This requires an external segmentation to be supplied via AddOverSegmentation and
    // InitializeBaseHierarchyLevel.
    // The number of regions is reduced at every level to level_cutoff_fraction times the
    // number of regions in the previous level, until number of regions falls
    // below min_region_num
    // If reduction is set to true, the first reduction is performed such that
    // the number does not exceed max_region_num, regardless of cutoff fraction.
    // If the number of regions does not exceed max_region_num, the first level is
    // duplicated. In either case, the merge or duplication is performed with
    // Rasterizations, i.e. it is save to call DiscardBottomLevel();
    void RunHierarchicalSegmentation(const RegionDistance& distance,
                                     float level_cutoff_fraction,
                                     int min_region_num,
                                     int max_region_num,
                                     bool reduction);

    // Iterates over all regions in desc testing if rasterization is in [lhs, rhs].
    // Regions, that are completly outside this bound will be labeled as
    // FLAGGED_FOR_REMOVAL and are excluded on RetrieveSegmentation3D.
    // SuperRegions are flagged for removal if all of its children are flagged.
    void ConstrainSegmentationToFrameInterval(int lhs, int rhs);

    // Iterates over regions in all hierarchy levels, adjusting area to only consist
    // of those slices within [lhs, rhs].
    void AdjustRegionAreaToFrameInterval(int lhs, int rhs);

    void RetrieveSegmentation2D(SegmentationDesc* desc);

    // Vector region_id_offset has to be at least of size ComputedHierarchyLevels().
    void RetrieveSegmentation3D(int frame_number,
                                bool save_descriptors,
                                SegmentationDesc* desc);

    // Assigns each region in the hierarchy a unique id. Specifically, if a region is
    // constrained and flag use_constraint_ids is set, a regions constrained_id will
    // be used as region id. Otherwise, region id is set to a regions index shifted
    // by region_id_offset[level] yielding unique ids across clips.
    // Maximum id at each level is returned in max_region_ids (OPTIONAL).
    void AssignUniqueRegionIds(bool use_constraint_ids,
                               const vector<int>& region_id_offset,
                               vector<int>* max_region_ids);

    int ComputedHierarchyLevels() const { return region_infos_.size(); }
    int NumRegionsAtLevel(int level) const { return region_infos_[level]->size(); }

    void ForceMergeLevels(int num_levels);
    void DiscardBottomLevel();

    int NumFramesAdded() const { return frame_number_; }
    int NumBaseHierarchiesAdded() const { return num_base_hierarchies_; }
  protected:
    // For each compound region at the current level, find one child in the base-level,
    // query the child's hierarchial_region_ids member at level and sets corresponding
    // output_ids member to the result.
    // It also returns the skeleton for the current hierarchy level, i.e. for each
    // constraint id the set of region that are constraint to it.
    void SetupRegionConstraints(int level,
                                vector<int>* output_ids,
                                hash_map<int, vector<int> >* skeleton);

    // Fills Segmentation desc with region at current frame_number.
    // If use_constraint_ids is set uses constraint_id instead of region_id specified by
    // RegionInformation. Id's are displaced at each hierarchy level by the
    // corresponding entry in region_id_offset.
    void FillSegmentationDesc(int frame_number,
                              bool save_descriptors,
                              SegmentationDesc* desc);

    // Adds new Region2D from RegionInformation ri to desc at current frame_number.
    void AddRegion2DToSegmentationDesc(const RegionInformation& ri,
                                       int frame_number,
                                       SegmentationDesc* desc) const;

    // Maps region id to minimum and maximum frame number, region is present.
    typedef hash_map<int, pair<int, int> > IdToBoundsMap;
    // Same as above for compound region.
    // Determines minimum and maximum frame for each compound region, from
    // input prev_bound_map and outputs map for current level in curr_bound_map.
    // If level == 0, you can pass an empty map for prev_bound_map.
    void AddCompoundRegionToSegmentationDesc(const RegionInformation& ri,
                                             int level,
                                             const IdToBoundsMap& prev_bound_map,
                                             IdToBoundsMap* curr_bound_map,
                                             HierarchyLevel* hier) const;
  protected:
    const float param_k_;
    const int frame_width_;
    const int frame_height_;
    const int chunk_id_;
    int frame_number_;
    int num_base_hierarchies_;

    // Set to true, if PullCounterpartSegmentationResult was called.
    bool is_constrained_hierarchical_segmentation_;

    // Set by AssignUniqueRegionIds.
    bool assigned_constrained_ids_;
    bool assigned_unique_ids_;

    // Remember regions added to chunk_.
    hash_map<int, bool> regions_added_to_chunk_;

    // Saves id of last added chunk. Used to detect chunk boundaries.
    int last_chunk_added_;

    shared_ptr<DenseSegmentationGraph> dense_seg_graph_;

    // RegionInformation structs with consecutive assigned id's.
    // Each vector entry represents a level of the segmentation.
    vector< shared_ptr<RegionInfoList> > region_infos_;

    // Used by subsequent calls of AddOverSegmentation.
    RegionInfoPtrMap region_info_map_;
  };

  template <class SpatialPixelDistance>
  void Segmentation::AddGenericImage(SpatialPixelDistance* distance) {
    // Do we have to add a segmentation graph?
    ASSURE_LOG(dense_seg_graph_ != NULL) << "Call OverSegmentationSizeHint first.";
    dense_seg_graph_->AddNodesAndSpatialEdges(distance);

    ++frame_number_;

    // Increment frame number.
    dense_seg_graph_->IncFrameNumber();
  }

  template <class SpatialPixelDistance>
  void Segmentation::AddGenericImageConstrained(const SegmentationDesc& desc,
                                                SpatialPixelDistance* distance) {
    // Do we have to add a segmentation graph?
    ASSURE_LOG(dense_seg_graph_ != NULL) << "Call OverSegmentationSizeHint first.";
    dense_seg_graph_->AddNodesAndSpatialEdgesConstrained(distance, desc);

    ++frame_number_;

    // Increment frame number.
    dense_seg_graph_->IncFrameNumber();
  }

  template <class TemporalPixelDistance>
  void Segmentation::ConnectTemporally(TemporalPixelDistance* distance) {
    ASSERT_LOG(frame_number_ >= 2) << "Add at least two images before introducing "
                                   << "temporal connections.";
    dense_seg_graph_->AddTemporalEdges(distance);
  }

  template <class TemporalPixelDistance>
  void Segmentation::ConnectTemporallyAlongFlow(const cv::Mat& flow_x,
                                                const cv::Mat& flow_y,
                                                TemporalPixelDistance* distance) {
    ASSERT_LOG(frame_number_ >= 2) << "Add at least two images before introducing "
                                   << "temporal connections.";
    dense_seg_graph_->AddTemporalFlowEdges(distance,
                                           flow_x,
                                           flow_y);
  }
}

#endif
