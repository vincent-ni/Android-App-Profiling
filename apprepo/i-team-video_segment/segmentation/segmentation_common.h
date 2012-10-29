/*
 *  segmentation_common.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef SEGMANTATION_COMMON_H__
#define SEGMANTATION_COMMON_H__

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include <cmath>
#include <ext/hash_map>
using __gnu_cxx::hash_map;
#include <vector>
using std::vector;

#include "assert_log.h"
#include "generic_swap.h"
#include "histograms.h"
#include "image_util.h"
#include "pixel_distance.h"
#include "region_descriptor.h"
#include "segmentation_util.h"

typedef struct _IplImage IplImage;

namespace Segment {
  typedef unsigned char uchar;

  class ImageHeader : public GenericHeader {
  public:
    ImageHeader(int img_id) : img_id_(img_id) {}
    bool IsEqual(const GenericHeader&) const;
    string ToString() const;
  protected:
    int img_id_;
  };

  class ImageBody : public GenericBody {
  public:
    ImageBody(shared_ptr<IplImage> img = shared_ptr<IplImage>() ) : img_(img) {}
    virtual void WriteToStream(std::ofstream& ofs) const;
    virtual void ReadFromStream(std::ifstream& ifs);
    shared_ptr<IplImage> GetImage() const { return img_; }
  protected:
    shared_ptr<IplImage> img_;
  };

  // Holds all information of a Region that is needed for hierarchical segmentation.
  struct RegionInformation {
    RegionInformation() :
      index(-1),
      size(0),
      parent_idx(-1),
      region_status(NORMAL),
      counterpart(0),
      constrained_id(-1),
      region_id(-1) {
    }

    int index;                    // Location in RegionInfoList.
    int size;                     // Size in voxels.

    // Id of my super-region in the hierarchy level above.
    int parent_idx;

    enum RegionStatus {
      NORMAL, FLAGGED_FOR_REMOVAL
    };

    // Set if region is isolated and not part of the segmentation anymore,
    // region will not be output during RetrieveSegmentation.
    RegionStatus region_status;

    // Sorted array of neighboring regions' index.
    vector<int> neighbor_idx;

    // Optional information.
    // A region is either a node, i.e. has a scanline representation,
    // or is a super-region, i.e. has a list of children id's.
    shared_ptr<Rasterization3D> raster;                      // Frame slices.
    shared_ptr< vector<int> > child_idx;                     // Children list, sorted.

    // Information to constrain adjacent RegionSegmentations.
    RegionInformation* counterpart;     // pointer to counterpart in previous
                                        // segmentation chunk set.
    int constrained_id;                 // Id of parent in previous segmentation chunk
                                        // set.
                                        // Regions with the same id, should belong to the
                                        // same super-region.
    int region_id;                      // Output id in protobuffer. If region is
                                        // constrained, region_id == constraint_id.

    // Will be filled by PullSegmentationResultsFromCounterparts for regions
    // in the over-segmentation if a region has a counterpart. Specifically, we
    // save the result of the counterpart's segmentation as list of parent region_id's
    // from the leaf to the root.
    shared_ptr< vector<int> > counterpart_region_ids;

    vector<shared_ptr<RegionDescriptor> > region_descriptors_;
  public:
    // Returns a vector containing each descriptor pair's distance.
    // Distances is expected to be sized correctly, i.e. size needs to be equal to number
    // of AddRegionDescriptor
    void DescriptorDistances(const RegionInformation& rhs,
                             vector<float>* distances) const;

    // Adds RegionDescriptor to list of descriptors for this region and takes ownership.
    // Note, framework guarantees that order of added descriptors is constant for all
    // regions.
    void AddRegionDescriptor(RegionDescriptor* desc);

    RegionDescriptor* GetDescriptorAtIdx(int idx) {
      ASSERT_LOG(idx < region_descriptors_.size());
      ASSERT_LOG(region_descriptors_[idx] != NULL);
      return region_descriptors_[idx].get();
    }

    int NumDescriptors() const { return region_descriptors_.size(); }

    // Calls PopulatingDescriptorFinished for each RegionDescriptor.
    void PopulatingDescriptorsFinished();

    // Merges or copies (if not present in this RegionInformation)
    // all RegionDescriptors from rhs.
    void MergeDescriptorsFrom(const RegionInformation& rhs);

    // Outputs all regions descriptors to an AggregatedDescriptor in segmentation.proto
    void OutputRegionFeatures(RegionFeatures* features) const;
  };

  // Maps the id of the region representative to its assigned RegionInformation.
  typedef hash_map<int, RegionInformation*> RegionInfoPtrMap;

  // Represents all information of a segmentation at a specific level of the hierarchy.
  typedef vector< shared_ptr<RegionInformation> > RegionInfoList;

  // A region mapping map region id to created RegionInformation. It is used to 
  // associate the irregular segmentation graphs between two chunk sets.
  typedef hash_map<int, RegionInformation*> RegionMapping;

  // Merges all information of src into dst.
  void MergeRegionInfoInto(const RegionInformation* src, RegionInformation* dst);

  // Use after MergeRegionInfoInto, to propagate merge to neighboring regions.
  // Note: region[current_idx] = -1;
  void NotifyRegionTreeOfIndexChange(int current_idx,
                                     int at_level,
                                     int new_idx,
                                     vector< shared_ptr<RegionInfoList> >* region_tree);

  // Returns true if item was inserted, if it was present already return false.
  template<class T> bool InsertSortedUniquely(const T& t, vector<T>* array) {
    typename vector<T>::iterator insert_pos =
        lower_bound(array->begin(), array->end(), t);
    if (insert_pos == array->end() || *insert_pos != t) {
      array->insert(insert_pos, t);
      return true;
    } else {
      return false;
    }
  }
}

#endif // SEGMANTATION_COMMON_H__
