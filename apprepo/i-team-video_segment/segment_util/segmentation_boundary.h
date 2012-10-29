#ifndef SEGMENTATION_BOUNDARY_H__
#define SEGMENTATION_BOUNDARY_H__

#include "segmentation_util.h"

namespace Segment {

  // Graph traversal operations.
  // Note region id is not index of region in protobuffer necessarily.
  struct BoundaryPoint {
    BoundaryPoint() : x(0), y(0) {}
    BoundaryPoint(int _x, int _y) : x(_x), y(_y) {}
    int x;
    int y;
  };

  // Lexicographic compare.
  class BoundaryPointComparator
      : public std::binary_function<bool, BoundaryPoint, BoundaryPoint> {
  public:
    bool operator()(const BoundaryPoint& lhs, const BoundaryPoint& rhs) {
      return (lhs.x < rhs.x) || (lhs.x == rhs.x && lhs.y < rhs.y);
    }
  };

  // A region boundary is always sorted w.r.t. lexicographic order.
  typedef vector<BoundaryPoint> RegionBoundary;

  // Returns boundary for a region by internally rendering the region and evaluating
  // for each pixel if it is a boundary pixel. Uses N4 neighborhood, i.e. a boundary point
  // is an inner (outer) boundary point, if the point is a region (non-region) pixel
  // neighboring to a non-region (region) pixel.
  // Uses a temporary buffer of size 3 * (frame_width + 2).
  void GetBoundary(const Region2D& region,
                   int frame_width,
                   bool inner_boundary,
                   vector<uchar>* buffer,
                   RegionBoundary* boundary);

  // Same as above for union of points.
  void GetBoundary(const vector<const Region2D*>& regions,
                   int frame_width,
                   bool inner_boundary,
                   vector<uchar>* buffer,
                   RegionBoundary* boundary);

}  // namespace Segment.

#endif  // SEGMENTATION_BOUNDARY_H__
