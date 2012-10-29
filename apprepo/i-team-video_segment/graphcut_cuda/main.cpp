/*
 *  main.cpp
 *  graphcut
 *
 *  Created by Matthias Grundmann on 5/30/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <highgui.h>
#include <iostream>
#include "../ImageFilterLib/image_util.h"
#include "../ImageFilterLib/assert_log.h"
#include "CudaCuts.h"

CvPoint cvPointAdd(const CvPoint& lhs, const CvPoint& rhs) {
  return cvPoint(lhs.x + rhs.x, lhs.y + rhs.y);
}

CvPoint cvPointSub(const CvPoint& lhs, const CvPoint& rhs) {
  return cvPoint(lhs.x - rhs.x, lhs.y - rhs.y);
}

int main() {
  
  // Load input images.
  IplImage* img_1_orig = cvLoadImage("cactus050.png"); 
  ASSURE_LOG(img_1_orig) << "Failed loading image 1.";
  IplImage* img_2_orig = cvLoadImage("cactus060.png"); 
  ASSURE_LOG(img_2_orig) << "Failed loading image 2.";
  
  // Determine overlap.
    
  CvPoint tl_1 = cvPoint(0, 0);
  CvPoint tl_2 = cvPoint(-38, -37);
  CvPoint frame_sz = cvPoint(img_1_orig->width, img_1_orig->height);
  
  CvPoint br_1 = cvPointAdd(tl_1, frame_sz);
  CvPoint br_2 = cvPointAdd(tl_2, frame_sz);
  
  CvPoint top_left = cvPoint(std::max(tl_1.x, tl_2.x), std::max(tl_1.y, tl_2.y));
  CvPoint bottom_right = cvPoint(std::min(br_1.x, br_2.x), std::min(br_1.y, br_2.y));
  
  // Compute ROIs
  CvPoint roi = { bottom_right.x - top_left.x, bottom_right.y - top_left.y };
  
  CvPoint local_1 = cvPointSub(top_left, tl_1);
  CvPoint local_2 = cvPointSub(top_left, tl_2);
  
  assert(roi.y > 0 && roi.x > 0);
  assert(local_1.x >= 0 && local_1.x + roi.x <= frame_sz.x);
  assert(local_2.x >= 0 && local_2.x + roi.x <= frame_sz.x);
  assert(local_1.y >= 0 && local_1.y + roi.y <= frame_sz.y);
  assert(local_2.y >= 0 && local_2.y + roi.y <= frame_sz.y);
  
  const char* src_1 = img_1_orig->imageData + img_1_orig->widthStep * local_1.y + 3 * local_1.x;
  const char* src_2 = img_2_orig->imageData + img_2_orig->widthStep * local_2.y + 3 * local_2.x;

  CvMat* frame_diff = cvCreateMat(roi.y, roi.x, CV_32S);
  int* dst_ptr = frame_diff->data.i;
  
  for (int i = 0; 
       i < roi.y; 
       ++i, src_1 += img_1_orig->widthStep, src_2 += img_2_orig->widthStep, 
       dst_ptr = PtrOffset(dst_ptr, frame_diff->step)) {
    const uchar* src_row_i = (uchar*)src_1;
    const uchar* src_row_j = (uchar*)src_2;
    int* dst_row_ptr = dst_ptr;
    
    for (int j = 0; j < roi.x; ++j, src_row_i += 3, src_row_j += 3, ++dst_row_ptr) {
      int r = (int)src_row_i[0] - (int)src_row_j[0];
      int g = (int)src_row_i[1] - (int)src_row_j[1];
      int b = (int)src_row_i[2] - (int)src_row_j[2];
      *dst_row_ptr = sqrt((r*r + g*g + b*b)) + 50;
    }
  }
  
  // Add along both dimensions.
  CvMat* node_diff_H = cvCreateMat(roi.y, roi.x, CV_32S);
  CvMat* node_diff_V = cvCreateMat(roi.y, roi.x, CV_32S);

  for (int i = 0; i < roi.y; ++i) {
    int* src_ptr = RowPtr<int>(frame_diff, i);
    int* dst_ptr = RowPtr<int>(node_diff_H, i);
    
    for (int j = 0; j < roi.x - 1; ++j, ++src_ptr, ++dst_ptr) {
      *dst_ptr = src_ptr[0] + src_ptr[1];
    }
  }
  
  for (int i = 0; i < roi.y-1; ++i) {
    int* src_ptr = RowPtr<int>(frame_diff, i);
    int* src_next_ptr = RowPtr<int>(frame_diff, i+1);
    
    int* dst_ptr = RowPtr<int>(node_diff_V, i);
    
    for (int j = 0; j < roi.x; ++j, ++src_ptr, ++dst_ptr, ++src_next_ptr) {
      *dst_ptr = src_ptr[0] + src_next_ptr[0];
    }
  }
  
  int label_transistion[4] = { 0, 1, 1, 0 };

  int err_code;
  err_code = cudaCutsInit(roi.x, roi.y, 2);
  ASSURE_LOG(err_code > 0) << "Init failed. CUDA probably not supported.\n";
  
//  CvMat* data_cost = cvCreateMat(roi.y, roi.x*2, CV_32S);
  CvMat* data_cost = cvCreateMat(roi.y, roi.x*2, CV_32S);
  cvSet(data_cost, cvScalar(0));
  /*
  for (int i = 0; i < roi.y; ++i) {
    int* data_ptr = RowPtr<int>(data_cost, i);
    for (int j = 0; j < roi.x * 2; ++j, ++data_ptr) {
      *data_ptr = rand() % 100;
    }
  }
  */
  // Set top and left border to label 0 (= label 1 high cost).
  
  for (int x = 0; x < roi.x-1; ++x) 
    data_cost->data.i[2 * x + 1] = 1 << 20;
  
  for (int y = 0; y < roi.y-1; ++y) 
    RowPtr<int>(data_cost, y)[1] = 1 << 20;

  // Set right and bottom border to label 1 (= label 0 high cost).
  for (int x = 1; x < roi.x; ++x) 
    RowPtr<int>(data_cost, roi.y-1)[2*x] = 1 << 20;
  
  for (int y = 1; y < roi.y; ++y) 
    RowPtr<int>(data_cost, y)[2*(roi.x-1)] = 1 << 20;
  
  err_code = cudaCutsSetupDataTerm(data_cost->data.i);
  ASSURE_LOG(!err_code) << "Data cost setup failed.";
  
  err_code = cudaCutsSetupSmoothTerm(label_transistion);
  ASSURE_LOG(!err_code) << "Transition term setup failed.";
  
  err_code = cudaCutsSetupHCue(node_diff_H->data.i);
  ASSURE_LOG(!err_code) << "Horizontal smoothness cost setup failed.";
  
  err_code = cudaCutsSetupVCue(node_diff_V->data.i);
  ASSURE_LOG(!err_code) << "Vertical smoothness cost setup failed.";
  
  err_code = cudaCutsSetupGraph();
  ASSURE_LOG(!err_code) << "Graph setup failed.";
  
//  printf("\nBefore optimization energy is %d\n", cudaCutsGetEnergy());
  err_code = cudaCutsNonAtomicOptimize();
  ASSURE_LOG(!err_code) << "Optimization failed.";
  
  err_code = cudaCutsGetResult();
  ASSURE_LOG(!err_code) << "Getting results failed.";
  
  IplImage* output = cvCreateImage(cvSize(roi.x, roi.y), IPL_DEPTH_8U, 1);
  const int* label_result = PixelLabel();
  

  // TODO: TEST remove 
  /*
  printf("\nTEST: Before change optimization energy is %d\n", cudaCutsGetEnergy());
  
  for (int x = 0; x < roi.x-1; ++x) {
    PixelLabel()[0] = 0;
    //    gc->setLabel(x, 0);
  }
  
  for (int y = 0; y < roi.y-1; ++y) {
    PixelLabel()[GridWidth() * y] = 0;
    //gc->setLabel(roi.x * y, 0);
  }
  
  // Set right and bottom border to label 1 (= label 0 high cost).
  for (int x = 1; x < roi.x; ++x) {
    PixelLabel()[GridWidth() * (roi.y-1) + x] = 1;
    //  gc->setLabel(roi.x * (roi.y-1) + x, 1);
  }
  
  for (int y = 1; y < roi.y; ++y) {
    PixelLabel()[GridWidth() * y + roi.x-1] = 1;
    //    gc->setLabel(roi.x * y + roi.x-1, 1);
  }  
  // end remove.
   */
  
  
  printf("\nAfter optimization energy is %d\n", cudaCutsGetEnergy());
  
  for (int y = 0; y < roi.y; ++y) {
    uchar* out_ptr = RowPtr<uchar>(output, y);
    for (int x = 0; x < roi.x; ++x, ++out_ptr) {
      *out_ptr = 255 * label_result[GridWidth()*y+x];
    }
  }
  
  cudaCutsFreeMem();
  
  cvSaveImage("graph_cut_result.png", output);
  
  return 0;
}
