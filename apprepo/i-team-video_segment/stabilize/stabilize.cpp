/*
 *  color_checker.cpp
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "stabilize.h"

#include "assert_log.h"
#include "highgui.h"
#include "image_util.h"
#include "mkl_lapack.h"
#include <math.h>
#include "optical_flow_unit.h"

using namespace ImageFilter;

#define MOTION_X 0.5
#define MOTION_Y 0
#define FEATURE_GRID_SPACING 1000

namespace VideoFramework {

  StabilizeUnit::StabilizeUnit(const std::string& video_stream_name,
                               const std::string& flow_stream_name)
      :  video_stream_name_(video_stream_name),
         flow_stream_name_(flow_stream_name) {
  }
    
  float ColorFeature::Distance(const ColorFeature& rhs) {
    return sq_norm(mean - rhs.mean) + sq_norm(var - rhs.var);
  }
  
  bool StabilizeUnit::OpenStreams(StreamSet* set) {
    video_stream_idx_ = FindStreamIdx(video_stream_name_, set);
    
    if (video_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find Video stream!\n";
      return false;
    }
    
    const VideoStream* vid_stream = 
         dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());
    
    ASSERT_LOG(vid_stream);
    
    frame_width_ = vid_stream->get_frame_width();
    frame_height_ = vid_stream->get_frame_height();        
    
    feat_grid_dim_x_ = ceil(frame_width_ / FEATURE_GRID_SPACING) + 1;
    feat_grid_dim_y_ = ceil(frame_height_ / FEATURE_GRID_SPACING) + 1;
    feat_grid_sz_ = feat_grid_dim_x_ * feat_grid_dim_y_;
    
    // Get optical flow stream.
    flow_stream_idx_ = FindStreamIdx(flow_stream_name_, set);
    
    if (flow_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find optical flow stream.\n";
      return false;
    }
    
    homog_composed_ = cvCreateMatShared(3, 3, CV_32F);
    cvSetIdentity(homog_composed_.get());
    
    input_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 3);
    map_x_ = cvCreateMatShared(frame_height_, frame_width_, CV_32F);
    map_y_ = cvCreateMatShared(frame_height_, frame_width_, CV_32F);
    
    output_flushed_ = false;
    
    
    // Test push pull.
    shared_ptr<CvMat> disp = cvCreateMatShared(512 + 4,
                                               3 * (512 + 4),
                                               CV_32F);
    const int border = 1;
    IplImage* lena = cvLoadImage("/Users/grundman/Pictures/lena512gray.png");
    
    vector<Feature> features;
    // Sample some random values uniformly.
    for (int i = 0; i < 20000; ++i) {
      CvPoint2D32f loc = cvPoint2D32f(100 + rand() % 312, 100 + rand() % 312);
      int intensity = *(RowPtr<uchar>(lena, (int)loc.y) + 3 * (int)loc.x);
      features.push_back(Feature(loc, cvPoint2D32f(intensity, intensity)));
    }
    
    PushPullErrors(features, cvPoint(0, 0), 
                   cvSize(512, 512),
                   BINOMIAL_5x5,
                   disp.get());
    
    shared_ptr<IplImage> magn = 
    cvCreateImageShared(512, 512, IPL_DEPTH_8U, 3);
    
    for (int i = 0; i < 512; ++i) {
      const float* disp_ptr = RowPtr<float>(disp.get(), i + border) + 3 * border;
      uchar* magn_ptr = RowPtr<uchar>(magn.get(), i);
      
      for (int j = 0;
           j < 512; 
           ++j, disp_ptr += 3, magn_ptr += 3) {
        magn_ptr[0] = disp_ptr[0];
        magn_ptr[1] = disp_ptr[0];
        magn_ptr[2] = disp_ptr[0];
      }
    }
    
    cvNamedWindow("test_out");
    cvShowImage("test_out", magn.get());
    cvWaitKey(0);  
    
    return true;
  }

void StabilizeUnit::EstimateLinearMotion(const vector<Feature>& features,
                                         bool similarity,
                                         CvMat* model) {
  const int num_params = similarity ? 4 : 6;
  const float frame_diam = sqrt(frame_width_ * frame_width_ + 
                                frame_height_ * frame_height_);
  const float s = 1.0 / (frame_diam);
  
  float pre_transform_data[9] = { s, 0, -0.5,
                                  0, s, -0.5,
                                  0, 0,   1};
  
  CvMat pre_transform;
  cvInitMatHeader(&pre_transform, 3, 3, CV_32F, pre_transform_data);
  
  shared_ptr<CvMat> A_mat = cvCreateMatShared(num_params, num_params, CV_32F);
  cvZero(A_mat.get());

  shared_ptr<CvMat> b_mat = cvCreateMatShared(num_params, 1, CV_32F);
  cvZero(b_mat.get());
  
  shared_ptr<CvMat> J_mat = cvCreateMatShared(2, num_params, CV_32F);
  
  for (vector<Feature>::const_iterator feat = features.begin();
       feat != features.end();
       ++feat) {
    CvPoint2D32f loc = transform(feat->loc, &pre_transform);
    CvPoint2D32f prev_loc = transform(feat->loc  + feat->vec, &pre_transform);
    
    // Compute Jacobian at loc.
    cvZero(J_mat.get());
    if (similarity) {
      cvSet2D(J_mat.get(), 0, 0, cvScalar(1.0));
      cvSet2D(J_mat.get(), 1, 1, cvScalar(1.0));
      
      cvSet2D(J_mat.get(), 0, 2, cvScalar(loc.x));
      cvSet2D(J_mat.get(), 0, 3, cvScalar(-loc.y));
      cvSet2D(J_mat.get(), 1, 2, cvScalar(loc.y));
      cvSet2D(J_mat.get(), 1, 3, cvScalar(loc.x));

    } else {
      // Affine  model.
      cvSet2D(J_mat.get(), 0, 0, cvScalar(1.0));
      cvSet2D(J_mat.get(), 1, 1, cvScalar(1.0));
      
      cvSet2D(J_mat.get(), 0, 2, cvScalar(loc.x));
      cvSet2D(J_mat.get(), 0, 3, cvScalar(loc.y));
      
      cvSet2D(J_mat.get(), 1, 4, cvScalar(loc.x));
      cvSet2D(J_mat.get(), 1, 5, cvScalar(loc.y));      
    }
    
    cvGEMM(J_mat.get(), J_mat.get(), 1.0, A_mat.get(), 1.0, A_mat.get(),
           CV_GEMM_A_T);
    
    // Set rhs.
    CvMat rhs;
    cvInitMatHeader(&rhs, 2, 1, CV_32F, &prev_loc, sizeof(float));
    cvGEMM(J_mat.get(), &rhs, 1.0, b_mat.get(), 1.0, b_mat.get(), CV_GEMM_A_T);
  }
  
  shared_ptr<CvMat> solution(cvCreateMatShared(num_params, 1, CV_32F));
  cvSolve(A_mat.get(), b_mat.get(), solution.get());
  
  shared_ptr<CvMat> pre_solution(cvCreateMatShared(3, 3, CV_32F));
  cvSetIdentity(pre_solution.get());
  if (similarity) {
    const float a = cvGet2D(solution.get(), 2, 0).val[0];
    const float b = cvGet2D(solution.get(), 3, 0).val[0];
    const float dx = cvGet2D(solution.get(), 0, 0).val[0];
    const float dy = cvGet2D(solution.get(), 1, 0).val[0];
    
    cvSet2D(pre_solution.get(), 0, 0, cvScalar(a));
    cvSet2D(pre_solution.get(), 0, 1, cvScalar(-b));
    cvSet2D(pre_solution.get(), 1, 0, cvScalar(b));
    cvSet2D(pre_solution.get(), 1, 1, cvScalar(a));

    cvSet2D(pre_solution.get(), 0, 2, cvScalar(dx));
    cvSet2D(pre_solution.get(), 1, 2, cvScalar(dy));
  } else {
    const float a = cvGet2D(solution.get(), 2, 0).val[0];
    const float b = cvGet2D(solution.get(), 3, 0).val[0];
    const float c = cvGet2D(solution.get(), 4, 0).val[0];
    const float d = cvGet2D(solution.get(), 5, 0).val[0];
    
    const float dx = cvGet2D(solution.get(), 0, 0).val[0];
    const float dy = cvGet2D(solution.get(), 1, 0).val[0];
    cvSet2D(pre_solution.get(), 0, 0, cvScalar(a));
    cvSet2D(pre_solution.get(), 0, 1, cvScalar(b));
    cvSet2D(pre_solution.get(), 1, 0, cvScalar(c));
    cvSet2D(pre_solution.get(), 1, 1, cvScalar(d));
    
    cvSet2D(pre_solution.get(), 0, 2, cvScalar(dx));
    cvSet2D(pre_solution.get(), 1, 2, cvScalar(dy));    
  }
  
  shared_ptr<CvMat> tmp_mat = cvCreateMatShared(3, 3, CV_32F);
  cvGEMM(pre_solution.get(), &pre_transform, 1.0, NULL, 0, tmp_mat.get());
  shared_ptr<CvMat> inv_pre_transform = cvCreateMatShared(3, 3, CV_32F);
  cvInvert(&pre_transform, inv_pre_transform.get());
  cvGEMM(inv_pre_transform.get(), tmp_mat.get(), 1.0, NULL, 0, model);
}
  
void StabilizeUnit::EstimatePerspective(const vector<Feature>& features,
                                        const CvMat* pre_transform,
                                        CvMat* model) {
  shared_ptr<CvMat> A_mat = cvCreateMatShared(2, 2, CV_64F);
  cvZero(A_mat.get());
    
  shared_ptr<CvMat> b_mat = cvCreateMatShared(2, 1, CV_64F);
  cvZero(b_mat.get());
    
  shared_ptr<CvMat> J_mat = cvCreateMatShared(2, 2, CV_64F);

  for (vector<Feature>::const_iterator feat = features.begin();
        feat != features.end();
        ++feat) {
    CvPoint2D32f loc = transform(feat->loc, pre_transform);
    CvPoint2D32f prev_loc = feat->loc  + feat->vec;
      
    // Compute Jacobian at loc.
    cvZero(J_mat.get());
    
    cvSet2D(J_mat.get(), 0, 0, cvScalar(prev_loc.x * loc.x));
    cvSet2D(J_mat.get(), 0, 1, cvScalar(prev_loc.x * loc.y));

    cvSet2D(J_mat.get(), 1, 0, cvScalar(prev_loc.y * loc.x));
    cvSet2D(J_mat.get(), 1, 1, cvScalar(prev_loc.y * loc.y));
      
    cvGEMM(J_mat.get(), J_mat.get(), 1.0, A_mat.get(), 1.0, A_mat.get(), CV_GEMM_A_T);
      
    // Set rhs.
    CvMat rhs;
    CvPoint2D32f diff = loc - prev_loc;
    double rhs_data[2] = { diff.x, diff.y };
    cvInitMatHeader(&rhs, 2, 1, CV_64F, rhs_data, sizeof(double));
    cvGEMM(J_mat.get(), &rhs, 1.0, b_mat.get(), 1.0, b_mat.get(), CV_GEMM_A_T);
  }
  
  shared_ptr<CvMat> solution(cvCreateMatShared(2, 1, CV_64F));
  cvSolve(A_mat.get(), b_mat.get(), solution.get());
  
  cvSetIdentity(model);
  const float w_1 = cvGet2D(solution.get(), 0, 0).val[0];
  const float w_2 = cvGet2D(solution.get(), 1, 0).val[0];
    
  cvSet2D(model, 2, 0, cvScalar(w_1));
  cvSet2D(model, 2, 1, cvScalar(w_2));
}
  
void StabilizeUnit::EstimateHomography(const vector<Feature>& features,
                                       CvMat* homography) {
  const float frame_diam = sqrt(frame_width_ * frame_width_ + 
                                frame_height_ * frame_height_);
  const float s = 1.0 / (frame_diam);
  
  float pre_transform_data[9] = { s, 0, -0.5,
                                  0, s, -0.5,
                                  0, 0,   1};
  
  CvMat pre_transform;
  cvInitMatHeader(&pre_transform, 3, 3, CV_32F, pre_transform_data);
  
  shared_ptr<CvMat> a_mat = cvCreateMatShared(2 * features.size(), 9, CV_32F);
  cvZero(a_mat.get());
  
  shared_ptr<CvMat> points_x = cvCreateMatShared(features.size(), 2, CV_32F);
  shared_ptr<CvMat> points_y = cvCreateMatShared(features.size(), 2, CV_32F);
  
  // Compute homography from features. (H * location = prev_location)
  int feat_idx = 0;
  
  for (vector<Feature>::const_iterator feat = features.begin();
       feat != features.end();
       ++feat, ++feat_idx) {
    float* point_1 = RowPtr<float>(points_x, feat_idx);
    float* point_2 = RowPtr<float>(points_y, feat_idx);
    
    point_1[0] = feat->loc.x;
    point_1[1] = feat->loc.y;

    point_2[0] = feat->loc.x + feat->vec.x;
    point_2[1] = feat->loc.y + feat->vec.y;
    
    
    float* row_1 = RowPtr<float>(a_mat, 2 * feat_idx);
    float* row_2 = RowPtr<float>(a_mat, 2 * feat_idx + 1);
    
    // We expect homography to be already initialized. 
    // Only use those features that already match under the similarity.
    if (norm(transform(feat->loc, homography) - (feat->loc + feat->vec)) > 4) {
      --feat_idx;
      continue;
    }
    
    CvPoint2D32f loc = transform(feat->loc, &pre_transform);
    CvPoint2D32f prev_loc = transform(feat->loc  + feat->vec, &pre_transform);
    
    row_1[0] = row_1[1] = row_1[2] = 0.0;
    
    row_1[3] = -loc.x;
    row_1[4] = -loc.y;
    row_1[5] = -1.0;
        
    row_1[6] = loc.x * prev_loc.y;
    row_1[7] = loc.y * prev_loc.y;
    row_1[8] = prev_loc.y;
        
    row_2[0] = loc.x;
    row_2[1] = loc.y;
    row_2[2] = 1.0;
        
    row_2[3] = row_2[4] = row_2[5] = 0.0;
        
    row_2[6] = -loc.x * prev_loc.x;
    row_2[7] = -loc.y * prev_loc.x;
    row_2[8] = -prev_loc.x;
  }
  
  // Test. Save to file.
  
  //  std::ofstream ofs("feature_mat.txt");
  //  row_1 = a_mat;
  //  for (int i = 0; i < 2 * features.size(); ++i, row_1 = PtrOffset(row_1, a_step)) {
  //    std::copy(row_1, row_1 + 9, std::ostream_iterator<float>(ofs, " "));
  //    if (i < 2 * features.size() - 1)
  //      ofs << "\n";
  //  }
  //  ofs.close();
  
  
  // Calculate SVD of A^T.
  // We need the last column
  int svd_m = 9;
  int svd_n = 2 * features.size();
  int lda = a_mat->step / sizeof(float);
  ASSERT_LOG(a_mat->step % sizeof(float) == 0) << "Step must be multiple of sizeof(float)\n";
  
  float singular_values[9];
  int lwork = (5 * svd_m + svd_n) * 100;
  float* work = new float[lwork];
  int info;
  
  int nine = 9;
  
  sgesvd("O",   // jobu
         "N",   // jobvt
         &svd_m,
         &svd_n,
         a_mat->data.fl,
         &lda,
         singular_values,
         0,   // u
         &nine,   // ldu
         0,   // vt
         &nine,   // ldvt
         work,
         &lwork,
         &info); 
  
  delete [] work;
  
  // Convert solution (last column of U) to Transform.
  float* h = RowPtr<float>(a_mat, 8);
  /*
  std::ofstream ofs2_h ("homography.txt");
  std::copy(h, h + 9, std::ostream_iterator<float>(ofs2_h, " "));
  ofs2_h.close();
  */
  
  // return and normalize to h[8] to 1.
  CvMat h_view;
  cvInitMatHeader(&h_view, 3, 3, CV_32F, h);
  cvScale(&h_view, &h_view, 1.0 / h[8]);
  
  shared_ptr<CvMat> tmp_mat = cvCreateMatShared(3, 3, CV_32F);
  cvGEMM(&h_view, &pre_transform, 1.0, NULL, 0, tmp_mat.get());
  shared_ptr<CvMat> inv_pre_transform = cvCreateMatShared(3, 3, CV_32F);
  cvInvert(&pre_transform, inv_pre_transform.get());
  cvGEMM(inv_pre_transform.get(), tmp_mat.get(), 1.0, NULL, 0, homography);
  cvScale(homography, homography, 1.0 / cvGet2D(homography, 2, 2).val[0]);
}
  
void StabilizeUnit::FillFeaturePatch(const IplImage* image,
                                     CvPoint pt,
                                     int patch_rad,
                                     ColorFeature* feature) {
  const int min_x = std::max(0, pt.x - patch_rad);
  const int max_x = std::min(image->width - 1, pt.x + patch_rad);

  const int min_y = std::max(0, pt.y - patch_rad);
  const int max_y = std::min(image->height - 1, pt.y + patch_rad);
  
  feature->mean = cvPoint3D32f(0, 0, 0);
  feature->var = cvPoint3D32f(0, 0, 0);
  
  const int pixel_sz = (max_y - min_y + 1) * (max_x - min_x + 1);
  if (pixel_sz == 0) {
    return;
  }
  
  const float inv_pixel_sz = 1.0f / pixel_sz;
  
  for (int i = min_y; i <= max_y; ++i) {
    const uchar* row_ptr = RowPtr<uchar>(image, i) + 3 * min_x;
    for (int j = min_x; j <= max_x; ++j, row_ptr += 3) {
      feature->mean = feature->mean + 
      cvPoint3D32f(row_ptr[0], row_ptr[1], row_ptr[2]);
      feature->var = feature->var + 
      cvPoint3D32f(row_ptr[0] * row_ptr[0],
                   row_ptr[1] * row_ptr[1],
                   row_ptr[2] * row_ptr[2]);
    }
  }
  feature->mean = feature->mean * inv_pixel_sz * (1.0 / 255.0f);
  feature->var = feature->var * inv_pixel_sz * (1.0 / 255.0f / 255.0f);
  
  feature->var.x -= feature->mean.x * feature->mean.x;
  feature->var.y -= feature->mean.y * feature->mean.y;
  feature->var.z -= feature->mean.z * feature->mean.z;
}
  
namespace {
  template<int border, int cn>
  void CopyBorder(CvMat* mat) {
    const int width = mat->width / cn - 2 * border;
    const int height = mat->height - 2 * border;
    
    // Top rows.
    for (int r = 0; r < border; ++r) {
      const float* src_ptr = RowPtr<float>(mat, border + r) + border * cn;
      float* dst_ptr = RowPtr<float>(mat, border - 1 - r);
    
      // Top left elems.
      for (int i = 0; i < border; ++i, dst_ptr += cn) {
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[(border - 1 - i)*cn +j];
        }
        ASSERT_LOG(!std::isnan(src_ptr[0]));
      }
    
      // src and dst should point to same column from here.
      ASSERT_LOG((src_ptr - dst_ptr) * sizeof(float) % mat->step == 0);
      
      for (int i = 0; i < width; ++i, dst_ptr += cn, src_ptr += cn) {
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[j];
        }
        ASSERT_LOG(dst_ptr[-1] >= 0);
      }
      
      // Top right elems.
      for (int i = 0; i < border; ++i, dst_ptr += cn) {
        src_ptr -= cn;
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[j];
        }
        ASSERT_LOG(dst_ptr[-1] >= 0);
      }
    }
      
    // Left and right border.
    for (int r = 0; r < height; ++r) {
      float* left_ptr = RowPtr<float>(mat, r + border) + border * cn;
      float* right_ptr = left_ptr + (width - 1) * cn;
      for (int i = 0; i < border; ++i) {
        for (int j = 0; j < cn; ++j) {
          left_ptr[-(i+1)*cn + j] = left_ptr[i*cn + j];  
          right_ptr[(i+1)*cn + j] = right_ptr[-i*cn + j];
        }
      }
    }
    
    // Bottom rows.
    for (int r = 0; r < border; ++r) {
      const float* src_ptr = RowPtr<float>(mat, border + height - 1 - r) +
          border * cn;
      
      float* dst_ptr = RowPtr<float>(mat, border + height + r);
      
      // First elems.
      for (int i = 0; i < border; ++i, dst_ptr += cn) {
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[(border - 1 - i)*cn +j];
        }
      }
      
      // src and dst should point to same column from here.
      ASSERT_LOG((dst_ptr - src_ptr) * sizeof(float) % mat->step == 0);
      
      for (int i = 0; i < width; ++i, dst_ptr += cn, src_ptr += cn) {
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[j];
        }
      }
      
      // Top right elems.
      for (int i = 0; i < border; ++i, dst_ptr += cn) {
        src_ptr -= cn;
        for (int j = 0; j < cn; ++j) {
          dst_ptr[j] = src_ptr[j];
        }
      }
    }
  }
}

  // Scattered data interpolation via PushPull algorithm.
  // Interpolation is performed over a size of domain_sz, where
  // features are placed into the corresponding bin at location
  // feature.loc + origin.
  // The output displacement is assumed be a 3 channel float image of
  // size ==domain_sz + 2 to account for a one pixel border.
void StabilizeUnit::PushPullErrors(const vector<Feature>& feat_errors, 
                                   CvPoint origin,
                                   CvSize domain_sz,
                                   PushPullFilter filter,
                                   CvMat* disp) {
  float bin5_weights[25] = {
    1,  4,  6,  4,  1,
    4,  16, 24, 16, 4,
    6,  24, 36, 24, 6, 
    4,  16, 24, 16, 4, 
    1,  4,  6,  4,  1 };
  
  float bin3_weights[9] = {
    1,  2,  1,
    2,  4,  2,
    1,  2,  1};
  
  // Normalize maximum of binomial filters to 1.
  for (int i = 0; i < 25; ++i) {
    bin5_weights[i] *= 1.f / 36.f;
  }
  
  for (int i = 0; i < 9; ++i) {
    bin3_weights[i] *= 1.f / 4.f;
  }
  
  int border;
  int num_filter_elems;
  float* filter_weights;
  
  switch (filter) {
    case BINOMIAL_3x3:
      border = 1;
      num_filter_elems = 9;
      filter_weights = bin3_weights;
      break;
    case BINOMIAL_5x5:
      border = 2;
      num_filter_elems = 25;
      filter_weights = bin5_weights;
      break;
    default:
      LOG(FATAL) << "Unknown filter requested.";
  }
  
  ASSURE_LOG(disp->width == 3 * (domain_sz.width + 2 * border));
  ASSURE_LOG(disp->height == domain_sz.height + 2 * border);
  
  origin.x += border;
  origin.y += border;
  
  // Disp is supposed to be a 3D float matrix.
  cvZero(disp);
  
  // Place feat_erros into disp.
  for (vector<Feature>::const_iterator feat = feat_errors.begin();
       feat != feat_errors.end();
       ++feat) {
    CvPoint pt = trunc(cvPoint2D32f(origin.x, origin.y) + feat->loc);
    float* ptr = RowPtr<float>(disp, pt.y) + 3 * pt.x;
    ptr[0] = feat->vec.x;
    ptr[1] = feat->vec.y;
    ptr[2] = 1;
  }
  
  return;
  
  // Create pyramids
  // TODO: Test for external ones.
  vector<shared_ptr<CvMat> > pyramid;
  vector<shared_ptr<CvMat> > tmp_upsample_pyramid;
  int width = domain_sz.width; 
  int height = domain_sz.height;
  
  while (width > 1 && height > 1) {
    tmp_upsample_pyramid.push_back(cvCreateMatShared(height + 2 * border,
                                                     3 * (width + 2 * border), CV_32F));
    width = (width + 1) / 2;
    height = (height + 1) / 2;
    
    pyramid.push_back(cvCreateMatShared(height + 2 * border,
                                        3 * (width + 2 * border), CV_32F));
  }
  
  // Create mip-map view.
  vector<CvMat*> mip_map(pyramid.size() + 1);
  mip_map[0] = disp;
  for (int i = 0; i < pyramid.size(); ++i) {
    mip_map[i + 1] = pyramid[i].get();
  }
  
  // Filter pyramid via push-pull.
  // We always filter from [border, border] to 
  // [width - 1 - border, height - 1 - border].
  
  // Pull-phase.
  for (int l = 1; l < mip_map.size(); ++l) {    
    for (int i = 0; i < mip_map[l - 1]->height - 2 * border; ++i) {
      float* dst_ptr = RowPtr<float>(mip_map[l - 1], i + border) + border * 3;
      for (int j = 0; j < mip_map[l - 1]->width / 3 - 2 * border; ++j, dst_ptr += 3) { 
        ASSERT_LOG(dst_ptr[2] >= 0);
        ASSERT_LOG(!std::isnan(dst_ptr[0]));
      }
    }    
    
    // Copy border.
    switch (filter) {
      case BINOMIAL_3x3:
        CopyBorder<1, 3>(mip_map[l - 1]);
        break;
      case BINOMIAL_5x5:
        CopyBorder<2, 3>(mip_map[l - 1]);
        break;
    }
    
    // No neg. weight test.
    for (int i = 0; i < mip_map[l - 1]->height; ++i) {
      float* dst_ptr = RowPtr<float>(mip_map[l - 1], i);
      for (int j = 0; j < mip_map[l - 1]->width / 3; ++j, dst_ptr += 3) {
        ASSERT_LOG(!std::isnan(dst_ptr[0]));
        ASSERT_LOG(dst_ptr[2] >= 0);
      }
    }
    
    // Setup offsets.
    vector<int> filter_off;   

    int idx = 0;
    for (int i = -border; i <= border; ++i) {
      for (int j = -border; j <= border; ++j) {
        filter_off.push_back(i * mip_map[l - 1]->step + sizeof(float) * 3 * j);
      }
    }
    
    // Filter odd pixels (downsample).
    for (int i = 0, height = mip_map[l]->height - 2 * border;
         i < height; ++i) {
      ASSERT_LOG(tmp_upsample_pyramid[6]->step > 0);
      
      float* dst_ptr = RowPtr<float>(mip_map[l], i + border) + border * 3;
      const float* src_ptr = RowPtr<float>(mip_map[l - 1], 2*i + border) + border * 3;
      
      for (int j = 0, width = mip_map[l]->width / 3 - 2 * border; 
           j < width;
           ++j, dst_ptr += 3, src_ptr += 2 * 3) {
        ASSERT_LOG(tmp_upsample_pyramid[6]->step > 0);
        float weight_sum = 0;
        float val_sum_x = 0;
        float val_sum_y = 0;
        for (int k = 0; k < num_filter_elems; ++k) {
          const float* cur_ptr = PtrOffset(src_ptr, filter_off[k]);
          const float w = cur_ptr[2] * filter_weights[k];
          ASSERT_LOG(w >= 0);
          
          val_sum_x += cur_ptr[0] * w;
          val_sum_y += cur_ptr[1] * w;
          
          weight_sum += w;
        }
        
        ASSERT_LOG(weight_sum >= 0);
        
        dst_ptr[2] = std::min(1.f, weight_sum);
        
        if (weight_sum > 1e-10) {
          const float inv_weight_sum = 1.f / weight_sum;
          dst_ptr[0] = val_sum_x * inv_weight_sum;
          dst_ptr[1] = val_sum_y * inv_weight_sum;
        } else {
          dst_ptr[0] = 0;
          dst_ptr[1] = 0;
          dst_ptr[2] = 0;
        }
        
        ASSERT_LOG(!std::isnan(dst_ptr[0]));
        ASSERT_LOG(!std::isnan(dst_ptr[1]));
        ASSERT_LOG(!std::isnan(dst_ptr[2]));
      }
    }
  }
  
  // Zero upsample pyramids.
  for (int i = 0; i < tmp_upsample_pyramid.size(); ++i) {
    cvZero(tmp_upsample_pyramid[i].get());
  }
  
  // Push-phase.
  for (int l =  mip_map.size() - 2; l >= 0; --l) {
    // Upsample higher level to current.
    for (int i = 0, height = mip_map[l + 1]->height - 2 * border;
         i < height;
         ++i) {
      const float* src_ptr = RowPtr<float>(mip_map[l + 1], i + border) + border * 3;
      float* dst_ptr = RowPtr<float>(tmp_upsample_pyramid[l], 2*i + border) + border * 3;
            
      for (int j = 0, width = mip_map[l + 1]->width / 3 - 2 * border; 
             j < width;
             ++j, dst_ptr += 2 * 3, src_ptr += 3) {
        dst_ptr[0] = src_ptr[0];
        dst_ptr[1] = src_ptr[1];
        dst_ptr[2] = src_ptr[2];
      }
    }
    
    // Copy border.
    switch (filter) {
      case BINOMIAL_3x3:
        CopyBorder<1, 3>(tmp_upsample_pyramid[l].get());
        break;
      case BINOMIAL_5x5:
        CopyBorder<2, 3>(tmp_upsample_pyramid[l].get());
        break;
    }
    
    // Setup offsets.
    vector<int> filter_off;   
    
    int idx = 0;
    for (int i = -border; i <= border; ++i) {
      for (int j = -border; j <= border; ++j) {
        filter_off.push_back(i * tmp_upsample_pyramid[l]->step + sizeof(float) * 3 * j);
      }
    }
    
    // Apply filter.
    for (int i = 0, height = mip_map[l]->height - 2 * border;
         i < height; ++i) {
      float* dst_ptr = RowPtr<float>(mip_map[l], i + border) + border * 3;
      const float* src_ptr = RowPtr<float>(tmp_upsample_pyramid[l], i + border) + border * 3;
      
      for (int j = 0, width = mip_map[l]->width / 3 - 2 * border; 
           j < width;
           ++j, dst_ptr += 3, src_ptr += 3) {
        // If this pixel is already saturated, no need to perform interpolation.
        if (dst_ptr[2] >= 1) {
          continue;
        }
        
        float weight_sum = 0;
        float val_sum_x = 0;
        float val_sum_y = 0;
        
        for (int k = 0; k < num_filter_elems; ++k) {
          const float* cur_ptr = PtrOffset(src_ptr, filter_off[k]);
          const float w = cur_ptr[2] * filter_weights[k];
          
          val_sum_x += cur_ptr[0] * w;
          val_sum_y += cur_ptr[1] * w;
          
          weight_sum += w;
        }
        
        if (weight_sum > 1e-10) {
          const float inv_weight_sum = 1.f / weight_sum;
          val_sum_x *= inv_weight_sum;
          val_sum_y *= inv_weight_sum;
        }
        
        // Blend.
        dst_ptr[0] = (1.f - dst_ptr[2]) * val_sum_x + dst_ptr[2] * dst_ptr[0];
        dst_ptr[1] = (1.f - dst_ptr[2]) * val_sum_y + dst_ptr[2] * dst_ptr[1];
        dst_ptr[2] = std::min(1.0f, 
                             (1.f - dst_ptr[2]) * weight_sum + dst_ptr[2] * dst_ptr[2]);
        ASSERT_LOG(!std::isnan(dst_ptr[0]));
        ASSERT_LOG(!std::isnan(dst_ptr[1]));
        ASSERT_LOG(!std::isnan(dst_ptr[2]));
      }
    }
  }
  
}  // anon. namespace.
  
  void StabilizeUnit::GetCorrection(const vector<Feature>& feat_errors,
                                    const IplImage* image,
                                    CvMat* disp_x,
                                    CvMat* disp_y) {
    const float rbf_std_dev = 30;
    
    // Compute displacement field for every pixel.
    shared_ptr<IplImage> magn = 
        cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 3);
    
    const int grid_spacing = 5;
    const int grid_dim_x = std::ceil((float)frame_width_ / grid_spacing) + 1;
    const int grid_dim_y = std::ceil((float)frame_height_ / grid_spacing) + 1;
    
    const int grid_size = grid_dim_x * grid_dim_y;
    const float space_coeff = -0.5 / (rbf_std_dev * rbf_std_dev);
    
    vector<CvPoint2D32f> grid_disp(grid_size);
    CvPoint2D32f* out_ptr = &grid_disp[0];
    for (int i = 0; i < grid_dim_y; ++i) {
      int curr_y = std::min(i * grid_spacing, frame_height_ - 1);
      for (int j = 0; j < grid_dim_x; ++j, ++out_ptr) {
        CvPoint2D32f pt = cvPoint2D32f(std::min(j * grid_spacing, frame_width_ - 1),
                                       curr_y);
        
        CvPoint2D64f error = cvPoint2D64f(0, 0);
        double weight_sum = 0;
        
        // Only walk over adjacent bins.
        for (vector<Feature>::const_iterator feat = feat_errors.begin();
             feat != feat_errors.end();
             ++feat) {
          const double r = sq_norm(feat->loc - pt);
          const double weight = exp(r * space_coeff);
          error = cvPoint2D64f(error.x + feat->vec.x * weight,
                               error.y + feat->vec.y * weight);
          weight_sum += weight;
        }
        
        if (weight_sum > 1e-30) {
          error = cvPoint2D64f(error.x / weight_sum,
                               error.y / weight_sum);
        }
        
        *out_ptr = cvPoint2D32f(error.x, error.y);
      }
    }
    
    // Compute the interpolant weights and increments for one grid cell.
    vector<float> interp_weights(6 * grid_spacing * grid_spacing);
    const float inv_grid_spacing = 1.0 / grid_spacing;
    float* weight_ptr = &interp_weights[0];
    for (int i = 0; i < grid_spacing; ++i) {
      for (int j = 0; j < grid_spacing; ++j, weight_ptr += 6) {
        const float dx = (float)j * inv_grid_spacing;
        const float dx_1 = 1.0f - dx;
        const float dy = (float)i * inv_grid_spacing;
        const float dy_1 = 1.0f - dy;
        
        weight_ptr[0] = dx_1 * dy_1;
        weight_ptr[1] = dx * dy_1;
        weight_ptr[2] = dx_1 * dy;       
        weight_ptr[3] = dx * dy;
        weight_ptr[4] = j != 0;
        weight_ptr[5] = (i != 0) * grid_dim_x;
      }
    }
    
    cvZero(disp_x);
    cvZero(disp_y);
    if (image) {
      cvScale(image, magn.get(), 0.1f);
    }
    
    // Interpolate solution from grid.
    for (int i = 0; i < grid_dim_y; ++i) {
      const CvPoint2D32f* row_grid_ptr = &grid_disp[i * grid_dim_x];
      for (int y = 0, 
           end_y = std::min(grid_spacing, frame_height_ - i * grid_spacing);
           y < end_y;
           ++y) {
        const CvPoint2D32f* grid_ptr = row_grid_ptr;
        float* disp_x_ptr = RowPtr<float>(disp_x, i * grid_spacing + y);
        float* disp_y_ptr = RowPtr<float>(disp_y, i * grid_spacing + y);
        uchar* mag_ptr = RowPtr<uchar>(magn, i * grid_spacing + y);
        
        for (int j = 0; j < grid_dim_x; ++j, ++grid_ptr) {  
          const float* weight_ptr = &interp_weights[y * grid_spacing * 6];
          for (int x = 0,
               end_x = std::min(grid_spacing, frame_width_ - j * grid_spacing);
               x < end_x;
               ++x, weight_ptr += 6, ++disp_x_ptr, ++disp_y_ptr, mag_ptr+=3) {
            const int inc_x = weight_ptr[4];
            const int inc_y = weight_ptr[5];
            CvPoint2D32f error = grid_ptr[0] * weight_ptr[0] + 
            grid_ptr[inc_x] * weight_ptr[1] +
            grid_ptr[inc_y] * weight_ptr[2] +
            grid_ptr[inc_y + inc_x] * weight_ptr[3];
            *disp_x_ptr = error.x;
            *disp_y_ptr = error.y;
            
            const int int_error = norm(error) * sqrt(2) * 150 * 0.9;
            mag_ptr[0] += int_error;
            mag_ptr[1] += int_error;
            mag_ptr[2] += int_error;
          }
        }
      }
    }

    if (image) {
      // Visualize residuals.
      for (vector<Feature>::const_iterator feat = feat_errors.begin();
           feat != feat_errors.end();
           ++feat) {
        cvLine(magn.get(), 
               trunc(feat->loc), 
               trunc(feat->loc + feat->vec * 10), 
               cvScalar(0., 255, 0));
      }
      
      cvNamedWindow("test_out");
      cvShowImage("test_out", magn.get());
      cvWaitKey(10);  
    }
  }
    
  void StabilizeUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);
    
    IplImage image;
    frame->ImageView(&image);
    frame_views_.push_back(image);

    // Get optical flow.
    const OpticalFlowFrame* flow_frame = 
        dynamic_cast<const OpticalFlowFrame*>(input->at(flow_stream_idx_).get());
    shared_ptr<CvMat> homog_copy(cvCreateMatShared(3, 3, CV_32F));
    cvSetIdentity(homog_copy.get());
    
    shared_ptr<CvMat> frame_homog(cvCreateMatShared(3, 3, CV_32F));
    cvSetIdentity(frame_homog.get());
    
    shared_ptr<CvMat> disp_x_forward = cvCreateMatShared(frame_height_, frame_width_, 
                                                         CV_32F);
    shared_ptr<CvMat> disp_y_forward = cvCreateMatShared(frame_height_, frame_width_,
                                                         CV_32F);
    shared_ptr<CvMat> disp_x_backward = cvCreateMatShared(frame_height_, frame_width_, 
                                                         CV_32F);
    shared_ptr<CvMat> disp_y_backward = cvCreateMatShared(frame_height_, frame_width_,
                                                         CV_32F);
    
    cvZero(disp_x_forward.get());
    cvZero(disp_y_forward.get());
    cvZero(disp_x_backward.get());
    cvZero(disp_y_backward.get());
    
    if (flow_frame->MatchedFrames() == 0) {
      frame_sets_.push_back(input);
      camera_path_.push_back(homog_copy);
      homographies_.push_back(frame_homog);
      
      correction_x_forward_.push_back(disp_x_forward);
      correction_y_forward_.push_back(disp_y_forward);
      correction_x_backward_.push_back(disp_x_backward);
      correction_y_backward_.push_back(disp_y_backward);
      
      return;
    }
    
    vector<Feature> features(flow_frame->NumberOfFeatures(0));
    flow_frame->FillVector(&features, 0);
    LOG(INFO) << "Features: " << features.size();
    
    shared_ptr<CvMat> homography(cvCreateMatShared(3, 3, CV_32F));
    cvSetIdentity(homography.get());
    EstimateLinearMotion(features, true, homography.get());

    shared_ptr<CvMat> homog_tmp(cvCreateMatShared(3, 3, CV_32F));
    cvGEMM(homog_composed_.get(), homography.get(), 1.0, NULL, 0, homog_tmp.get());
    cvCopy(homog_tmp.get(), homog_composed_.get());
    
    // Add constant motion.
    shared_ptr<CvMat> inter_frame(cvCreateMatShared(3, 3, CV_32F));
    cvSetIdentity(inter_frame.get());
    cvSet2D(inter_frame.get(), 0, 2, cvScalar(MOTION_X));
    cvSet2D(inter_frame.get(), 1, 2, cvScalar(MOTION_Y));
    cvGEMM(homog_composed_.get(), inter_frame.get(), 1.0, NULL, 0, homog_tmp.get());
    cvCopy(homog_tmp.get(), homog_composed_.get());    

    // Copy for later warping.
    cvCopy(homog_composed_.get(), homog_copy.get());
    camera_path_.push_back(homog_copy);
    
    EstimateHomography(features, homography.get());
    homographies_.push_back(homography);
    
    // Evaluate feature errors.
    /*
    const float feat_grid_scale = 1.0 / FEATURE_GRID_SPACING;
    shared_ptr<FeaturePatchGrid> forward_grid(new FeaturePatchGrid(feat_grid_sz_));
    shared_ptr<FeaturePatchGrid> backward_grid(new FeaturePatchGrid(feat_grid_sz_));
    */
    
    vector<Feature> forward_errors;
    vector<Feature> backward_errors;
    for (vector<Feature>::const_iterator feat = features.begin();
         feat != features.end();
         ++feat) {
      CvPoint2D32f trans_loc = transform(feat->loc, homography.get());
      CvPoint2D32f error_vec = feat->loc + feat->vec - trans_loc;

      // Only allow for small corrections.
      if (norm(error_vec) < 2) {
        Feature forward_error_feature(feat->loc + feat->vec, error_vec);
        Feature backward_error_feature(trans_loc, error_vec);
        forward_errors.push_back(forward_error_feature);
        backward_errors.push_back(backward_error_feature);
      } else {
        LOG(INFO) << "Rejected: " << norm(error_vec);
      }
    }
    
    shared_ptr<CvMat> disp = cvCreateMatShared(frame_height_ + 2,
                                               3 * (frame_width_ + 2),
                                               CV_32F);
    const int border = 1;
    PushPullErrors(forward_errors, cvPoint(0, 0), 
                   cvSize(frame_width_, frame_height_),
                   BINOMIAL_5x5, disp.get());
    shared_ptr<IplImage> magn = 
       cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 3);
    
    for (int i = 0; i < frame_height_; ++i) {
      float* disp_x_ptr = RowPtr<float>(disp_x_forward.get(), i);
      float* disp_y_ptr = RowPtr<float>(disp_y_forward.get(), i);
      float* disp_ptr = RowPtr<float>(disp.get(), i + border) + 3 * border;
      uchar* magn_ptr = RowPtr<uchar>(magn.get(), i);
      
      for (int j = 0;
           j < frame_width_; 
           ++j, ++disp_x_ptr, ++disp_y_ptr, disp_ptr += 3, magn_ptr += 3) {
        disp_x_ptr[0] = disp_ptr[0];
        disp_y_ptr[0] = disp_ptr[1];
        const int magn = 
            norm(cvPoint2D32f(disp_ptr[0], disp_ptr[1])) * sqrt(2) * 150;
        magn_ptr[0] = magn;
        magn_ptr[1] = magn;
        magn_ptr[2] = magn;
      }
    }
    
    cvNamedWindow("test_out");
    cvShowImage("test_out", magn.get());
    cvWaitKey(10);      
    
    PushPullErrors(backward_errors, cvPoint(0, 0), 
                   cvSize(frame_width_, frame_height_),
                   BINOMIAL_5x5, disp.get());
    
    for (int i = 0; i < frame_height_; ++i) {
      float* disp_x_ptr = RowPtr<float>(disp_x_backward.get(), i);
      float* disp_y_ptr = RowPtr<float>(disp_y_backward.get(), i);
      float* disp_ptr = RowPtr<float>(disp.get(), i + border) + 3 * border;
      
      for (int j = 0;
           j < frame_width_; 
           ++j, ++disp_x_ptr, ++disp_y_ptr, disp_ptr += 3) {
        disp_x_ptr[0] = disp_ptr[0];
        disp_y_ptr[0] = disp_ptr[1];
      }
    }
    
    //GetCorrection(forward_errors, &image, disp_x_forward.get(), disp_y_forward.get());
    //GetCorrection(backward_errors, NULL, disp_x_backward.get(), disp_y_backward.get());
    correction_x_forward_.push_back(disp_x_forward);
    correction_y_forward_.push_back(disp_y_forward);

    correction_x_backward_.push_back(disp_x_backward);
    correction_y_backward_.push_back(disp_y_backward);

    frame_sets_.push_back(input);
    
    /*
    LOG(INFO) << "Homography for frame " << frame_num_;
    for (int i = 0; i < 3; ++i) {
      const float* row_ptr = RowPtr<float>(homog_composed_.get(), i);
      LOG(INFO) << row_ptr[0] << " " << row_ptr[1] << " " << row_ptr[2];
    }*/
    
    LOG(INFO) << "Processed frame " << frame_num_;
    ++frame_num_;
  }
  
  void StabilizeUnit::PostProcess(list<FrameSetPtr>* append) {
    LOG(INFO) << "POST PROCESS";
    
    if (output_flushed_) {
      return;
    }
    
    shared_ptr<CvMat> camera_invert(cvCreateMatShared(3, 3, CV_32F));
    shared_ptr<CvMat> camera_forward(cvCreateMatShared(3, 3, CV_32F));
    shared_ptr<CvMat> camera_backward(cvCreateMatShared(3, 3, CV_32F));
    const int key_frames = 30;
    
    for (int f = 0; f < frame_num_; ++f) {
      FrameSetPtr frame_set = frame_sets_.front();
      frame_sets_.pop_front();
      
      VideoFrame* frame = 
          dynamic_cast<VideoFrame*>(frame_set->at(video_stream_idx_).get());
      ASSERT_LOG(frame);
      
      IplImage image;
      frame->ImageView(&image);
      cvCopy(&image, input_.get());
      
      shared_ptr<CvMat> camera_transform = camera_path_[f];
      const float crop_scale = 0.75;
      
      // Use camera_transform only at keyframes.
      if (f % key_frames == 0 || f == frame_num_ - 1) {
        // Create mapping matrices.
        cvInvert(camera_transform.get(), camera_invert.get());
        
        for (int i = 0; i < frame_height_; ++i) {
          float* x_ptr = RowPtr<float>(map_x_, i);
          float* y_ptr = RowPtr<float>(map_y_, i);
          
          for (int j = 0; j < frame_width_; ++j, ++x_ptr, ++y_ptr) {
            CvPoint2D32f pt = cvPoint2D32f(j, i) * crop_scale + 
            cvPoint2D32f(frame_width_ * (1.f - crop_scale) * 0.5,
                         frame_height_ * (1.f - crop_scale) * 0.5);
            CvPoint2D32f pt_trans = transform(pt, camera_invert.get());
            *x_ptr = pt_trans.x;
            *y_ptr = pt_trans.y;
          }
        }
      } else {
        // Use two transforms here.
        int prev_key_frame = (f / key_frames) * key_frames;
        int next_key_frame = std::min(prev_key_frame + key_frames, frame_num_ - 1);
        float frame_diff = next_key_frame - prev_key_frame;
        
        // Forward warp from prev_key_frame.
        LOG(INFO) << "Frame " << f << " warping backward from keyframe " 
                  << next_key_frame;

        shared_ptr<CvMat> prev_camera_transform = camera_path_[prev_key_frame];
        shared_ptr<CvMat> next_camera_transform = camera_path_[next_key_frame];
        const float alpha = (f - prev_key_frame) / frame_diff;
        
        // Create per pixel warp, only evaluate on grid points.
        const int grid_spacing = 5;
        const int grid_dim_x = std::ceil((float)frame_width_ / grid_spacing) +
                               (frame_width_ % grid_spacing == 0);
        const int grid_dim_y = std::ceil((float)frame_height_ / grid_spacing) +
                               (frame_height_ % grid_spacing == 0);
        const int grid_size = grid_dim_x * grid_dim_y;
        
        vector<CvPoint2D32f> grid_disp(grid_size);
        CvPoint2D32f* out_ptr = &grid_disp[0];
        
        float av_diff = 0;
        
        for (int i = 0; i < grid_dim_y; ++i) {
          int curr_y = std::min(i * grid_spacing, frame_height_ - 1);
          for (int j = 0; j < grid_dim_x; ++j, ++out_ptr) {
            CvPoint2D32f pt_src = cvPoint2D32f(std::min(j * grid_spacing, frame_width_ - 1),
                                               curr_y);
            CvPoint2D32f pt = pt_src * crop_scale + 
            cvPoint2D32f(frame_width_ * (1.f - crop_scale) * 0.5,
                         frame_height_ * (1.f - crop_scale) * 0.5);
            
            // Warp w.r.t. to prev_key_frame.
            cvInvert(prev_camera_transform.get(), camera_invert.get());
            CvPoint2D32f pt_prev = transform(pt, camera_invert.get());
            
            for (int frame_id = prev_key_frame + 1; frame_id <= f; ++frame_id) {
              // Apply correction if still in bounds.
              if (pt_prev.x < 0 || pt_prev.x >= frame_width_ - 1 ||
                  pt_prev.y < 0 || pt_prev.y >= frame_height_ - 1) {
              } else {
                const float shift_x =
                cvGet2D(correction_x_forward_[frame_id].get(), pt_prev.y,
                        pt_prev.x).val[0];
                const float shift_y =
                cvGet2D(correction_y_forward_[frame_id].get(), pt_prev.y,
                        pt_prev.x).val[0];
                
                pt_prev.x -= shift_x;
                pt_prev.y -= shift_y;
              }
              
              shared_ptr<CvMat> homography = homographies_[frame_id];
              cvInvert(homography.get(), camera_invert.get());
              pt_prev = transform(pt_prev, camera_invert.get());   
              
              shared_ptr<CvMat> inter_frame(cvCreateMatShared(3, 3, CV_32F));
              cvSetIdentity(inter_frame.get());
              cvSet2D(inter_frame.get(), 0, 2, cvScalar(-MOTION_X));
              cvSet2D(inter_frame.get(), 1, 2, cvScalar(-MOTION_Y));
              pt_prev = transform(pt_prev, inter_frame.get());  
            }
            
            // Warp w.r.t. to next_key_frame.
            cvInvert(next_camera_transform.get(), camera_invert.get());
            CvPoint2D32f pt_next = transform(pt, camera_invert.get());
            
            for (int frame_id = next_key_frame - 1; frame_id >= f; --frame_id) {
              // Remove constant motion.
              shared_ptr<CvMat> inter_frame(cvCreateMatShared(3, 3, CV_32F));
              cvSetIdentity(inter_frame.get());
              cvSet2D(inter_frame.get(), 0, 2, cvScalar(MOTION_X));
              cvSet2D(inter_frame.get(), 1, 2, cvScalar(MOTION_Y));
              pt_next = transform(pt_next, inter_frame.get());        
                          
              // Remove homography
              shared_ptr<CvMat> homography = homographies_[frame_id + 1];
              pt_next = transform(pt_next, homography.get()); 
                          
              // Apply correction if still in bounds.
              if (pt_next.x < 0 || pt_next.x >= frame_width_ - 1 ||
                  pt_next.y < 0 || pt_next.y >= frame_height_ - 1) {
              } else {
                const float shift_x =
                cvGet2D(correction_x_backward_[frame_id + 1].get(), pt_next.y,
                        pt_next.x).val[0];
                const float shift_y =
                cvGet2D(correction_y_backward_[frame_id + 1].get(), pt_next.y,
                        pt_next.x).val[0];
                
                pt_next.x += shift_x;
                pt_next.y += shift_y;              
              }            
            }
              
            CvPoint2D32f pt_trans = cvPoint2D32f(-1e10, -1e10);
            
            const float coeff = -1.667 * alpha * alpha * alpha +
                                2.5 * alpha * alpha +
                               -1.833 * alpha + 1.f;
            pt_trans = pt_prev * (1.0f - alpha) + pt_next * alpha;
            //             pt_prev * coeff + pt_next * (1.0f - coeff); 
            av_diff += norm(pt_prev - pt_next);
            
            *out_ptr = pt_trans;
          }
        }
        
        LOG(INFO) << "Av diff: " << av_diff / grid_dim_y / grid_dim_x;
        
        vector<float> interp_weights(6 * grid_spacing * grid_spacing);
        const float inv_grid_spacing = 1.0 / grid_spacing;
        float* weight_ptr = &interp_weights[0];
        for (int i = 0; i < grid_spacing; ++i) {
          for (int j = 0; j < grid_spacing; ++j, weight_ptr += 6) {
            const float dx = (float)j * inv_grid_spacing;
            const float dx_1 = 1.0f - dx;
            const float dy = (float)i * inv_grid_spacing;
            const float dy_1 = 1.0f - dy;
            
            weight_ptr[0] = dx_1 * dy_1;
            weight_ptr[1] = dx * dy_1;
            weight_ptr[2] = dx_1 * dy;       
            weight_ptr[3] = dx * dy;
            weight_ptr[4] = j != 0;
            weight_ptr[5] = (i != 0) * grid_dim_x;
          }
        }
        
        // Compute maps by interpolation.
        for (int i = 0; i < grid_dim_y; ++i) {
          const CvPoint2D32f* row_grid_ptr = &grid_disp[i * grid_dim_x];
          for (int y = 0, 
               end_y = std::min(grid_spacing, frame_height_ - i * grid_spacing);
               y < end_y;
               ++y) {
            const CvPoint2D32f* grid_ptr = row_grid_ptr;
            float* map_x_ptr = RowPtr<float>(map_x_, i * grid_spacing + y);
            float* map_y_ptr = RowPtr<float>(map_y_, i * grid_spacing + y);
            
            for (int j = 0; j < grid_dim_x; ++j, ++grid_ptr) {  
              const float* weight_ptr = &interp_weights[y * grid_spacing * 6];
              for (int x = 0,
                   end_x = std::min(grid_spacing, frame_width_ - j * grid_spacing);
                   x < end_x;
                   ++x, weight_ptr += 6, ++map_x_ptr, ++map_y_ptr) {
                const int inc_x = weight_ptr[4];
                const int inc_y = weight_ptr[5];
                CvPoint2D32f mapping = grid_ptr[0] * weight_ptr[0] + 
                                       grid_ptr[inc_x] * weight_ptr[1] +
                                       grid_ptr[inc_y] * weight_ptr[2] +
                                       grid_ptr[inc_y + inc_x] * weight_ptr[3];
                *map_x_ptr = mapping.x;
                *map_y_ptr = mapping.y;
              }
            }
          }
        }
      }
      
      LOG(INFO) << "Warping ...";
      cvRemap(input_.get(), &image, map_x_.get(), map_y_.get(), CV_INTER_CUBIC);
      append->push_back(frame_set);
    }
    
    output_flushed_ = true;
  }
}