#ifndef SEGMENTATION_CONTOUR_H__
#define SEGMENTATION_CONTOUR_H__

#include <vector>
#include <core/core.hpp>

namespace Segment {

  struct Vertex {
    Point2i pt;
    int order;
    
  }
  
  class Contour {
    
  }
  
  class Contourizer {
   public:
    // Freeman coding scheme. Direction of next pixel w.r.t. current one P
    // 3 2 1
    // 4 P 0
    // 5 6 7
    
    
    
   private: 
    vector<Point2i> freeman_map_;
    vector<Point2i> inv_freeman_map_;
  };

}  // Segment.

#endif   // #ifndef SEGMENTATION_CONTOUR_H__