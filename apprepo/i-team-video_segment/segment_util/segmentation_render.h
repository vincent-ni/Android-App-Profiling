#ifndef SEGMENTATION_RENDER_H__
#define SEGMENTATION_RENDER_H__

#include "segmentation_util.h"
#include <boost/unordered_map.hpp>

namespace Segment {

  // Tuple representing a region at a specific level. Used to represent local
  // hierarchy levels.
  struct RegionID {
    RegionID(int id_ = -1, int level_ = 0) : id(id_), level(level_) {};
    int id;
    int level;

    // Lexicographic ordering.
    bool operator<(const RegionID& region) const {
      return level < region.level || (level == region.level && id < region.id);
    }

    bool operator==(const RegionID& region) const {
      return id == region.id && level == region.level;
    }

    bool operator!=(const RegionID& region) const {
      return !(*this == region);
    }
  };

  struct RegionIDHasher {
  public:
    size_t operator()(const RegionID& r_id) const {
      // We don't expect more than 128 hierarchy levels.
      return (r_id.level << (31-8)) + r_id.id; }
  };

  // Functional object to customize RenderRegionsRandomColor.
  class HierarchyColorGenerator {
  public:
    // hierarchy can be zero if hierarchy_level is zero.
    HierarchyColorGenerator(int hierarchy_level,
                            int channels,
                            const Hierarchy* hierarchy);

    // Is called by RenderRegions with each oversegmented region id.
    // Id is mapped to corresponding parent id at the desired hierarchy level
    // and this id is used as color seed to random generate colors.
    // Mapped id needs also to be returned (for draw shape descriptors, etc.)
    // If false is returned, region is not rendered.
    bool operator()(int overseg_region_id,
                    RegionID* mapped_id,
                    unsigned char* colors) const;

  protected:
    const Hierarchy* hierarchy_;
    int hierarchy_level_;
    const int channels_;
  };

  inline int ColorDiff_L1(const char* first, const char* second) {
    return abs((int)first[0] - (int)second[0]) +
    abs((int)first[1] - (int)second[1]) +
    abs((int)first[2] - (int)second[2]);
  }

  template <class ColorGenerator>
  void RenderRegions(char* img,
                     const int width_step,
                     const int width,
                     const int height,
                     const int channels,
                     bool highlight_boundary,
                     bool draw_shape_descriptors,
                     const SegmentationDesc& seg,
                     const ColorGenerator& generator) {
    // Parent map: map parent id to vector of children id.
    typedef boost::unordered_map<RegionID, vector<int>, RegionIDHasher> RegionIDMap;
    RegionIDMap parent_map;
    vector<unsigned char> color(channels);

    // Traverse regions.
    const RepeatedPtrField<Region2D>& regions = seg.region();
    for(RepeatedPtrField<Region2D>::const_iterator r = regions.begin();
        r != regions.end();
        ++r) {
      // Get color.
      RegionID mapped_id;
      if (!generator(r->id(), &mapped_id, &color[0])) {
        continue;
      }

      parent_map[mapped_id].push_back(r->id());

      for(RepeatedPtrField<ScanInterval>::const_iterator s =
          r->raster().scan_inter().begin();
          s != r->raster().scan_inter().end();
          ++s) {
        const int curr_y = s->y();
        char* out_ptr = img + width_step * curr_y + s->left_x() * channels;
        for (int j = 0, len = s->right_x() - s->left_x() + 1;
             j < len;
             ++j, out_ptr += channels) {
          for (int c = 0; c < channels; ++c) {
            out_ptr[c] = color[c];
          }
        }
      }
    }

    // Edge highlight post-process.
    if (highlight_boundary) {
      for (int i = 0; i < height - 1; ++i) {
        char* row_ptr = img + i * width_step;
        for (int j = 0; j < width - 1; ++j, row_ptr += channels) {
          if (ColorDiff_L1(row_ptr, row_ptr + channels) != 0 ||
              ColorDiff_L1(row_ptr, row_ptr + width_step) != 0)
            row_ptr[0] = row_ptr[1] = row_ptr[2] = 0;
        }

        // Last column.
        if (ColorDiff_L1(row_ptr, row_ptr + width_step) != 0)
          row_ptr[0] = row_ptr[1] = row_ptr[2] = 0;
      }

      // Last row.
      char* row_ptr = img + width_step * (height - 1);
      for (int j = 0; j < width - 1; ++j, row_ptr += channels) {
        if (ColorDiff_L1(row_ptr, row_ptr + channels) != 0)
          row_ptr[0] = row_ptr[1] = row_ptr[2] = 0;
      }
    }

    if (draw_shape_descriptors) {
      for (RegionIDMap::const_iterator parent = parent_map.begin();
           parent != parent_map.end();
           ++parent) {
        int parent_id = parent->first.id;
        DrawShapeDescriptors(parent->second, seg, img, width_step, width, height,
                             channels, &parent_id);
      }
    }
  }


  // Renders each region with a random color for 3-channel 8-bit input image.
  // If highlight_boundary is set, region boundary will be colored black.
  inline
  void RenderRegionsRandomColor(char* img,
                                int width_step,
                                int width,
                                int height,
                                int channels,
                                int hierarchy_level,
                                bool highlight_boundary,
                                bool draw_shape_descriptors,
                                const SegmentationDesc& desc,
                                const Hierarchy* seg_hier) {
    // Clear image.
    memset(img, 0, width_step * height);

    RenderRegions(img, width_step, width, height, channels, highlight_boundary,
                  draw_shape_descriptors, desc,
                  HierarchyColorGenerator(hierarchy_level, channels, seg_hier));
  }

}  // namespace Segment.

#endif // SEGMENTATION_RENDER_H__
