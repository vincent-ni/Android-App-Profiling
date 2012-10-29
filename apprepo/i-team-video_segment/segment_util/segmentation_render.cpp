#include "segmentation_render.h"

#include "assert_log.h"
typedef unsigned char uchar;

namespace Segment {
  HierarchyColorGenerator::HierarchyColorGenerator(int hierarchy_level,
                                                   int channels,
                                                   const Hierarchy* hierarchy)
  : hierarchy_(hierarchy),
  hierarchy_level_(hierarchy_level),
  channels_(channels) {
    if (hierarchy_level > 0 && hierarchy == NULL) {
      hierarchy_level_ = 0;
      LOG(WARNING) << "Requested level > 0, but hierarchy is NULL. Truncated to zero.";
    }

    // Might to truncate desired level.
    if (hierarchy != NULL && hierarchy_level_ >= hierarchy_->size()) {
      hierarchy_level_ = hierarchy_->size() - 1;
      LOG(WARNING) << "Requested level " << hierarchy_level << " not present in "
      << "hierarchy. Truncated to " << hierarchy_level_ << "\n";
    }
  }

  bool HierarchyColorGenerator::operator()(int overseg_region_id,
                                           RegionID* mapped_id,
                                           uchar* colors) const {
    ASSERT_LOG(mapped_id);
    ASSERT_LOG(colors);
    int region_id = overseg_region_id;

    if (hierarchy_level_ > 0) {
      region_id = GetParentId(overseg_region_id, 0, hierarchy_level_, *hierarchy_);
    }

    *mapped_id = RegionID(region_id, hierarchy_level_);

    srand(region_id);
    for (int c = 0; c < channels_; ++c) {
      colors[c] = (uchar) (rand() % 255);
    }

    return true;
  }
}  // namespace Segment.
