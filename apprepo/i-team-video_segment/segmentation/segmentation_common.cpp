/*
 *  segmentation_common.cpp
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 1/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "segmentation_common.h"

#include <iostream>

#include <boost/pending/disjoint_sets.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>
using boost::tuple;
using boost::get;
using boost::lexical_cast;
#include <fstream>

#include <opencv2/core/core_c.h>
#include <google/protobuf/repeated_field.h>
using google::protobuf::RepeatedPtrField;
using google::protobuf::RepeatedField;

#include "assert_log.h"
#include "image_util.h"
#include "segmentation_util.h"

using namespace ImageFilter;

namespace Segment {

  typedef SegmentationDesc::Region2D SegRegion;

  bool ImageHeader::IsEqual(const GenericHeader& rhs_header) const {
    const ImageHeader& rhs = dynamic_cast<const ImageHeader&>(rhs_header);
    return img_id_ == rhs.img_id_;
  }

  string ImageHeader::ToString() const {
    return lexical_cast<string>(img_id_);
  }

  void ImageBody::WriteToStream(std::ofstream& ofs) const {
    ofs.write(reinterpret_cast<char*>(&img_->width), sizeof(img_->width));
    ofs.write(reinterpret_cast<char*>(&img_->height), sizeof(img_->height));
    ofs.write(reinterpret_cast<char*>(&img_->nChannels), sizeof(img_->nChannels));
    ofs.write(reinterpret_cast<char*>(&img_->depth), sizeof(img_->depth));
    ofs.write(reinterpret_cast<char*>(&img_->widthStep), sizeof(img_->widthStep));

    ofs.write(img_->imageData, img_->widthStep * img_->height);
  }

  void ImageBody::ReadFromStream(std::ifstream& ifs) {
    int frame_width, frame_height, channels, depth, widthStep;
    ifs.read(reinterpret_cast<char*>(&frame_width), sizeof(frame_width));
    ifs.read(reinterpret_cast<char*>(&frame_height), sizeof(frame_height));
    ifs.read(reinterpret_cast<char*>(&channels), sizeof(channels));
    ifs.read(reinterpret_cast<char*>(&depth), sizeof(depth));
    ifs.read(reinterpret_cast<char*>(&widthStep), sizeof(widthStep));

    img_ = cvCreateImageShared(frame_width, frame_height, depth, channels);
    assert(img_->widthStep == widthStep);

    ifs.read(img_->imageData, widthStep * frame_height);
  }

  /*
  void MergeScanlineReps(const vector<RegionScanlineRep>& lhs,
                         const vector<RegionScanlineRep>& rhs,
                         vector<RegionScanlineRep>* out) {
    int i = 0;
    int j = 0;

    const int lhs_sz = lhs.size();
    const int rhs_sz = rhs.size();

    out->clear();
    out->reserve(lhs_sz + rhs_sz);

    while (i < lhs_sz && j < rhs_sz) {
      if (lhs[i].frame < rhs[j].frame) {
        out->push_back(lhs[i++]);
      } else if (lhs[i].frame > rhs[j].frame) {
        out->push_back(rhs[j++]);
      } else {
        // Same frame number. Merge frame slice.
        const int lhs_top = lhs[i].top_y;
        const int rhs_top = rhs[j].top_y;

        RegionScanlineRep merged_rep(lhs[i].frame, std::min(lhs_top, rhs_top));

        int k = 0;
        int l = 0;

        const vector<IntervalList>& lhs_scan = lhs[i].scanline;
        const vector<IntervalList>& rhs_scan = rhs[j].scanline;

        const int sz_k = lhs_scan.size();
        const int sz_l = rhs_scan.size();

        merged_rep.scanline.reserve(sz_k + sz_l);

        while (k < sz_k &&
               lhs_top + k < rhs_top) {
          merged_rep.scanline.push_back(lhs_scan[k++]);
        }

        while (l < sz_l &&
               rhs_top + l < lhs_top) {
          merged_rep.scanline.push_back(rhs_scan[l++]);
        }

        // Both k and l are pointing to the same scanline from now on.
        while (k < sz_k && l < sz_l) {
          // k and l are pointing to the same scanline.
          // Merge scanline intervals.
          const IntervalList& lhs_interval_list = lhs_scan[k];
          const IntervalList& rhs_interval_list = rhs_scan[l];

          assert(lhs_top + k == rhs_top + l);

          int m = 0;
          int n = 0;
          const int sz_m = lhs_interval_list.size();
          const int sz_n = rhs_interval_list.size();

          IntervalList merged_inter;
          merged_inter.reserve(sz_m + sz_n);

          while (m < sz_m && n < sz_n) {
            if (lhs_interval_list[m].second + 1 < rhs_interval_list[n].first) {
              merged_inter.push_back(lhs_interval_list[m++]);
            } else if (rhs_interval_list[n].second + 1 < lhs_interval_list[m].first) {
              merged_inter.push_back(rhs_interval_list[n++]);
            } else {
              assert (lhs_interval_list[m].second + 1 == rhs_interval_list[n].first ||
                      lhs_interval_list[m].first == rhs_interval_list[n].second + 1);

              // Someones first is the others second -- Merge interval.
              std::pair<int, int> merged_interval =
              std::make_pair(std::min(lhs_interval_list[m].first, rhs_interval_list[n].first),
                             std::max(lhs_interval_list[m].second, rhs_interval_list[n].second));
              ++m;
              ++n;

              while (true) {
                // if (m < sz_m)
                //  assert (lhs_interval_list[m].first > merged_interval.second);

                // if (n < sz_n)
                //  assert(rhs_interval_list[n].first > merged_interval.second);

                if (m < sz_m && lhs_interval_list[m].first == merged_interval.second + 1)
                  merged_interval.second = lhs_interval_list[m++].second;
                else if (n < sz_n && rhs_interval_list[n].first == merged_interval.second + 1)
                  merged_interval.second = rhs_interval_list[n++].second;
                else
                  break;
              }

              merged_inter.push_back(merged_interval);
            }
          }

          // Add remaining intervals.
          while (m < sz_m)
            merged_inter.push_back(lhs_interval_list[m++]);
          while (n < sz_n)
            merged_inter.push_back(rhs_interval_list[n++]);

          merged_rep.scanline.push_back(merged_inter);
          ++k;
          ++l;
        }

        // Process remaining scanlines.
        if (k < sz_k) {
          // Add empty scanlines in case this it is needed.
          while (merged_rep.top_y + merged_rep.scanline.size() < lhs_top + k)
            merged_rep.scanline.push_back(IntervalList());

          while (k < sz_k)
            merged_rep.scanline.push_back(lhs_scan[k++]);
        }

        if (l < sz_l) {
          while(merged_rep.top_y + merged_rep.scanline.size() < rhs_top + l)
            merged_rep.scanline.push_back(IntervalList());

          while (l < sz_l)
            merged_rep.scanline.push_back(rhs_scan[l++]);
        }

        out->push_back(merged_rep);
        ++i;
        ++j;
      }
    }

    // Append remaining slices.
    while (i < lhs_sz)
      out->push_back(lhs[i++]);
    while (j < rhs_sz)
      out->push_back(rhs[j++]);
  }

   */

  void RegionInformation::DescriptorDistances(const RegionInformation& rhs,
                                              vector<float>* distances) const {
    /*
    float color =  hist->ChiSquareDist(*rhs.hist);
    //hist->JSDivergence(*rhs.hist);
   // hist->ChiSquareDist(*rhs.hist);
    //std::min(1.f, hist->KLDivergence(*rhs.hist) * 0.1f);
    float gradient = 0.0f; // gra_hist->ChiSquareDist(*rhs.gra_hist);
    float flow = 0; //FlowDistance(rhs);

    float merged = 1.0f - (1.0f - color) * (1.0f - gradient) * (1.0f - flow);
   // std::cerr <<  merged * merged << "\n";
    return merged * merged;
    */

    ASSURE_LOG(region_descriptors_.size() == rhs.region_descriptors_.size());
    ASSERT_LOG(distances->size() == region_descriptors_.size());
    for (int idx = 0; idx < region_descriptors_.size(); ++idx) {
      (*distances)[idx] =
          region_descriptors_[idx]->RegionDistance(*rhs.region_descriptors_[idx]);
    }
  }

  void RegionInformation::AddRegionDescriptor(RegionDescriptor* desc) {
    region_descriptors_.push_back(shared_ptr<RegionDescriptor>(desc));
  }

  void RegionInformation::PopulatingDescriptorsFinished() {
    for (vector<shared_ptr<RegionDescriptor> >::iterator descriptor =
             region_descriptors_.begin();
         descriptor != region_descriptors_.end();
         ++descriptor) {
      if (*descriptor != NULL) {
        (*descriptor)->PopulatingDescriptorFinished();
      }
    }
  }

  void RegionInformation::MergeDescriptorsFrom(const RegionInformation& rhs) {
    // This could be a new super-region (empty descriptors). Allocate in that case.
    if (region_descriptors_.size() < rhs.region_descriptors_.size()) {
      ASSURE_LOG(region_descriptors_.empty());
      region_descriptors_.resize(rhs.region_descriptors_.size());
    }

    for (int descriptor_idx = 0;
         descriptor_idx < rhs.region_descriptors_.size();
         ++descriptor_idx) {
      ASSURE_LOG(rhs.region_descriptors_[descriptor_idx] != NULL);
      if (region_descriptors_[descriptor_idx] == NULL) {
        region_descriptors_[descriptor_idx].reset(
            rhs.region_descriptors_[descriptor_idx]->Clone());
      } else {
        region_descriptors_[descriptor_idx]->MergeWithDescriptor(
            *rhs.region_descriptors_[descriptor_idx]);
      }
    }
  }

  void RegionInformation::OutputRegionFeatures(
      RegionFeatures* features) const {
    for (vector<shared_ptr<RegionDescriptor> >::const_iterator descriptor =
             region_descriptors_.begin();
         descriptor != region_descriptors_.end();
         ++descriptor) {
      (*descriptor)->AddToRegionFeatures(features);
    }
  }

  void MergeRegionInfoInto(const RegionInformation* src, RegionInformation* dst) {
    // Update area.
    dst->size += src->size;

    // Merge neighbor ids, avoid duplicates.
    vector<int> merged_neighbors;
    merged_neighbors.reserve(src->neighbor_idx.size() + dst->neighbor_idx.size());
    std::set_union(src->neighbor_idx.begin(), src->neighbor_idx.end(),
                   dst->neighbor_idx.begin(), dst->neighbor_idx.end(),
                   std::back_inserter(merged_neighbors));

    // Avoid adding dst->region_id to merged_neighbors.
    vector<int>::iterator dst_id_location =
      std::lower_bound(merged_neighbors.begin(),
                       merged_neighbors.end(),
                       dst->index);

    if (dst_id_location != merged_neighbors.end() &&
        *dst_id_location == dst->index) {
      merged_neighbors.erase(dst_id_location);
    }

    dst->neighbor_idx.swap(merged_neighbors);

    // Merge scanline representation.
    if (src->raster != NULL) {
      // Both RegionInfo's need to have rasterization present.
      ASSURE_LOG(dst->raster != NULL);
      shared_ptr<Rasterization3D> merged_raster(new Rasterization3D());
      MergeRasterization3D(*src->raster, *dst->raster, merged_raster.get());
      dst->raster.swap(merged_raster);
    }

    // Merge children.
    if (src->child_idx != NULL) {
      ASSURE_LOG(dst->child_idx != NULL);
      vector<int> merged_children;
      merged_children.reserve(src->child_idx->size() + dst->child_idx->size());
      std::set_union(src->child_idx->begin(), src->child_idx->end(),
                     dst->child_idx->begin(), dst->child_idx->end(),
                     back_inserter(merged_children));
      dst->child_idx->swap(merged_children);
    }

    // Merge histograms.
    dst->MergeDescriptorsFrom(*src);
  }

  void PerformRegionTreeIdxChange(int current_idx,
                                  int at_level,
                                  int new_idx,
                                 vector< shared_ptr<RegionInfoList> >* region_tree) {
    // Fetch RegionInformation from region_tree.
    ASSURE_LOG(region_tree);
    ASSURE_LOG(at_level < region_tree->size());
    ASSURE_LOG(current_idx < (*region_tree)[at_level]->size());

    RegionInformation* region_info = (*region_tree)[at_level]->at(current_idx).get();

    // Notify neighbor's of id change.
    for (vector<int>::const_iterator neighbor_idx = region_info->neighbor_idx.begin();
         neighbor_idx != region_info->neighbor_idx.end();
         ++neighbor_idx) {
      RegionInformation* neighbor_info = (*region_tree)[at_level]->at(*neighbor_idx).get();
      // Find current_idx.
      vector<int>::iterator idx_pos = std::lower_bound(neighbor_info->neighbor_idx.begin(),
                                                       neighbor_info->neighbor_idx.end(),
                                                       current_idx);
      ASSERT_LOG(*idx_pos == current_idx);
      // Erase and insert new_id, except if new_id and neighbor's id are identical
      // (no region should be neighbor of itself). In this case only erase.
      neighbor_info->neighbor_idx.erase(idx_pos);

      if (neighbor_info->index != new_idx) {
        vector<int>::iterator insert_pos =
            std::lower_bound(neighbor_info->neighbor_idx.begin(),
                             neighbor_info->neighbor_idx.end(),
                             new_idx);

        // Only insert if not already neighbors. (Avoid duplicates).
        if (insert_pos == neighbor_info->neighbor_idx.end() ||
            *insert_pos != new_idx) {
          neighbor_info->neighbor_idx.insert(insert_pos, new_idx);
        }
      }
    }

    // Notify children of id change.
    if (region_info->child_idx != NULL) {
      for (vector<int>::const_iterator child_idx = region_info->child_idx->begin();
           child_idx != region_info->child_idx->end();
           ++child_idx) {
        RegionInformation* child_info = (*region_tree)[at_level - 1]->at(*child_idx).get();
        ASSERT_LOG(child_info->parent_idx == current_idx);
        child_info->parent_idx = new_idx;
      }
    }

    // Notify parent of id change.
    if (region_info->parent_idx >= 0) {
      RegionInformation* parent_info =
          (*region_tree)[at_level + 1]->at(region_info->parent_idx).get();

      // Find current id in parent's children.
      ASSERT_LOG(parent_info->child_idx);

      vector<int>::iterator child_pos = std::lower_bound(parent_info->child_idx->begin(),
                                                         parent_info->child_idx->end(),
                                                         current_idx);
      ASSERT_LOG(*child_pos == current_idx);

      // Erase and insert new id.
      parent_info->child_idx->erase(child_pos);

      vector<int>::iterator insert_pos = std::lower_bound(parent_info->child_idx->begin(),
                                                          parent_info->child_idx->end(),
                                                          new_idx);
      parent_info->child_idx->insert(insert_pos, new_idx);
    }

    // Perform actual id change for this region.
    region_info->index = -1;
  }
}
