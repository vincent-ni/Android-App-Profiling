
#include "segmentation_boundary.h"
#include "assert_log.h"

namespace Segment {

  namespace {
    // Sets buffer to one for each pixel of the scanline.
    // Returns x-range of rendered pixels.
    std::pair<int, int> RenderScanInterval(const ScanInterval& scan_inter,
                                           uchar* buffer) {
      std::pair<int, int> range(1e6, -1e6);
      const int interval_sz = scan_inter.right_x() - scan_inter.left_x() + 1;
      range.first = std::min<int>(range.first, scan_inter.left_x());
      range.second = std::min<int>(range.second, scan_inter.right_x());
      memset(buffer + scan_inter.left_x(), 1, interval_sz);
      return range;
    }

    std::pair<int, int> RenderScanlines(const vector<const Region2D*>& regions,
                                        int y,
                                        uchar* buffer) {
      std::pair<int, int> range(1e6, -1e6);
      for (vector<const Region2D*>::const_iterator r = regions.begin();
           r != regions.end();
           ++r) {
        RepeatedPtrField<ScanInterval>::const_iterator s =
        LocateScanLine(y, (*r)->raster());
        while (s != (*r)->raster().scan_inter().end() &&
               s->y() == y) {
          std::pair<int, int> ret_range =
          RenderScanInterval(*s, buffer);
          range.first = std::min<int>(range.first, ret_range.first);
          range.second = std::max<int>(range.second, ret_range.second);
        }
      }
      return range;
    }
  }

  void GetBoundary(const SegmentationDesc::Region2D& region,
                   int frame_width,
                   bool inner_boundary,
                   vector<uchar>* buffer,
                   RegionBoundary* boundary) {
    vector<const SegmentationDesc::Region2D*> regions;
    regions.push_back(&region);
    GetBoundary(regions, frame_width, inner_boundary, buffer, boundary);
  }

  void GetBoundary(const vector<const Region2D*>& regions,
                   int frame_width,
                   bool inner_boundary,
                   vector<uchar>* buffer,
                   RegionBoundary* boundary) {
    const int rad = 1;
    const int width_step = frame_width + 2 * rad;
    ASSERT_LOG(buffer->size() >= 3 * width_step);

    memset(buffer, 0, 3 * width_step);

    // Set up scanline pointers.
    uchar* prev_ptr = &(*buffer)[rad];
    uchar* curr_ptr = &(*buffer)[rad + width_step];
    uchar* next_ptr = &(*buffer)[rad + 2 * width_step];

    // Determine min and max y.
    int min_y = 1e7;
    int max_y = -1e7;

    for (vector<const Region2D*>::const_iterator r = regions.begin();
         r != regions.end();
         ++r) {
      min_y = std::min<int>((*r)->raster().scan_inter(0).y(), min_y);
      max_y = std::max<int>((*r)->raster().scan_inter(
                                                      (*r)->raster().scan_inter_size() - 1).y(), max_y);
    }

    // Render first scanline.
    vector<std::pair<int, int> > ranges;
    if (inner_boundary) {
      ranges.push_back(RenderScanlines(regions, min_y, curr_ptr));
    }

    int shift = inner_boundary ? 0 : 1;

    for (int y = min_y - shift; y <= max_y + shift; ++y) {
      if (y + 1 <= max_y) {
        ranges.push_back(RenderScanlines(regions, y + 1, next_ptr));
      }

      int min_range = 1e7;
      int max_range = -1e7;

      for (int k = 0; k < ranges.size(); ++k) {
        min_range = std::min(min_range, ranges[k].first);
        max_range = std::max(max_range, ranges[k].second);
      }

      const uchar* prev_x_ptr = prev_ptr + min_range - shift;
      const uchar* curr_x_ptr = curr_ptr + min_range - shift;
      const uchar* next_x_ptr = next_ptr + min_range - shift;

      for (int x = min_range;
           x <= max_range + shift;
           ++x, ++prev_x_ptr, ++curr_x_ptr, ++next_x_ptr) {
        // A point is a boundary point if the current value for it is set,
        // but one of its 4 neighbors is not (inner_boundary, else inverted criteria).
        if (inner_boundary) {
          if (curr_x_ptr[0] &&
              (!curr_x_ptr[-1] || !curr_x_ptr[1] || !prev_x_ptr[0] || !next_x_ptr[0])) {
            boundary->push_back(BoundaryPoint(x, y));
          }
        } else {
          if (!curr_x_ptr[0] &&
              (curr_x_ptr[-1] || curr_x_ptr[1] || prev_x_ptr[0] || next_x_ptr[0])) {
            boundary->push_back(BoundaryPoint(x, y));
          }
        }
      }

      // Swap with wrap around.
      std::swap(prev_ptr, curr_ptr);
      std::swap(curr_ptr, next_ptr);
      if (ranges.size() >= 3) {
        ranges.erase(ranges.begin());
      }
    }
  }
};
