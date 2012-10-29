
#ifndef CANVAS_CUT_H__
#define CANVAS_CUT_H__

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include <boost/shared_array.hpp>
using boost::shared_array;

#include <QImage>
#include <QPoint>

#include <core/core_c.h>

class GCoptimizationGridGraph;

class CanvasCut {
public:
  CanvasCut(int width, int height);
  void ProcessImage(IplImage* canvas_window, const uchar* img_data, int img_width_step, 
                    const uchar* mask_data = 0, const uchar* to_canvas = 0,
                    const uchar* from_canvas = 0);
  
  int Label(int idx);
  
private:
  int width_;
  int height_;
//  IplImage* canvas_foreground_;
  
  shared_array<int> graph_smoothness_H_;
  shared_array<int> graph_smoothness_V_;
  shared_array<int> frame_diff_;
  
  shared_array<int> graph_data_term_;
  shared_ptr<GCoptimizationGridGraph> gc_;
};

#endif // CANVAS_CUT_H__
