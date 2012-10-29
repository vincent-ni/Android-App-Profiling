/*
 *  main.cpp
 *  
 *
 *  Created by Matthias Grundmann on 5/24/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */


#include "nnf.h"
#include <cv.h>
#include <highgui.h>
#include "grundmann_common.h"
#include <omp.h>

using namespace ImageFilter;

int main() {
  // Load Sift from file.
//  const int width = 234;
//  const int height = 314;  
  const int width = 202;
  const int height = 219;
  
  // Source file.
  std::ifstream ifs_sift1("sift1.txt", std::ios_base::in);
  vector<SIFT_DESC> sift1(width * height);
  
  for (int i = 0, idx = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j, ++idx) {
      SIFT_DESC sift_desc;
      float norm = 0;
      
      for (int k = 0; k < SIFT_DESC::static_size; ++k) {
        ifs_sift1 >> sift_desc[k];
        norm += sift_desc[k] * sift_desc[k];
      }
          
     // ASSERT_LOG(!ifs_sift1.tellg() >= 0);
      ASSERT_LOG(fabs(norm - 1.f) < 0.01f) << "SIFT descriptor needs to be normalized.\n";
      sift1[idx] = sift_desc;
    }
  }
  
  ifs_sift1.close();
  
  // Reference file.
  // Pad file such that we have a radius of rad.
  // Holds marker descriptor with large value to signal border operation.
  SIFT_DESC invalid_desc;
  for (int i = 0; i < SIFT_DESC::static_size; ++i)
    invalid_desc[i] = 1e10;     // Sq. will be used.
  
  const int rad = 1;
  std::ifstream ifs_sift2("sift2.txt", std::ios_base::in);
  vector<SIFT_DESC> sift2((width + 2 * rad) * (height + 2 * rad), invalid_desc);
  
  for (int i = 0; i < height; ++i) {
    int idx = (i + rad) * (width + 2*rad) + rad;
    for (int j = 0; j < width; ++j, ++idx) {
      SIFT_DESC sift_desc;
      float norm = 0; 
      for (int k = 0; k < SIFT_DESC::static_size; ++k) {
        ifs_sift2 >> sift_desc[k];
         norm += sift_desc[k] * sift_desc[k];
      }

     // ASSERT_LOG(!ifs_sift2.tellg() >= 0);
      ASSERT_LOG(fabs(norm - 1.f) < 0.02) << "SIFT descriptor needs to be normalized.\n";
      sift2[idx] = sift_desc;
    }
  }
  ifs_sift2.close();

  CvMat* min_values = cvCreateMat(height, width, CV_32F);
  CvMat* offset_mat = cvCreateMat(height, width, CV_32S);
  
  ComputeNNF(width, height, sift1,
             width, height, sift2, rad,
             10, offset_mat, min_values);
  
  std::cout << "Summed error for flow: " << cvSum(min_values).val[0] << "\n";
   
  /*
  
  IplImage* img_1_orig = cvLoadImage("/Users/grundman/Documents/MATLAB/img_1.jpg"); 
  ASSERT_LOG(img_1_orig) << "Image 1 wasn't loaded correctly";
  IplImage* img_2_orig = cvLoadImage("/Users/grundman/Documents/MATLAB/img_2.jpg"); 
  ASSERT_LOG(img_2_orig) << "Image 2 wasn't loaded correctly";
  
  IplImage* img_1 = cvCreateImage(cvSize(img_1_orig->width, img_1_orig->height),
                                     IPL_DEPTH_8U, 4);
  IplImage* img_2 = cvCreateImage(cvSize(img_2_orig->width, img_2_orig->height),
                                     IPL_DEPTH_8U, 4);

  cvCvtColor(img_1_orig, img_1, CV_BGR2BGRA);
  cvCvtColor(img_2_orig, img_2, CV_BGR2BGRA);

  const int patch_rad = 3;
 
  const int inner_width = img_1->width - 2 * patch_rad;
  const int inner_height = img_1->height - 2 * patch_rad;
  vector<const uchar*> offset_vec(inner_width * inner_height);

  CvMat* min_values = cvCreateMat(inner_height, inner_width, CV_32S);
  
  ImageFilter::ComputeNNF(img_2, img_1, patch_rad, 5, 0, &offset_vec, min_values);
  std::cout << "Summed error for reconstruction: " << cvSum(min_values).val[0] << "\n";
  
  IplImage* out_img = cvCreateImage(cvSize(img_2_orig->width, img_2_orig->height),
                                     IPL_DEPTH_8U, 3);
  ImageFilter::AveragePatches(img_1, offset_vec, patch_rad, min_values, out_img);
  cvSaveImage("reconstruct_test.png", out_img);
   */
}
