
#include "canvas_cut.h"
#include <cmath>

#include "GCoptimization.h"
#include "image_util.h"

#define CONSTRAIN_WEIGHT (1 << 12)
#define SOFT_CONSTRAIN 50

CanvasCut::CanvasCut(int width, int height) : width_(width), height_(height) {
  
  frame_diff_.reset(new int[width_ * height_]);
  graph_smoothness_H_.reset(new int[width_ * height_]);
  graph_smoothness_V_.reset(new int[width_ * height_]);
  
      /*
  IppiSize roi = {width_, height_};
  ippiSet_32s_C1R(0, frame_diff_.get(), width_ * sizeof(int), roi);
  ippiSet_32s_C1R(0, graph_smoothness_H_.get(), width_ * sizeof(int), roi);
  ippiSet_32s_C1R(0, graph_smoothness_V_.get(), width_ * sizeof(int), roi);
  */
      
  memset(frame_diff_.get(), 0, width_ * height_ * sizeof(int));
  memset(graph_smoothness_H_.get(), 0, width_ * height_ * sizeof(int));
  memset(graph_smoothness_V_.get(), 0, width_ * height_ * sizeof(int));
      
  graph_data_term_.reset(new int[width_ * height_ * 2]);  
}

void CanvasCut::ProcessImage(IplImage* canvas_window, const uchar* img_data, int img_width_step, 
                             const uchar* mask_ptr, const uchar* to_canvas,
                             const uchar* from_canvas) {
  // Compute data and smoothness term.
  const uchar* canvas_ptr = (uchar*) canvas_window->imageData;
  const uchar* frame_ptr = img_data;
  int* diff_ptr = frame_diff_.get();
  int* data_term_ptr = graph_data_term_.get();
  
  const int canvas_step = canvas_window->widthStep;
  memset(graph_data_term_.get(), 0, width_ * 2 * height_ * sizeof(int));
  
  // Label 0: Take pixel from frame.
  // Label 1: Take pixel from canvas.
  
  // Initialize border to have label 1.
  // Top & bottom.
  int* top_row = graph_data_term_.get();
  int* bottom_row = graph_data_term_.get() + (height_-1) * width_ * 2;
  for (int i = 0; i < width_; ++i, top_row += 2, bottom_row += 2) {
    top_row[0] = CONSTRAIN_WEIGHT;
    bottom_row[0] = CONSTRAIN_WEIGHT;
  }
  
  // Left & right.
  int* left_col = graph_data_term_.get();
  int* right_col = graph_data_term_.get() + 2 * (width_-1);
  for (int i = 0; i < height_; ++i, left_col += 2*width_, right_col += 2*width_) {
    left_col[0] = CONSTRAIN_WEIGHT;
    right_col[0] = CONSTRAIN_WEIGHT;
  }
  
  for (int i = 0; 
       i < height_;
       ++i, canvas_ptr += canvas_step, frame_ptr += img_width_step,
       diff_ptr += width_,
       data_term_ptr += 2*width_) {
    
    const uchar* canvas_row = canvas_ptr;
    const uchar* frame_row = frame_ptr;
    int* diff_row = diff_ptr;
    int* data_term_row = data_term_ptr;
    
    for (int j = 0; 
         j < width_;
         ++j, canvas_row += 4, frame_row += 3, ++diff_row, data_term_row+=2) {
      int r = (int)canvas_row[2] - (int)frame_row[0];
      int g = (int)canvas_row[1] - (int)frame_row[1];
      int b = (int)canvas_row[0] - (int)frame_row[2];
      
      *diff_row = std::sqrt(r*r + g*g + b*b);
      
      if (canvas_row[3] == 0) {       // Canvas element is empty -> use frame instead.
        data_term_row[1] = CONSTRAIN_WEIGHT;
        data_term_row[0] = 0;     // Erase possible earlier border operation.
      }
    }
  }
  
  // Set center to come from current frame.
  if (to_canvas == 0 && from_canvas == 0) {
    const int radius = 25;
    data_term_ptr = graph_data_term_.get() + ((width_ * 2) * height_ / 2) + width_ / 2 * 2;
  
    for (int i = -radius; i <= radius; ++i) {
      int* data_term_row = data_term_ptr + i * width_ * 2;
      data_term_row -= 2*radius;
    
      for (int j = -radius; j <= radius; ++j, data_term_row+=2) {
        data_term_row[1] = 100;
      }
    }
  } else {
    
    if (to_canvas) {
      for (int i = 0; i < height_; ++i) {
        const uchar* mask_ptr = to_canvas + i * width_;
        int* data_ptr = graph_data_term_.get() + 2 * width_ * i;
        for (int j = 0; j < width_; ++j, ++mask_ptr, data_ptr +=2) {
          if (*mask_ptr) {
            data_ptr[0] = 0;
            data_ptr[1] = CONSTRAIN_WEIGHT;
          }
        }
      }
    }
    
    if (from_canvas) {
      for (int i = 0; i < height_; ++i) {
        const uchar* mask_ptr = from_canvas + i * width_;
        int* data_ptr = graph_data_term_.get() + 2 * width_ * i;
        for (int j = 0; j < width_; ++j, ++mask_ptr, data_ptr +=2) {
          if (*mask_ptr) {
            data_ptr[0] = CONSTRAIN_WEIGHT;
            data_ptr[1] = 0;
          }
        }
      }
    }
  }
  
  // Calculate seam smoothness cost from frame difference.        
  for (int i = 0; i < height_; ++i) {
    int* src_ptr = frame_diff_.get() + i * width_;
    int* dst_ptr = graph_smoothness_H_.get() + i * width_;
    
    for (int j = 0; j < width_ - 1; ++j, ++src_ptr, ++dst_ptr) {
      *dst_ptr = src_ptr[0] + src_ptr[1];
    }
  }
  
  for (int i = 0; i < height_ - 1; ++i) {
    int* src_ptr = frame_diff_.get() + i * width_;
    int* src_next_ptr = frame_diff_.get() + (i + 1) * width_;
    int* dst_ptr = graph_smoothness_V_.get() + i * width_;
    
    for (int j = 0; j < width_; ++j, ++src_ptr, ++dst_ptr, ++src_next_ptr) {
      *dst_ptr = src_ptr[0] + src_next_ptr[0];
    }
  }
  
  // Do not carve through foreground (mask_value == 1) 
  if (mask_ptr) {
    for (int i = 0; i < height_; ++i) {
      int* dst_v_ptr = graph_smoothness_V_.get() + i * width_;
      int* dst_h_ptr = graph_smoothness_H_.get() + i * width_;
      for (int j = 0; j < width_; ++j, ++mask_ptr, ++dst_v_ptr, ++dst_h_ptr) {
        if (*mask_ptr > 180) {
          *dst_v_ptr *= (*mask_ptr - 128.0f) / 128.0f * 3;
          *dst_h_ptr *= (*mask_ptr - 128.0f) / 128.0f * 3;
        }
      }
    }
  }
  
  // All foreground objects that were stitched to the canvas should remain on it!
  /*
  if (canvas_foreground_) {
    for (int i = 0; i < height_; ++i) {
      uchar* foreground_ptr = RowPtr<uchar>(canvas_foreground_, canvas_offset.y() +i) 
                                 + canvas_offset.x();
      
      int* data_ptr = graph_data_term_.get() + 2 * width_ * i;
      for (int j = 0; j < width_; ++j, ++foreground_ptr, data_ptr +=2) {
//        if (*foreground_ptr) {
  //        data_ptr[0] = 10;
    //      data_ptr[1] = 0;
    //    }
      }
    }
  }
   */
  
  /*
  if (mask_ptr) {
    for (int i = 0; i < height_; ++i) {
      int* data_ptr = graph_data_term_.get() + 2 * width_ * i;
      for (int j = 0; j < width_; ++j, ++mask_ptr, data_ptr += 2) {
        if (!*mask_ptr) {
          data_ptr[0] = CONSTRAIN_WEIGHT;
          data_ptr[1] = 0;
        }
      }
    }
  }
  */
  
  // Setup graph and solve for optimal cut.
  int label_transistion[4] = { 0, 1, 1, 0 };
  
  gc_.reset(new GCoptimizationGridGraph(width_, height_, 2));
  
  // Setup initial data.
  gc_->setDataCost(graph_data_term_.get());
  
  gc_->setSmoothCostVH(label_transistion, graph_smoothness_H_.get(),
                      graph_smoothness_V_.get());
  
  // Two Label problem. Optimal cut after one iteration.
  gc_->expansion(1);  
}

int CanvasCut::Label(int idx) {
  return gc_->whatLabel(idx);
}


