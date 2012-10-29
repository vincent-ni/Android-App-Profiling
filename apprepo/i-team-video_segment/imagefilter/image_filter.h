/*
 *  image_filters.h
 *  ImageFilterLib
 *
 *  Created by Matthias Grundmann on 9/29/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef IMAGE_FILTER_LIB_H__
#define IMAGE_FILTER_LIB_H__

#include <opencv2/core/core_c.h>
#include "image_util.h"

namespace ImageFilter {

// Computes Edge Tangent Flow from 32f image.
// Last parameter is optional and will be assigned the normalized gradient magnitude.
void EdgeTangentFlow(const IplImage* image, int radius, int iterations,
                     CvMat* flow_x, CvMat* flow_y, CvMat* gra_mag_out = 0);

void BilateralFilter(const IplImage* image, float sigma_space, float sigma_color,
                     IplImage* output);

// Bilateral filter along tangent on normal of Edge Tangent Flow.
// Pass edge tangent flow obtained from EdgeTangentFlow to flow_x, flow_y.
// Optional parameter step_size determines how far we traverse along the tangent
// or normal flow. step_size is expected to be between 0 and 1.
void BilateralFlowFilter(const IplImage* image,
                         const CvMat* flow_x,
                         const CvMat* flow_y,
                         float sigma_space_tangent,
                         float sigma_space_normal,
                         float sigma_color_tangent,
                         float sigma_color_normal,
                         int iterations,
                         IplImage* output,
                         const CvMat* step_size = 0);

// Takes the normalized gradient and converts in to step_size.
// All gradient values above threshold will be assigned to 1.
// The gradient values below threshold will be assigned to according to a sigmoid function
// with the following parameters:
// steepness ~ 100 - 300
// threshold ~ 0.1, determines when value one should be reached

// The function that will be applied is:
//    1 / (1 + exp(-steepness(x - delta)))

// where delta is set to:
//    delta = threshold - log(0.001)/-steepness
//                              ^^
//                        correction term to assure that value at threshold is ~ 1.
//                        otherwise function would be 0.5 at threshold.

// Function is able to work in-place, i.e. gra_mag and step_size can point to the same CvMat.
void GradientToStepSize(const CvMat* gra_mag,
                        float threshold,
                        float steepness,
                        CvMat* step_size);
class BlendOpOver {
public:
  void operator()(const uchar* lhs, const uchar* rhs, uchar* output) const {
    const float scale = 1.0 / 255.0;
    const float lhs_alpha = (float)lhs[3] * scale;
    const float lhs_alpha_1 = 1.0 - lhs_alpha;
    const float rhs_alpha = (float)rhs[3] * scale;
    const float alpha_mixed = lhs_alpha_1 * rhs_alpha;
    output[0] = (uchar)(lhs_alpha * lhs[0] + alpha_mixed * rhs[0]);
    output[1] = (uchar)(lhs_alpha * lhs[1] + alpha_mixed * rhs[1]);
    output[2] = (uchar)(lhs_alpha * lhs[2] + alpha_mixed * rhs[2]);
    output[3] = (uchar)((lhs_alpha + alpha_mixed) * 255);
  }
};

template<class BlendOp>
  void AlphaBlend(const IplImage* lhs,
                  const IplImage* rhs,
                  const BlendOp& blend_op,
                  IplImage* output) {
    for (int i = 0; i < lhs->height; ++i) {
      const uchar* lhs_ptr = RowPtr<uchar>(lhs, i);
      const uchar* rhs_ptr = RowPtr<uchar>(rhs, i);
      uchar* out_ptr = RowPtr<uchar>(output, i);
      for (int j = 0; j < lhs->width; ++j, lhs_ptr += 4, rhs_ptr += 4, out_ptr += 4) {
        blend_op(lhs_ptr, rhs_ptr, out_ptr);
      }
    }
  }
}


#endif
