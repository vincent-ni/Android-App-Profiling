/*
 *  warping.h
 *  make-test
 *
 *  Created by Matthias Grundmann on 10/1/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <opencv2/core/core_c.h>
#include <vector>
#include "image_util.h"
#include "assert_log.h"

namespace ImageFilter {

  inline bool IsWithinBounds(int x, int y, int width, int height) {
    return (x >= 0 && x < width && y>=0 && y < height);
  }

  // Forward warps the 3 channel image input along flow_x, flow_y to output.
  // The pixel in input and output are assumed to be of type Type.
  // If undefined_mask is specifed, pixels that are not assigned any color
  // value during warping will be set to mask a value of 0.
  // flow_x and flow_y are 1 channel images of depth 32F
  // input and output are 3 channel images of type Type.
  // undefined_mask is a 1 channel image of depth 8U.

  template <class Type>
  void ForwardWarp(const IplImage* input, const IplImage* flow_x, const IplImage* flow_y,
                   IplImage* output, IplImage* undefined_mask = 0) {
    ASSURE_LOG(HasSameDimensions(input, flow_x) &&
               HasSameDimensions(input, flow_y) &&
               HasSameDimensions(input, output))
    << "ForwardWarp: All inputs must be of same dimension.\n";

    const int width = input->width;
    const int height = input->height;

    // Allocate color and weight image.
    shared_ptr<IplImage> color_sum = cvCreateImageShared(width, height, IPL_DEPTH_32F, 3);
    shared_ptr<IplImage> weight_sum = cvCreateImageShared(width, height, IPL_DEPTH_32F, 1);
    cvZero(color_sum.get());
    cvSet(weight_sum.get(), cvScalar(1e-6));


    // Process each pixel.
    for (int i = 0; i < height; ++i) {
      const Type* src_ptr = RowPtr<const Type>(input, i);
      const float* flow_x_ptr = RowPtr<const float>(flow_x, i);
      const float* flow_y_ptr = RowPtr<const float>(flow_y, i);

      // float* color_sum_ptr = RowPtr<float>(color_sum, i);
      // float* weight_sum_ptr = RowPtr<float>(weight_sum, i);

      for (int j = 0;
           j < width;
           ++j, ++flow_x_ptr, ++flow_y_ptr, src_ptr += 3) {
        float dst_x = j + *flow_x_ptr;
        float dst_y = i + *flow_y_ptr;

        // Splat color at destination.
        const int left = floor(dst_x);
        const int top = floor(dst_y);

        const float dx = dst_x - (float)left;
        const float dy = dst_y - (float)top;

        const float _1dx = 1 - dx;
        const float _1dy = 1 - dy;

        const float _1dx_1dy = _1dx * _1dy;
        const float _1dxdy = _1dx * dy;
        const float _1dydx = _1dy * dx;
        const float dxdy = dx*dy;

        float* sum_ptr = RowPtr<float>(color_sum, top) + 3 * left;
        float* weight_ptr = RowPtr<float>(weight_sum, top) + left;

        // Top row.
        if (IsWithinBounds(left, top, width, height)) {
          weight_ptr[0] += _1dx_1dy;
          sum_ptr[0] += (float)src_ptr[0] * _1dx_1dy;
          sum_ptr[1] += (float)src_ptr[1] * _1dx_1dy;
          sum_ptr[2] += (float)src_ptr[2] * _1dx_1dy;
        }

        if (IsWithinBounds(left + 1, top, width, height)) {
          weight_ptr[1] += _1dydx;
          sum_ptr[3+0] += (float)src_ptr[3+0] * _1dydx;
          sum_ptr[3+1] += (float)src_ptr[3+1] * _1dydx;
          sum_ptr[3+2] += (float)src_ptr[3+2] * _1dydx;
        }

        // Bottom row.
        sum_ptr = PtrOffset(sum_ptr, color_sum->widthStep);
        weight_ptr = PtrOffset(weight_ptr, weight_sum->widthStep);

        if (IsWithinBounds(left, top + 1, width, height)) {
          weight_ptr[0] += _1dxdy;
          sum_ptr[0] += (float)src_ptr[0] * _1dxdy;
          sum_ptr[1] += (float)src_ptr[1] * _1dxdy;
          sum_ptr[2] += (float)src_ptr[2] * _1dxdy;
        }

        if (IsWithinBounds(left + 1, top + 1, width, height)) {
          weight_ptr[1] += dxdy;
          sum_ptr[3+0] += (float)src_ptr[3+0] * dxdy;
          sum_ptr[3+1] += (float)src_ptr[3+1] * dxdy;
          sum_ptr[3+2] += (float)src_ptr[3+2] * dxdy;
        }
      }
    }

    // Normalize by weight and output.

    if (!undefined_mask) {
      for (int i = 0; i < height; ++i) {
        const float* color_sum_ptr = RowPtr<const float>(color_sum, i);
        const float* weight_sum_ptr = RowPtr<const float>(weight_sum, i);
        Type* dst_ptr = RowPtr<Type>(output, i);

        for (int j = 0;
             j < width;
             ++j, ++weight_sum_ptr, color_sum_ptr += 3, dst_ptr += 3) {
          float weight = 1.0f / *weight_sum_ptr;
          dst_ptr[0] = (Type)(color_sum_ptr[0] * weight);
          dst_ptr[1] = (Type)(color_sum_ptr[1] * weight);
          dst_ptr[2] = (Type)(color_sum_ptr[2] * weight);
        }
      }
    } else {
      for (int i = 0; i < height; ++i) {
        const float* color_sum_ptr = RowPtr<const float>(color_sum, i);
        const float* weight_sum_ptr = RowPtr<const float>(weight_sum, i);
        Type* dst_ptr = RowPtr<Type>(output, i);
        uchar* mask_ptr = RowPtr<uchar>(undefined_mask, i);

        for (int j = 0;
             j < width;
             ++j, ++weight_sum_ptr, color_sum_ptr += 3, dst_ptr += 3, ++mask_ptr) {
          float weight = 1.0f / *weight_sum_ptr;
          dst_ptr[0] = (Type)(color_sum_ptr[0] * weight);
          dst_ptr[1] = (Type)(color_sum_ptr[1] * weight);
          dst_ptr[2] = (Type)(color_sum_ptr[2] * weight);

          if (*weight_sum_ptr < 1.1e-6)
            *mask_ptr = 0;
          else
            *mask_ptr = 1;
        }
      }
    }
  }


  // Backward warps the 4 channel image input to output.
  // output(x, y) = input(A * (x,y)) using bilinear interpolation, where A is 2x3 matrix and assumed
  // to be the non-affine part of a general 3x3 transform.
  // It is assumed that A * (x,y) is within the bounds of input. No boundary checking is performed.
  // TODO: Implement if necessary.
  // The pixel in input and output are assumed to be of type Type.
  template <class Type>
  void BackwardWarp(const IplImage* input, float A[6], IplImage* output) {
    const int width = output->width;
    const int height = output->height;

    for (int i = 0; i < height; ++i) {
      Type* dst_ptr = RowPtr<Type>(output, i);
      for (int j = 0; j < width; ++j, dst_ptr += 4) {

        float src_x = A[0] * j + A[1] * i + A[2];
        float src_y = A[3] * j + A[4] * i + A[5];

        ASSERT_LOG(src_x >= 0 && src_x < input->width - 1);
        ASSERT_LOG(src_y >= 0 && src_y < input->height - 1);

        const int left = floor(src_x);
        const int top = floor(src_y);

        const float dx = src_x - (float)left;
        const float dy = src_y - (float)top;

        int n_dx = (dx != 0) * 4;
        int n_dy = dy != 0;

        const float _1dx = 1 - dx;
        const float _1dy = 1 - dy;

        const float _1dx_1dy = _1dx * _1dy;
        const float _1dxdy = _1dx * dy;
        const float _1dydx = _1dy * dx;
        const float dxdy = dx*dy;

        const Type* src_ptr = RowPtr<const Type>(input, top) + 4 * left;
        float color[4] = { .0f, .0f, .0f, .0f };
        float weight = 0;

        // Top row.
        color[0] += (float)src_ptr[0] * _1dx_1dy;
        color[1] += (float)src_ptr[1] * _1dx_1dy;
        color[2] += (float)src_ptr[2] * _1dx_1dy;
        color[3] += (float)src_ptr[3] * _1dx_1dy;

        color[0] += (float)src_ptr[n_dx + 0] * _1dydx;
        color[1] += (float)src_ptr[n_dx + 1] * _1dydx;
        color[2] += (float)src_ptr[n_dx + 2] * _1dydx;
        color[3] += (float)src_ptr[n_dx + 3] * _1dydx;

        // Bottom row.
        src_ptr = PtrOffset(src_ptr, input->widthStep * n_dy);

        color[0] += (float)src_ptr[0] * _1dxdy;
        color[1] += (float)src_ptr[1] * _1dxdy;
        color[2] += (float)src_ptr[2] * _1dxdy;
        color[3] += (float)src_ptr[3] * _1dxdy;

        color[0] += (float)src_ptr[n_dx + 0] * dxdy;
        color[1] += (float)src_ptr[n_dx + 1] * dxdy;
        color[2] += (float)src_ptr[n_dx + 2] * dxdy;
        color[3] += (float)src_ptr[n_dx + 3] * dxdy;

        // Assign
        dst_ptr[0] = (Type)(color[0]);
        dst_ptr[1] = (Type)(color[1]);
        dst_ptr[2] = (Type)(color[2]);
        dst_ptr[3] = (Type)(color[3]);
      }
    }
  }

  // Backward warps the 3 channel image input along flow_x, flow_y to output.
  // The pixel in input and output are assumed to be of type Type.
  template <class Type>
  void BackwardWarp(const IplImage* input, const IplImage* flow_x, const IplImage* flow_y,
                    IplImage* output) {
    ASSURE_LOG(HasSameDimensions(input, flow_x) &&
               HasSameDimensions(input, flow_y) &&
               HasSameDimensions(input, output))
    << "BackwardWarp: All inputs must be of same dimension.\n";

    const int width = input->width;
    const int height = input->height;

    // Process each pixel.
    for (int i = 0; i < height; ++i) {
      Type* dst_ptr = RowPtr<Type>(output, i);
      const float* flow_x_ptr = RowPtr<const float>(flow_x, i);
      const float* flow_y_ptr = RowPtr<const float>(flow_y, i);

      for (int j = 0;
           j < width;
           ++j, ++flow_x_ptr, ++flow_y_ptr, dst_ptr += 3) {

        float src_x = j + *flow_x_ptr;
        float src_y = i + *flow_y_ptr;

        // Interpolate color from source.
        const int left = floor(src_x);
        const int top = floor(src_y);

        const float dx = src_x - (float)left;
        const float dy = src_y - (float)top;

        const float _1dx = 1 - dx;
        const float _1dy = 1 - dy;

        const float _1dx_1dy = _1dx * _1dy;
        const float _1dxdy = _1dx * dy;
        const float _1dydx = _1dy * dx;
        const float dxdy = dx*dy;

        const Type* src_ptr = RowPtr<const Type>(input, top) + 3 * left;
        float color[3] = { .0f, .0f, .0f };
        float weight = 0;

        // Top row.
        if (IsWithinBounds(left, top, width, height)) {
          weight += _1dx_1dy;
          color[0] += (float)src_ptr[0] * _1dx_1dy;
          color[1] += (float)src_ptr[1] * _1dx_1dy;
          color[2] += (float)src_ptr[2] * _1dx_1dy;
        }

        if (IsWithinBounds(left + 1, top, width, height)) {
          weight += _1dydx;
          color[0] += (float)src_ptr[3+0] * _1dydx;
          color[1] += (float)src_ptr[3+1] * _1dydx;
          color[2] += (float)src_ptr[3+2] * _1dydx;
        }

        // Bottom row.
        src_ptr = PtrOffset(src_ptr, input->widthStep);

        if (IsWithinBounds(left, top + 1, width, height)) {
          weight += _1dxdy;
          color[0] += (float)src_ptr[0] * _1dxdy;
          color[1] += (float)src_ptr[1] * _1dxdy;
          color[2] += (float)src_ptr[2] * _1dxdy;
        }

        if (IsWithinBounds(left + 1, top + 1, width, height)) {
          weight += dxdy;
          color[0] += (float)src_ptr[3+0] * dxdy;
          color[1] += (float)src_ptr[3+1] * dxdy;
          color[2] += (float)src_ptr[3+2] * dxdy;
        }

        // Assign.
        if (weight != 0) {
          weight = 1.0 / weight;
          dst_ptr[0] = (Type)(color[0] * weight);
          dst_ptr[1] = (Type)(color[1] * weight);
          dst_ptr[2] = (Type)(color[2] * weight);
        } else {
          dst_ptr[0] = dst_ptr[1] = dst_ptr[2] = 0.f;
        }
      }
    }
  }

}
