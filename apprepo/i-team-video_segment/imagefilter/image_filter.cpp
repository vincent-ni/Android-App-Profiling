/*
 *  image_filters.cpp
 *  ImageFilterLib
 *
 *  Created by Matthias Grundmann on 9/29/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "image_filter.h"
#include "image_util.h"
#include "assert_log.h"

#include <opencv2/imgproc/imgproc_c.h>
#include <vector>

namespace ImageFilter {

  // Interpolates matrices A, B and C3 at (x,y). Assumes that C3 is a matrix having 3 interleaved
  // channels. No error checks will be performed!
  inline void Interpolate3RGB(const CvMat* A, const CvMat* B, const CvMat* C3, float x, float y,
                              float* out_a, float* out_b,
                              float* out_c1, float* out_c2, float* out_c3) {
    const int left = floor(x);
    const int top = floor(y);

    const float dx = x - (float)left;
    const float dy = y - (float)top;

    const int inc_x = dx != 0;
    const int inc_x3 = 3*inc_x;
    const int inc_y = dy != 0;

    const float _1dx = 1 - dx;
    const float _1dy = 1 - dy;

    const float _1dx_1dy = _1dx * _1dy;
    const float _1dxdy = _1dx * dy;
    const float _1dydx = _1dy * dx;
    const float dxdy = dx*dy;

    const float* top_left = RowPtr<float>(A, top) + left;
    const float* bottom_left = PtrOffset<float>(top_left, inc_y*A->step);
    *out_a = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x) +
             _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x);


    top_left = RowPtr<float>(B, top) + left;
    bottom_left = PtrOffset<float>(top_left, inc_y*B->step);
    *out_b = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x) +
             _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x);

    top_left = RowPtr<float>(C3, top) + left*3;
    bottom_left = PtrOffset<float>(top_left, inc_y*C3->step);
    *out_c1 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
              _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

    ++top_left, ++bottom_left;
    *out_c2 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
              _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

    ++top_left, ++bottom_left;
    *out_c3 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
              _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);
  }

  inline void Interpolate4RGB(const CvMat* A, const CvMat* B, const CvMat* D, const CvMat* C3,
                              float x, float y, float* out_a, float* out_b, float* out_d,
                              float* out_c1, float* out_c2, float* out_c3) {
    const int left = floor(x);
    const int top = floor(y);

    const float dx = x - (float)left;
    const float dy = y - (float)top;

    const int inc_x = dx != 0;
    const int inc_x3 = 3*inc_x;
    const int inc_y = dy != 0;

    const float _1dx = 1 - dx;
    const float _1dy = 1 - dy;

    const float _1dx_1dy = _1dx * _1dy;
    const float _1dxdy = _1dx * dy;
    const float _1dydx = _1dy * dx;
    const float dxdy = dx*dy;

    const float* top_left = RowPtr<float>(A, top) + left;
    const float* bottom_left = PtrOffset<float>(top_left, inc_y*A->step);
    *out_a = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x) +
    _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x);


    top_left = RowPtr<float>(B, top) + left;
    bottom_left = PtrOffset<float>(top_left, inc_y*B->step);
    *out_b = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x) +
             _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x);

    top_left = RowPtr<float>(D, top) + left;
    bottom_left = PtrOffset<float>(top_left, inc_y*D->step);
    *out_d = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x) +
             _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x);


    top_left = RowPtr<float>(C3, top) + left*3;
    bottom_left = PtrOffset<float>(top_left, inc_y*C3->step);
    *out_c1 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
    _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

    ++top_left, ++bottom_left;
    *out_c2 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
              _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

    ++top_left, ++bottom_left;
    *out_c3 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
              _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);
  }

  // Implementation functions
  void BilateralFlowFilterTangentImpl(const CvMat* input,
                                      const CvMat* flow_x,
                                      const CvMat* flow_y,
                                      const std::vector<float>& gauss_vec,
                                      const std::vector<float>& expLUT,
                                      const float lut_scale,
                                      CvMat* output) {
    const int img_width = input->width;
    const int img_height = input->height;
    const int radius = gauss_vec.size() - 1;

    for (int j = 0; j < img_height; ++j) {
      const float* src_ptr = RowPtr<float>(input, j);
      const float* u_ptr = RowPtr<float>(flow_x, j);
      const float* v_ptr = RowPtr<float>(flow_y, j);
      float* dst_ptr = RowPtr<float>(output, j);

      for (int i = 0; i < img_width; ++i, src_ptr += 3, dst_ptr+=3, ++u_ptr, ++v_ptr) {
        float my_b = *src_ptr;
        float my_g = *(src_ptr+1);
        float my_r = *(src_ptr+2);

        float weight = gauss_vec[0] * expLUT[0];
        float val_b = my_b * weight;
        float val_g = my_g * weight;
        float val_r = my_r * weight;
        float weight_sum = weight;

        // Go one step in tangent direction.
        float cur_x = i + *u_ptr;
        float cur_y = j + *v_ptr;
        float cur_b, cur_g, cur_r, dir_x, dir_y;

        // Traverse along tangent - positive direction.
        for (int k=1; k <= radius; ++k) {
          Interpolate3RGB(flow_x, flow_y, input, cur_x, cur_y, &dir_x, &dir_y,
                          &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
          gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x += dir_x;
          cur_y += dir_y;
        }

        // Traverse along tangent - negative direction.
        cur_x = i - *u_ptr;
        cur_y = j - *v_ptr;
        for (int k=1; k <= radius ; ++k) {
          Interpolate3RGB(flow_x, flow_y, input, cur_x, cur_y, &dir_x, &dir_y,
                          &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
          gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x -= dir_x;
          cur_y -= dir_y;
        }

        weight_sum = 1.0 / weight_sum;
        *dst_ptr = val_b * weight_sum;
        *(dst_ptr+1) = val_g * weight_sum;
        *(dst_ptr+2) = val_r * weight_sum;
      }
    }
  }

  void BilateralFlowFilterTangentImplStep(const CvMat* input,
                                          const CvMat* flow_x,
                                          const CvMat* flow_y,
                                          const CvMat* step_size,
                                          const std::vector<float>& gauss_vec,
                                          const std::vector<float>& expLUT,
                                          const float lut_scale,
                                          CvMat* output) {
    const int img_width = input->width;
    const int img_height = input->height;
    const int radius = gauss_vec.size() - 1;

    for (int j = 0; j < img_height; ++j) {
      const float* src_ptr = RowPtr<float>(input, j);
      const float* u_ptr = RowPtr<float>(flow_x, j);
      const float* v_ptr = RowPtr<float>(flow_y, j);
      const float* step_ptr = RowPtr<float>(step_size, j);
      float* dst_ptr = RowPtr<float>(output, j);

      for (int i = 0; i < img_width; ++i, src_ptr += 3, dst_ptr+=3, ++u_ptr, ++v_ptr, ++step_ptr) {
        float my_b = *src_ptr;
        float my_g = *(src_ptr+1);
        float my_r = *(src_ptr+2);

        float weight = gauss_vec[0] * expLUT[0];
        float val_b = my_b * weight;
        float val_g = my_g * weight;
        float val_r = my_r * weight;
        float weight_sum = weight;

        // Go one step in tangent direction.
        float cur_x = i + *u_ptr * *step_ptr;
        float cur_y = j + *v_ptr * *step_ptr;
        float cur_b, cur_g, cur_r, dir_x, dir_y, step;

        // Traverse along tangent - positive direction.
        for (int k=1; k <= radius; ++k) {
          Interpolate4RGB(flow_x, flow_y, step_size, input, cur_x, cur_y, &dir_x, &dir_y,
                          &step, &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
                   gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x += dir_x * step;
          cur_y += dir_y * step;
        }

        // Traverse along tangent - negative direction.
        cur_x = i - *u_ptr * *step_ptr;
        cur_y = j - *v_ptr * *step_ptr;
        for (int k=1; k <= radius ; ++k) {
          Interpolate4RGB(flow_x, flow_y, step_size, input, cur_x, cur_y, &dir_x, &dir_y,
                          &step, &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
                   gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x -= dir_x * step;
          cur_y -= dir_y * step;
        }

        weight_sum = 1.0 / weight_sum;
        *dst_ptr = val_b * weight_sum;
        *(dst_ptr+1) = val_g * weight_sum;
        *(dst_ptr+2) = val_r * weight_sum;
      }
    }
  }

  void BilateralFlowFilterNormalImpl(const CvMat* input,
                                     const CvMat* flow_x,
                                     const CvMat* flow_y,
                                     const std::vector<float>& gauss_vec,
                                     const std::vector<float>& expLUT,
                                     const float lut_scale,
                                     CvMat* output) {
    // Same as tangent but with (y,-x) as direction
    const int img_width = input->width;
    const int img_height = input->height;
    const int radius = gauss_vec.size() - 1;

    for (int j = 0; j < img_height; ++j) {
      const float* src_ptr = RowPtr<float>(input, j);
      const float* u_ptr = RowPtr<float>(flow_x, j);
      const float* v_ptr = RowPtr<float>(flow_y, j);
      float* dst_ptr = RowPtr<float>(output, j);

      for (int i = 0; i < img_width; ++i, src_ptr += 3, dst_ptr+=3, ++u_ptr, ++v_ptr) {
        float my_b = *src_ptr;
        float my_g = *(src_ptr+1);
        float my_r = *(src_ptr+2);

        float weight = gauss_vec[0] * expLUT[0];
        float val_b = my_b * weight;
        float val_g = my_g * weight;
        float val_r = my_r * weight;
        float weight_sum = weight;

        float cur_x = i + *v_ptr;
        float cur_y = j - *u_ptr;
        float cur_b, cur_g, cur_r, dir_x, dir_y;

        // Traverse along tangent - positive direction.
        for (int k=1; k <= radius ; ++k) {
          Interpolate3RGB(flow_x, flow_y, input, cur_x, cur_y, &dir_x, &dir_y,
                          &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
          gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x += dir_y;
          cur_y -= dir_x;
        }

        // Traverse along tangent - negative direction.
        cur_x = i - *v_ptr;
        cur_y = j + *u_ptr;
        for (int k=1; k <= radius ; ++k) {
          Interpolate3RGB(flow_x, flow_y, input, cur_x, cur_y, &dir_x, &dir_y,
                          &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
          gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x -= dir_y;
          cur_y += dir_x;
        }

        weight_sum = 1.0 / weight_sum;
        *dst_ptr = val_b * weight_sum;
        *(dst_ptr+1) = val_g * weight_sum;
        *(dst_ptr+2) = val_r * weight_sum;
      }
    }
  }


  void BilateralFlowFilterNormalImplStep(const CvMat* input,
                                         const CvMat* flow_x,
                                         const CvMat* flow_y,
                                         const CvMat* step_size,
                                         const std::vector<float>& gauss_vec,
                                         const std::vector<float>& expLUT,
                                         const float lut_scale,
                                         CvMat* output) {
    // Same as tangent but with (y,-x) as direction
    const int img_width = input->width;
    const int img_height = input->height;
    const int radius = gauss_vec.size() - 1;

    for (int j = 0; j < img_height; ++j) {
      const float* src_ptr = RowPtr<float>(input, j);
      const float* u_ptr = RowPtr<float>(flow_x, j);
      const float* v_ptr = RowPtr<float>(flow_y, j);
      const float* step_ptr = RowPtr<float>(step_size, j);
      float* dst_ptr = RowPtr<float>(output, j);

      for (int i = 0; i < img_width; ++i, src_ptr += 3, dst_ptr+=3, ++u_ptr, ++v_ptr, ++step_ptr) {
        float my_b = *src_ptr;
        float my_g = *(src_ptr+1);
        float my_r = *(src_ptr+2);

        float weight = gauss_vec[0] * expLUT[0];
        float val_b = my_b * weight;
        float val_g = my_g * weight;
        float val_r = my_r * weight;
        float weight_sum = weight;

        float cur_x = i + *v_ptr * *step_ptr;
        float cur_y = j - *u_ptr * *step_ptr;
        float cur_b, cur_g, cur_r, dir_x, dir_y, step;

        // Traverse along tangent - positive direction.
        for (int k=1; k <= radius ; ++k) {
          Interpolate4RGB(flow_x, flow_y, step_size, input, cur_x, cur_y, &dir_x, &dir_y,
                          &step, &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
                   gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x += dir_y * step;
          cur_y -= dir_x * step;
        }

        // Traverse along tangent - negative direction.
        cur_x = i - *v_ptr * *step_ptr;
        cur_y = j + *u_ptr * *step_ptr;
        for (int k=1; k <= radius ; ++k) {
          Interpolate4RGB(flow_x, flow_y, step_size, input, cur_x, cur_y, &dir_x, &dir_y,
                          &step, &cur_b, &cur_g, &cur_r);

          const float diff_b = cur_b - my_b;
          const float diff_g = cur_g - my_g;
          const float diff_r = cur_r - my_r;

          weight = expLUT[(int)((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * lut_scale)] *
                   gauss_vec[k];
          weight_sum += weight;

          val_b += cur_b * weight;
          val_g += cur_g * weight;
          val_r += cur_r * weight;

          // Move along.
          cur_x -= dir_y * step;
          cur_y += dir_x * step;
        }

        weight_sum = 1.0 / weight_sum;
        *dst_ptr = val_b * weight_sum;
        *(dst_ptr+1) = val_g * weight_sum;
        *(dst_ptr+2) = val_r * weight_sum;
      }
    }
  }


  void BilateralFilter(const IplImage* image, float sigma_space, float sigma_color,
                       IplImage* output) {
    // Check validity of input.
    ASSURE_LOG(HasSameDimensions(image, output)) << "Input and output image dimensions differ.";
    ASSURE_LOG(image->depth == IPL_DEPTH_32F) << "Input image must be 32f.";
    ASSURE_LOG(image->nChannels == 3 || image->nChannels == 1)
        << "Input image must have one or three channels.";

    // Setup temp image.
    const int radius = sigma_space * 1.5;
    const int diam = 2*radius + 1;
    const int img_width = image->width;
    const int img_height = image->height;
    const int cn = image->nChannels;

    shared_ptr<IplImage> tmp_image_img = cvCreateImageShared(image->width + 2*radius,
                                                             image->height + 2*radius,
                                                             IPL_DEPTH_32F,
                                                             cn);

    cvCopyMakeBorder(image, tmp_image_img.get(), cvPoint(radius, radius), IPL_BORDER_REPLICATE);
    cvSetImageROI(tmp_image_img.get(), cvRect(radius, radius, img_width, img_height));
    CvMat tmp_image_mat;
    CvMat* tmp_image = cvGetMat(tmp_image_img.get(), &tmp_image_mat);
    cvResetImageROI(tmp_image_img.get());

    // Calculate space offsets and weights.
    std::vector<int> space_ofs(diam*diam);
    std::vector<float> space_weights(diam*diam);
    int space_sz = 0;
    const float space_coeff = -0.5 / (sigma_space * sigma_space);

    for (int i = -radius; i <= radius; ++i) {
      for (int j = -radius; j <= radius; ++j) {
        int r2 = i*i + j*j;
        if (r2 > radius*radius)
          continue;

        space_ofs[space_sz] = i * tmp_image->step + j * sizeof(float) * cn;
        space_weights[space_sz++] = exp(space_coeff * (float)r2);
      }
    }

    // Compute color exp-weight lookup table.
    double min_val, max_val;
    CvMat image_mat;
    const CvMat* image_mat_ptr = cvReshape(image, &image_mat, 1); // 1 channel rep.
    cvMinMaxLoc (image_mat_ptr, &min_val, &max_val);

    const float diff_range = std::max(1e-3, (max_val - min_val)*(max_val - min_val) * cn * 1.02);

    const int num_bins = (1 << 12) * cn;
    const float scale = (float) num_bins / diff_range;

    std::vector<float> expLUT(num_bins);
    const float color_coeff = -0.5 / (sigma_color * sigma_color);
    bool zero_reached = false;
    for (int i = 0; i < num_bins; ++i) {
      if (!zero_reached) {
        expLUT[i] = exp( (float)i / scale * color_coeff);
        zero_reached = (expLUT[i] < 1e-10);
      } else {
        expLUT[i] = 0;
      }
    }

    // Bilateral filtering.
    if (image->nChannels == 1) { // Gray scale case.
      for (int i = 0; i < img_height; ++i) {
        const float* src_ptr = RowPtr<float>(tmp_image, i);
        float* dst_ptr = RowPtr<float>(output, i);
        const float* local_ptr;

        for (int j = 0; j < img_width; ++j, ++src_ptr, ++dst_ptr) {

          float my_val = *src_ptr;
          float weight, weight_sum = 0, val_sum = 0, val_diff;

          for (int k = 0; k < space_sz; ++k) {
            local_ptr = PtrOffset(src_ptr, space_ofs[k]);
            val_diff = my_val - *local_ptr;
            val_diff *= val_diff;

            weight = space_weights[k] * expLUT[int(val_diff * scale)];
            weight_sum += weight;

            val_sum += *local_ptr * weight;
          }

          *dst_ptr = val_sum / weight_sum;
        }

      }
    } else {    // Color case.
      for (int i = 0; i < img_height; ++i) {
        const float* src_ptr = RowPtr<float>(tmp_image, i);
        float* dst_ptr = RowPtr<float>(output, i);
        const float* local_ptr;

        for (int j = 0; j < img_width; ++j, src_ptr+=cn, dst_ptr+=cn) {
          float my_b = *src_ptr;
          float my_g = *(src_ptr+1);
          float my_r = *(src_ptr+2);
          float weight, weight_sum = 0;
          float diff_r, diff_g, diff_b, val_r, val_g, val_b;
          float sum_r = 0, sum_g = 0, sum_b = 0;

          for (int k = 0; k < space_sz; ++k) {
            local_ptr = PtrOffset(src_ptr, space_ofs[k]);
            val_b = *local_ptr;
            val_g = *(local_ptr+1);
            val_r = *(local_ptr+2);

            diff_b = my_b - val_b;
            diff_g = my_g - val_g;
            diff_r = my_r - val_r;
            int idx = (int) ((diff_b*diff_b + diff_g*diff_g + diff_r*diff_r) * scale);
            ASSERT_LOG(idx >= 0 && idx < num_bins) << "idx is out LUT bounds";
            weight = space_weights[k] * expLUT[idx];
            weight_sum += weight;

            sum_b += val_b * weight;
            sum_g += val_g * weight;
            sum_r += val_r * weight;
          }

          weight_sum = 1.0 / weight_sum;
          *dst_ptr = sum_b * weight_sum;
          *(dst_ptr + 1) = sum_g * weight_sum;
          *(dst_ptr + 2) = sum_r * weight_sum;
        }
      }
    }
  }

  void EdgeTangentFlow(const IplImage* image, int radius, int iterations, CvMat* flow_x_out,
                       CvMat* flow_y_out, CvMat* gra_mag_out) {
    // Check validity of input.
    ASSURE_LOG(HasSameDimensions(image, flow_x_out))
      << "Input image and flow_x matrix dimensions differ.";
    ASSURE_LOG(HasSameDimensions(image, flow_y_out))
      << "Input image and flow_y matrix dimensions differ.";
    ASSURE_LOG(image->depth == IPL_DEPTH_32F) << "Input image must be 32f.";

    const int diam = 2*radius+1;
    const int img_width = image->width;
    const int img_height = image->height;

    // Allocate temporary data-structures.
    shared_ptr<IplImage> flow_x_img = cvCreateImageShared(img_width + 2*radius,
                                                          img_height + 2*radius,
                                                          IPL_DEPTH_32F, 1);
    shared_ptr<IplImage> flow_y_img = cvCreateImageShared(img_width + 2*radius,
                                                          img_height + 2*radius,
                                                          IPL_DEPTH_32F, 1);

    shared_ptr<IplImage> gra_mag_img = cvCreateImageShared(img_width + 2*radius,
                                                           img_height + 2*radius,
                                                           IPL_DEPTH_32F, 1);

    cvSetImageROI(flow_x_img.get(), cvRect(radius, radius, img_width, img_height));
    cvSetImageROI(flow_y_img.get(), cvRect(radius, radius, img_width, img_height));
    cvSetImageROI(gra_mag_img.get(), cvRect(radius, radius, img_width, img_height));

    CvMat flow_x_mat, flow_y_mat, gra_mag_mat;
    CvMat* flow_x = cvGetMat(flow_x_img.get(), &flow_x_mat);
    CvMat* flow_y = cvGetMat(flow_y_img.get(), &flow_y_mat);
    CvMat* gra_mag = cvGetMat(gra_mag_img.get(), &gra_mag_mat);

    cvResetImageROI(flow_x_img.get());
    cvResetImageROI(flow_y_img.get());
    cvResetImageROI(gra_mag_img.get());

    // Color convert if necessary.
    shared_ptr<IplImage> gray_img;
    if (image->nChannels > 1) {
      gray_img = cvCreateImageShared(img_width, img_height, IPL_DEPTH_32F, 1);
      cvCvtColor (image, gray_img.get(), CV_BGR2GRAY);
      image = gray_img.get();
    }

    // Compute gradient (swap x and y)
    cvSobel(image, flow_y, 1, 0);
    cvSobel(image, flow_x, 0, 1);

    // Compute magnitude, normalize and transform (y,x) -> (y,-x) to yield tangents.
    float max_gra_mag = -1e38;
    for (int i = 0; i < img_height; ++i) {
      float* u_ptr = RowPtr<float>(flow_x, i);
      float* v_ptr = RowPtr<float>(flow_y, i);
      float* gra_ptr = RowPtr<float>(gra_mag, i);
      for (int j = 0; j < img_width; ++j, ++u_ptr, ++v_ptr, ++gra_ptr) {
        float mag = *u_ptr * *u_ptr + *v_ptr * *v_ptr;
        if (mag > 1e-10) {
          mag = sqrt(mag);
          *gra_ptr = mag;
          if (mag > max_gra_mag)
            max_gra_mag = mag;

          mag = 1.0 / mag;
          *u_ptr = *u_ptr * mag;
          *v_ptr = *v_ptr * -mag;
        } else {
          *u_ptr = 0;
          *v_ptr = 0;
          *gra_ptr = 0;
        }
      }
    }

    if (max_gra_mag > 1e-10)
      cvScale (gra_mag, gra_mag, 1.0 / max_gra_mag);

    if (gra_mag_out) {
      ASSURE_LOG(HasSameDimensions(image, gra_mag_out))
          << "Input image and gradient magnitude matrix dimensions differ.";
      cvCopy(gra_mag, gra_mag_out);
    }

    // Inplace make replicate border
    cvCopyMakeBorder(flow_x, flow_x_img.get(), cvPoint(radius, radius), IPL_BORDER_REPLICATE);
    cvCopyMakeBorder(flow_y, flow_y_img.get(), cvPoint(radius, radius), IPL_BORDER_REPLICATE);
    cvCopyMakeBorder(gra_mag, gra_mag_img.get(), cvPoint(radius, radius), IPL_BORDER_REPLICATE);

    // Setup box filter.
    ASSURE_LOG (flow_x->step == flow_y->step) << "flow_x and flow_y have different step sizes.";
    ASSURE_LOG (flow_x->step == gra_mag->step)
        << "flow_x and gradient magnitude have different step sizes.";

    const int space_sz = diam*diam;
    std::vector<int> space_ofs(space_sz);
    for (int i = -radius, idx = 0; i <= radius; ++i)
      for (int j = -radius; j <= radius; ++j, ++idx)
        space_ofs[idx] = i * flow_x->step + j * sizeof(float);

    // Now filter iteration times to get results.
    const int elems = diam*diam;
    for (int iter = 0; iter < iterations; ++iter) {
      for (int i = 0; i < img_height; ++i) {
        const float* u_ptr_in = RowPtr<float>(flow_x, i);
        const float* v_ptr_in = RowPtr<float>(flow_y, i);
        const float* gra_ptr = RowPtr<float>(gra_mag, i);

        float* u_ptr_out = RowPtr<float>(flow_x_out, i);
        float* v_ptr_out = RowPtr<float>(flow_y_out, i);
        const int* space_ptr;

        for (int j = 0;
             j < img_width;
             ++j, ++u_ptr_in, ++v_ptr_in, ++gra_ptr, ++u_ptr_out, ++v_ptr_out) {

          space_ptr = &space_ofs[0];
          float u_sum = 0, v_sum = 0, weight_sum = 0, weight, dot_prod, other_u, other_v, norm;
          const float my_u = *u_ptr_in;
          const float my_v = *v_ptr_in;
          const float my_gra = *gra_ptr;

          if (my_gra > 1e-10) {
            int neg_dot = 0;    // Count how often other vector faces in opposite direction.
            for (int k = 0; k < space_sz; ++k, ++space_ptr) {
              // Gradient weight.
              weight = 0.5 * (1 + *PtrOffset(gra_ptr, *space_ptr) - my_gra);
              other_u = *PtrOffset(u_ptr_in, *space_ptr);
              other_v = *PtrOffset(v_ptr_in, *space_ptr);
              // Dot product.
              dot_prod = other_u * my_u + other_v * my_v;
              if (dot_prod < 0)
                ++neg_dot;

              weight *= fabs(dot_prod);

              u_sum += other_u * weight;
              v_sum += other_v * weight;
              weight_sum += weight;
            }

            // Turn 180 degree if necessary.
            if (neg_dot > elems/2) {
              u_sum = -u_sum;
              v_sum = -v_sum;
            }

          } else {
            // Same as above with dot_prod implicitly set to 1
            for (int k = 0; k < space_sz; ++k, ++space_ptr) {
              // Gradient weight.
              weight = 0.5 * (1 + *PtrOffset(gra_ptr, *space_ptr) - my_gra);

              other_u = *PtrOffset(u_ptr_in, *space_ptr);
              other_v = *PtrOffset(v_ptr_in, *space_ptr);

              u_sum += other_u * weight;
              v_sum += other_v * weight;
              weight_sum += weight;
            }
          }

          // Normalize.
          weight_sum = 1.0 / weight_sum;   // always > 0
          u_sum *= weight_sum;
          v_sum *= weight_sum;
          norm = u_sum * u_sum + v_sum * v_sum;
          if (norm > 1e-10) {
            norm = 1.0/sqrt(norm);
            *u_ptr_out = u_sum * norm;
            *v_ptr_out = v_sum * norm;
          } else {
            *u_ptr_out = 0;
            *v_ptr_out = 0;
          }
        }
      }

      // Copy with border.
      if (iter < iterations-1) {
        cvCopyMakeBorder(flow_x_out, flow_x_img.get(), cvPoint(radius, radius),
                         IPL_BORDER_REPLICATE);
        cvCopyMakeBorder(flow_y_out, flow_y_img.get(), cvPoint(radius, radius),
                         IPL_BORDER_REPLICATE);
      }
    }
  }

  void BilateralFlowFilter(const IplImage* image,
                           const CvMat* flow_x_in,
                           const CvMat* flow_y_in,
                           float sigma_space_tangent,
                           float sigma_space_normal,
                           float sigma_color_tangent,
                           float sigma_color_normal,
                           int iterations,
                           IplImage* output_img,
                           const CvMat* step_size) {
    // Check validity of input.
    ASSURE_LOG(HasSameDimensions(image, output_img)) << "Input and output image dimensions differ";
    ASSURE_LOG(image->depth == IPL_DEPTH_32F) << "Input image must be 32f";
    ASSURE_LOG(image->nChannels == 3) << "Input image must be 3 channel image";

    // Flow images with border.
    const int radius_tangent = 1.5 * sigma_space_tangent;
    const int radius_normal = 1.5 * sigma_space_normal;
    const int radius_max = std::max(radius_tangent, radius_normal);
    const int img_width = image->width;
    const int img_height = image->height;
    const int cn = image->nChannels;

    shared_ptr<IplImage> flow_x_img = cvCreateImageShared(img_width + 2*radius_max,
                                                          img_height + 2*radius_max,
                                                          IPL_DEPTH_32F,
                                                          1);
    shared_ptr<IplImage> flow_y_img = cvCreateImageShared(img_width + 2*radius_max,
                                                          img_height + 2*radius_max,
                                                          IPL_DEPTH_32F,
                                                          1);

    cvCopyMakeBorder(flow_x_in, flow_x_img.get(), cvPoint(radius_max, radius_max),
                     IPL_BORDER_REPLICATE);
    cvCopyMakeBorder(flow_y_in, flow_y_img.get(), cvPoint(radius_max, radius_max),
                     IPL_BORDER_REPLICATE);
    cvSetImageROI(flow_x_img.get(), cvRect(radius_max, radius_max, img_width, img_height));
    cvSetImageROI(flow_y_img.get(), cvRect(radius_max, radius_max, img_width, img_height));

    CvMat flow_x_mat, flow_y_mat;
    CvMat* flow_x = cvGetMat(flow_x_img.get(), &flow_x_mat);
    CvMat* flow_y = cvGetMat(flow_y_img.get(), &flow_y_mat);

    cvResetImageROI(flow_x_img.get());
    cvResetImageROI(flow_y_img.get());

    // Temporary images.
    shared_ptr<IplImage> input_img = cvCreateImageShared(img_width + 2*radius_max,
                                                         img_height + 2*radius_max,
                                                         IPL_DEPTH_32F,
                                                         3);
    cvSetImageROI(input_img.get(), cvRect(radius_max, radius_max, img_width, img_height));
    CvMat input_mat;
    CvMat* input = cvGetMat(input_img.get(), &input_mat);
    cvResetImageROI(input_img.get());

    cvCopyMakeBorder(image, input_img.get(), cvPoint(radius_max, radius_max),
                     IPL_BORDER_REPLICATE);

    // Spatial weights.
    std::vector<float> gauss_vec_tangent(radius_tangent+1);
    float tangent_coeff = -0.5 / (sigma_space_tangent * sigma_space_tangent);
    for (int i = 0; i <= radius_tangent; ++i) {
      gauss_vec_tangent[i] = exp(float(i) * tangent_coeff);
    }

    std::vector<float> gauss_vec_normal(radius_normal+1);
    float normal_coeff = -0.5 / (sigma_space_normal * sigma_space_normal);
    for (int i = 0; i <= radius_normal; ++i) {
      gauss_vec_normal[i] = exp(float(i) * normal_coeff);
    }

    // Compute expLUTs.
    double min_val, max_val;
    CvMat image_mat;
    const CvMat* image_mat_ptr = cvReshape(image, &image_mat, 1);   // 1 channel rep.
    cvMinMaxLoc (image_mat_ptr, &min_val, &max_val);

    const float diff_range = std::max(1e-3, (max_val - min_val)*(max_val - min_val) * cn * 1.02);
    const int num_bins = (1 << 12) * cn;
    const float scale = (float) num_bins / diff_range;

    std::vector<float> expLUT_tangent(num_bins);
    std::vector<float> expLUT_normal(num_bins);
    const float color_tangent_coeff = -0.5 / (sigma_color_tangent * sigma_color_tangent);
    const float color_normal_coeff = -0.5 / (sigma_color_normal * sigma_color_normal);

    bool zero_reached_tangent = false;
    bool zero_reached_normal = false;

    for (int i = 0; i < num_bins; ++i) {
      if (!zero_reached_tangent) {
        expLUT_tangent[i] = exp( (float)i / scale * color_tangent_coeff);
        zero_reached_tangent = (expLUT_tangent[i] < 1e-10);
      } else {
        expLUT_tangent[i] = 0;
      }

      if(!zero_reached_normal) {
        expLUT_normal[i] = exp ( (float)i / scale * color_normal_coeff);
        zero_reached_normal = (expLUT_normal[i] < 1e-10);
      } else {
        expLUT_normal[i] = 0;
      }
    }

    // Output mat view.
    CvMat output_mat;
    CvMat* output = cvGetMat(output_img, &output_mat);

    // Alternate filtering.
    for (int iter = 0; iter < iterations; ++iter) {
      if (step_size) {
        BilateralFlowFilterTangentImplStep(input, flow_x, flow_y, step_size, gauss_vec_tangent,
                                           expLUT_tangent, scale, output);
      } else {
        BilateralFlowFilterTangentImpl(input, flow_x, flow_y, gauss_vec_tangent, expLUT_tangent,
                                       scale, output);
      }

      cvCopyMakeBorder(output, input_img.get(), cvPoint(radius_max, radius_max),
                       IPL_BORDER_REPLICATE);

      if (step_size) {
        BilateralFlowFilterNormalImplStep(input, flow_x, flow_y, step_size, gauss_vec_normal,
                                          expLUT_normal, scale, output);
      } else {
        BilateralFlowFilterNormalImpl(input, flow_x, flow_y, gauss_vec_normal, expLUT_normal,
                                      scale, output);
      }

      if (iter != iterations-1)
        cvCopyMakeBorder(output, input_img.get(), cvPoint(radius_max, radius_max),
                         IPL_BORDER_REPLICATE);
    }

  }

  void GradientToStepSize(const CvMat* gra_mag,
                          float threshold,
                          float steepness,
                          CvMat* step_size) {
    ASSURE_LOG(HasSameDimensions(gra_mag, step_size))
        << "Input and output matrix dimensions differ";
    const int img_width = gra_mag->width;
    const int img_height = gra_mag->height;

    const float delta = threshold - log(0.001) / -steepness;

    for (int j = 0; j < img_height; ++j) {
      const float* gra_ptr = RowPtr<float>(gra_mag, j);
      float* step_ptr = RowPtr<float>(step_size, j);

      for (int i = 0; i < img_width; ++i, ++gra_ptr, ++step_ptr) {
        if (*gra_ptr > threshold) {
          *step_ptr = 1.0;
        } else {
          *step_ptr = 1 / (1 + exp(-steepness * (*gra_ptr - delta)));
        }
      }
    }
  }

}
