/*
 *  foreground_tracker.cpp
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 8/5/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "foreground_tracker.h"
#include <iostream>
#include "image_util.h"
#include "segmentation_util.h"
#include "assert_log.h"
#include "mkl_lapack.h"
#include <QtGui>
#include "levmar.h"
#include "GCoptimization.h"
#include <calib3d/calib3d.hpp>

#include <google/protobuf/repeated_field.h>
using google::protobuf::RepeatedPtrField;

using namespace VideoFramework;

ForegroundTracker::ForegroundTracker(const std::string& feature_file,
                                     bool use_keyframe_tracking,
                                     int ba_max_frames,
                                     float ba_frame_area_ratio,
                                     const std::string& vid_stream_name,
                                     const std::string& seg_stream_name,
                                     const std::string& region_flow_stream_name,
                                     const std::string& key_frame_stream_name,
                                     bool debug_mode) 
    : feature_file_(feature_file), 
      use_key_frame_tracking_(use_keyframe_tracking),
      ba_max_frames_(ba_max_frames),
      ba_frame_area_ratio_(ba_frame_area_ratio),
      vid_stream_name_(vid_stream_name), 
      seg_stream_name_(seg_stream_name),
      region_flow_stream_name_(region_flow_stream_name),
      key_frame_stream_name_(key_frame_stream_name),
      is_debug_mode_(debug_mode) {
  
}

bool ForegroundTracker::OpenStreams(StreamSet* set) {
  if ((vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set)) < 0) {
    std::cerr << "ForegroundTracker::OpenStreams: VideoStream not found.\n";
    return false;
  }
  
  if ((flow_stream_idx_ = FindStreamIdx(region_flow_stream_name_, set)) < 0) {
    std::cerr << "ForegroundTracker::OpenStreams: RegionFlowStream not found.\n";
    return false;
  }
  
  if ((seg_stream_idx_ = FindStreamIdx(seg_stream_name_, set)) < 0) {
    std::cerr << "ForegroundTracker::OpenStreams: SegmentationStream not found.\n";
    return false;
  }
  
  // Attach KeyFrameStream.
  VideoFramework::DataStream* key_frame_stream = 
      new VideoFramework::DataStream(key_frame_stream_name_);
  
  set->push_back(shared_ptr<VideoFramework::DataStream>(key_frame_stream));
  
  // Attach InlierStream.
  VideoFramework::DataStream* inlier_region_stream = 
      new VideoFramework::DataStream("InlierRegionStream");
  set->push_back(shared_ptr<VideoFramework::DataStream>(inlier_region_stream));
  
  ofs_feat_.open(feature_file_.c_str(), std::ios_base::out);
  frame_number_ = 0;
  last_ba_frame_ = 0;
  
  if (is_debug_mode_) 
    ofs_debug_.open((feature_file_ + ".debug").c_str(),
                    std::ios_base::out);
  
  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(set->at(vid_stream_idx_).get());
  frame_width_ = vid_stream->get_frame_width();
  frame_height_ = vid_stream->get_frame_height();
  id_img_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_32S, 1);
  prev_id_img_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_32S, 1);
  ASSURE_LOG(id_img_->widthStep == id_img_->width * sizeof(int))
      << "ForegroundTracker::OpenStreams: ID img must be borderless";
  
  prev_ba_model_ = SimilarityModel();
  concat_model_ = SimilarityModel();
  
  return true;
}

int ForegroundTracker::EstimateModel(const vector<RegionFlow>& region_flow,
                                    int total_features,
                                    float scale,
                                    int frame_dist,
                                    vector<int>* best_inlier_regions,
                                    vector<int>* outlier_regions,
                                    SimilarityModel* model,
                                    vector<float>* inlier_score_output) {
  float max_inliers = 0;
  int max_iterations = 200;
  vector<int> best_inliers;
  
  float err_scale = 1.f + log((float)frame_dist);
  float inv_scale = 1.f / scale;
  
  // Determine average motion magnitude of the region_flow -
  // Discard top and bottom 5%.
  vector<float> motion_magnitudes;
  for (vector<RegionFlow>::const_iterator rf = region_flow.begin();
       rf != region_flow.end();
       ++rf)
    motion_magnitudes.push_back(norm(rf->mean_vector));
  
  sort(motion_magnitudes.begin(), motion_magnitudes.end());
  int percent_shift = motion_magnitudes.size() * 5 / 100;
  float av_motion = std::accumulate(motion_magnitudes.begin() + percent_shift,
                                    motion_magnitudes.end() - percent_shift,
                                    0.f);
  av_motion /= (motion_magnitudes.size() - 2 * percent_shift);
  av_motion *= inv_scale;
  err_scale = std::max<float>(sqrt(av_motion), 1.0f);
  
  vector<shared_ptr<CvMat> > region_pts(region_flow.size());
  vector<shared_ptr<CvMat> > region_lines(region_flow.size());
  vector<float> inlier_score(region_flow.size());
  
  if (inlier_score_output) {
    *inlier_score_output = vector<float>(region_flow.size(), 1.0f);
  }
  
  int idx = 0;
  for (vector<RegionFlow>::const_iterator rf = region_flow.begin();
       rf != region_flow.end();
       ++rf, ++idx) {
    region_pts[idx] = cvCreateMatShared(2, rf->features.size(), CV_32F);
    float* x_ptr = RowPtr<float>(region_pts[idx], 0);
    float* y_ptr = RowPtr<float>(region_pts[idx], 1);
    for (FeatList::const_iterator pt = rf->features.begin();
         pt != rf->features.end();
         ++pt, ++x_ptr, ++y_ptr) {
      x_ptr[0] = pt->loc.x + pt->vec.x;
      y_ptr[0] = pt->loc.y + pt->vec.y;
    }
    
    region_lines[idx] = cvCreateMatShared(3, rf->features.size(), CV_32F);
  }
  
//  while (max_inliers < total_features * 2 / 3) {
  shared_ptr<CvMat> fundamentalM = cvCreateMatShared(3, 3, CV_32F);
  
    for (int iter = 0; iter < max_iterations; ++iter) {
      // Pick n = 3 regions randomly.
      const int region_sample_sz = 3;
      int region_sample[region_sample_sz];
      for (int r = 0; r < region_sample_sz; ++r) {
        int tries = 0;
        int test_r;
        
        do {
          test_r = rand() % region_flow.size();
          if (++tries > 1000) {
            QMessageBox::critical(0, "", "Frame could not be tracked. Using identity transform.");
            best_inlier_regions->clear();
            outlier_regions->clear();
            *model = SimilarityModel();
            return 0;
          }
          
        // Ignore duplicate or empty regions.
        } while (region_flow[test_r].features.size() == 0 ||
                 std::find(region_sample, region_sample + r, test_r) != region_sample + r);
        
        region_sample[r] = test_r;
      }
      
      vector<CvPoint2D32f> point_x;
      vector<CvPoint2D32f> point_y;
      
      // Convert into point clouds.
      for (int r = 0; r < region_sample_sz; ++r) {
        for (FeatList::const_iterator f = region_flow[region_sample[r]].features.begin();
             f != region_flow[region_sample[r]].features.end();
             ++f) {
          point_x.push_back(f->loc + f->vec);
          point_y.push_back(f->loc);
        }
      }
      
      // Construct matrix.
      shared_ptr<CvMat> points_1 = cvCreateMatShared(2, point_x.size(), CV_32F);
      shared_ptr<CvMat> points_2 = cvCreateMatShared(2, point_y.size(), CV_32F);
      
      // Small number of points, speed does not matter.
      for (int i = 0; i < point_x.size(); ++i) {
        cvSetReal2D(points_1.get(), 0, i, point_x[i].x);
        cvSetReal2D(points_1.get(), 1, i, point_x[i].y);
        cvSetReal2D(points_2.get(), 0, i, point_y[i].x);
        cvSetReal2D(points_2.get(), 1, i, point_y[i].y);
      }
      
      // Compute model.
      if (cvFindFundamentalMat(points_1.get(), points_2.get(), fundamentalM.get(),
                               CV_FM_8POINT) == 0)
        continue;
      
//      SimilarityModel local_model;
//      SimilarityFromPoints(point_x, point_y, &local_model);
      
      // Determine inliers.
      float inliers = 0;
      vector<int> inlier_regions;
      std::fill(inlier_score.begin(), inlier_score.end(), 0.f);
      
      int r_idx = 0;
      for (vector<RegionFlow>::const_iterator rf = region_flow.begin();
           rf != region_flow.end();
           ++rf, ++r_idx) {
        
        int conform_features = 0;
        
        // Is this an overlay?
        if (norm(rf->mean_vector) * inv_scale < 0.2f && av_motion > 1.f)
          continue;
        
        cvComputeCorrespondEpilines(region_pts[r_idx].get(),
                                    1, fundamentalM.get(), region_lines[r_idx].get());
        
        int f_idx = 0;
        float dist_sum = 0;
        for (FeatList::const_iterator f = rf->features.begin();
             f != rf->features.end();
             ++f, ++f_idx) {
          // Compute distance.
          float dist = cvGetReal2D(region_lines[r_idx].get(), 0, f_idx) * f->loc.x +
                       cvGetReal2D(region_lines[r_idx].get(), 1, f_idx) * f->loc.y +
                       cvGetReal2D(region_lines[r_idx].get(), 2, f_idx);
          dist = fabs(dist) * inv_scale;
          dist_sum += dist / (.3 * err_scale); // distance as ratio to error threshold.
          
          if (dist < .3 * err_scale)
            ++conform_features;
        }
        
        inlier_score[r_idx] = dist_sum / rf->features.size();
        if (conform_features > rf->features.size() * 2 / 3) {
          inliers += conform_features;
          inlier_regions.push_back(rf - region_flow.begin());
        }
      }
      
      if (inliers > max_inliers) {
        max_inliers = inliers;
        best_inliers.swap(inlier_regions);
        if (inlier_score_output)
          inlier_score_output->swap(inlier_score);
      }
    }
    
  if (max_inliers == 0) {
    QMessageBox::critical(0, "", "Estimating model failed. Not enough inliers found."
                          "Using similarity transform.");
  
    best_inlier_regions->clear();
    outlier_regions->clear();
    *model = SimilarityModel();
    return 0;
  }
  
  std::cout << "My total features: " <<  total_features << " inliers: " << max_inliers << "\n";
  
  // Compute model from best inlier regions.
  vector<CvPoint2D32f> point_x;
  vector<CvPoint2D32f> point_y;

  for (vector<int>::const_iterator r = best_inliers.begin(); r != best_inliers.end(); ++r) {
    for (FeatList::const_iterator f = region_flow[*r].features.begin();
         f != region_flow[*r].features.end();
         ++f) {
      point_x.push_back(f->loc + f->vec);
      point_y.push_back(f->loc);
    }
  }
  
  // Compute model.
  SimilarityFromPoints(point_x, point_y, model);
  
  // Set inlier and outlier regions.
  best_inlier_regions->swap(best_inliers);
  
  vector<int> all_tracked_regions(region_flow.size());
  for (int r = 0; r < region_flow.size(); ++r)
    all_tracked_regions[r] = r;
  
  outlier_regions->resize(region_flow.size() - best_inlier_regions->size());
  
  std::set_difference(all_tracked_regions.begin(), all_tracked_regions.end(), 
                      best_inlier_regions->begin(), best_inlier_regions->end(),
                      outlier_regions->begin());
  
  return point_x.size();
}

void ForegroundTracker::SimilarityFromPoints(const vector<CvPoint2D32f>& point_x,
                                             const vector<CvPoint2D32f>& point_y,
                                             SimilarityModel* model) {

  ASSURE_LOG(point_x.size() == point_y.size());
  
  // Compute mean and variance for each point cloud.
  CvPoint2D32f mean_x = {0, 0};
  CvPoint2D32f mean_y = {0, 0};
  
  float var_x = 0;
  float var_y = 0;
  
  
  for (vector<CvPoint2D32f>::const_iterator p = point_x.begin(); p != point_x.end(); ++p) {
    mean_x = mean_x + *p;
  }
  
  for (vector<CvPoint2D32f>::const_iterator p = point_y.begin(); p != point_y.end(); ++p) {
    mean_y = mean_y + *p;
  }
  
  mean_x = mean_x * (1.0f / point_x.size());
  mean_y = mean_y * (1.0f / point_y.size());
  
  // Saved in column major order.
  double H[4] = {0, 0, 0, 0};
  
  for (int p = 0; p < point_x.size(); ++p) {
    CvPoint2D32f diff_x = point_x[p] - mean_x;
    CvPoint2D32f diff_y = point_y[p] - mean_y;
    
    var_x += sq_norm(diff_x);
    var_y += sq_norm(diff_y);
    
    H[0] += diff_y.x * diff_x.x;
    H[1] += diff_y.y * diff_x.x;
    H[2] += diff_y.x * diff_x.y;
    H[3] += diff_y.y * diff_x.y;
  }
  
  var_x = var_x * (1.0f / point_x.size());
  var_y = var_y * (1.0f / point_y.size());
  
  for (int i = 0 ; i < 4; ++i)
    H[i] /= point_x.size();
  
  // Compute determinant for H.
  float detH = H[0] * H[3] - H[1] * H[2];
  
  // SVD of H.
  char c_a = 'A';
  int n_2 = 2;
  int lda = 2;
  
  double singular_values[2];
  double U[4];
  double Vt[4];
  
  double work[128];
  int lwork = 128;
  int info;
  
  dgesvd(&c_a,    // all columns of U
         &c_a,    // all rows of V
         &n_2,     // dim m
         &n_2,     // dim n
         H,
         &lda,
         singular_values,
         U,
         &lda,
         Vt,
         &lda,
         work,
         &lwork,
         &info);

  ASSURE_LOG(info == 0) << "SimilarityFromPoints: SVD execution failed.\n";
  
  // If det(H) < 0 multiply last column of U with -1.
  if (detH < 0) {
    U[2] *= -1.0;
    U[3] *= -1.0;
  }
  
  // Rotation matrix is U * Vt;
  // R is row major order.
  double R[4];
  R[0] = U[0] * Vt[0] + U[2] * Vt[1];
  R[1] = U[0] * Vt[2] + U[2] * Vt[3];
  R[2] = U[1] * Vt[0] + U[3] * Vt[1];
  R[3] = U[1] * Vt[2] + U[3] * Vt[3];
  
  // Extract angle from R. ( R * (1, 0)^T )
  float phi = atan2(R[2], R[0]);
  
  // Determine scale.
  float s = 1.0 / var_x * (singular_values[0] + singular_values[1] * (detH < 0 ? -1.0 : 1.0));
  
  CvPoint2D32f rot_mean_x = cvPoint2D32f(R[0] * mean_x.x + R[1] * mean_x.y,
                                         R[2] * mean_x.x + R[3] * mean_x.y);
  
  // Determine translation.
  CvPoint2D32f t = mean_y - rot_mean_x * s;
  
  model->s = s;
  model->phi = phi;
  model->t_x = t.x;
  model->t_y = t.y;
}

double ForegroundTracker::GetModelParam(int model_id, ModelParam param, const double* p) {
  if (model_id == 0) {
    switch (param) {
      case MODEL_PARAM_S:
        return 1.0;
      case MODEL_PARAM_PHI:
        return 0.0;
      case MODEL_PARAM_TX:
        return 0.0;
      case MODEL_PARAM_TY:
        return 0.0;
    }
  } else {
    const double* p_start = p + (model_id - 1) * 4;
    
    switch (param) {
      case MODEL_PARAM_S:
        return p_start[0];
      case MODEL_PARAM_PHI:
        return p_start[1];
      case MODEL_PARAM_TX:
        return p_start[2];
      case MODEL_PARAM_TY:
        return p_start[3];
    }    
  }
  
  std::cout << "ForegroundTracker::GetModelParam: Invalid Model param requested.\n";
  return 0;
}

void ForegroundTracker::lev_func(double* p, double* f_p, int m, int n, void* data) {
  ForegroundTracker::LevmarParams* params = (ForegroundTracker::LevmarParams*)data;
  vector< list<ModelMatch> >& model_matches = *params->matches;
  
  // Compute residual for each point.
  double* out = f_p;
  
  int m_idx = 0;
  for (vector< list<ModelMatch> >::const_iterator i = model_matches.begin();
       i != model_matches.end();
       ++i, ++m_idx) {
    for (list<ModelMatch>::const_iterator j = i->begin(); j != i->end(); ++j) {
      
      // Transform each pt. according to the transformation:
      // s_j * R_j * x + t_j = y_j
      // x = 1/s_j *  R_j^(-1) * (y_j - t_j)
      // ==> s_i * R_i * [ 1/s_j *  R_j^(-1) * (y_j - t_j) ] + t_i = y_i
      // s_i / s_j * R_i * R_j^(-1) * (y_j - t_j) + t_i = y_i
      // or in our (x,y) notation:
      // s_x / s_y * R_x * R_y^(-1) * (y - t_y) + t_x = x
      
      // Precompute some parts:
      double s_frac = GetModelParam(m_idx, MODEL_PARAM_S, p) / 
                          GetModelParam(j->match_id, MODEL_PARAM_S, p);
      double phi_diff = GetModelParam(m_idx, MODEL_PARAM_PHI, p) - 
                            GetModelParam(j->match_id, MODEL_PARAM_PHI, p);
      CvPoint2D32f t_x = cvPoint2D32f(GetModelParam(m_idx, MODEL_PARAM_TX, p),
                                      GetModelParam(m_idx, MODEL_PARAM_TY, p));
      CvPoint2D32f t_y =  cvPoint2D32f(GetModelParam(j->match_id, MODEL_PARAM_TX, p),
                                       GetModelParam(j->match_id, MODEL_PARAM_TY, p));
      
      for (vector<PointMatch>::const_iterator pt = j->pt_matches.begin();
           pt != j->pt_matches.end();
           ++pt, ++out) {
        CvPoint2D32f res = rotate(pt->second - t_y, phi_diff) * s_frac + t_x - pt->first;
        out[0] = sq_norm(res);
        if (out[0] > params->error_threshold)
          out[0] = params->error_threshold;

        out[0] *= pt->weight;
      }
    }
  }
  
  ASSERT_LOG(out - f_p == n) << "Missing parameters in lev_fun";
  /*
  std::copy(f_p, f_p + n, std::ostream_iterator<double>(std::cout, " "));
  int cont;
  std::cin >> cont;
  std::cout << '\n';
  */
}

void ForegroundTracker::lev_jac(double* p, double* j_p, int m, int n, void* data) {
  // j_p is column major w.r.t. to parameters.
  
  // m = number of parameters.
  // n = number of residuals.
  
  // General approach:
  // Iterate over each residual. 
  // For each residual, iterate over all models.
  // If the model does not influence the current residual -> set to zero.
  // Otherwise: --> set to Jacobian.
  
  ForegroundTracker::LevmarParams* params = (ForegroundTracker::LevmarParams*)data;
  vector< list<ModelMatch> >& model_matches = *params->matches;
  
  // Compute derivative for each point.
  double* out = j_p;
  
  // Initialize Jacobian to zero.
  memset(j_p, 0, sizeof(double) * m * n);
  
  // Tmp storage.
  CvPoint2D32f deriv[4];
  float matrix[4], c_phi, s_phi;
  
  // TODO: Support for external constraints.
  int m_idx = 0;
  for (vector< list<ModelMatch> >::const_iterator i = model_matches.begin();
       i != model_matches.end();
       ++i, ++m_idx) {
    for (list<ModelMatch>::const_iterator j = i->begin(); j != i->end(); ++j) {
      // Only touch parts for m_idx and j->match_id.
      
      // Derivative of residual w.r.t. to transformation parameters:
      // || s_x / s_y * R_x * R_y^(-1) * (y - t_y) + t_x - x ||^2
      
      // Chain rule: 
      //  2 * r^T * inner derivative
      // inner derivative is a 2 x #parameter matrix.
      
      // Derivative of R(a) w.r.t. a: DR(a): [-s(a) -c(a); c(a) -s(a)];
      
      // inner derivative for x:  [= m_idx]
      // s_x : 1.0 / s_y * R_x * R_y^T * (y - t_y)
      // phi_x : s_x / s_y * DR_x * R_y^T * (y - t_y)
      // t_x_1: [1 0]
      // t_x_2: [0 1]
      
      // inner derivative for y:  [ = j->match_id]
      // s_y : -s_x / s_y^2 * R_x * R_y^T * (y - t_y)
      // phi_y: s_x / s_y * R_x * DR_y^T * (y - t_y)
      // t_y_1:  s_x / s_y * R_x * R_y^T * [-1 0];
      // t_y_2:  s_x / s_y * R_x * R_y^T * [0 -1];
      
      // Precompute some parts:
      double s_frac = GetModelParam(m_idx, MODEL_PARAM_S, p) / 
                        GetModelParam(j->match_id, MODEL_PARAM_S, p);
      double phi_diff = GetModelParam(m_idx, MODEL_PARAM_PHI, p) - 
                          GetModelParam(j->match_id, MODEL_PARAM_PHI, p);
      
      CvPoint2D32f t_x = cvPoint2D32f(GetModelParam(m_idx, MODEL_PARAM_TX, p),
                                      GetModelParam(m_idx, MODEL_PARAM_TY, p));
      CvPoint2D32f t_y = cvPoint2D32f(GetModelParam(j->match_id, MODEL_PARAM_TX, p),
                                      GetModelParam(j->match_id, MODEL_PARAM_TY, p));
      
      // One line of the jacobian at a time.
      for (vector<PointMatch>::const_iterator pt = j->pt_matches.begin();
           pt != j->pt_matches.end();
           ++pt, out += m) {
        
        CvPoint2D32f res = rotate(pt->second - t_y, phi_diff) * s_frac + t_x - pt->first;

        // Derivative for all residuals above error_threshold is zero.
        if (sq_norm(res) > params->error_threshold)
          continue;
        
        // Derivative w.r.t. x:
        if (m_idx) {
          deriv[0] = rotate(pt->second - t_y, phi_diff) * 
                         (1.0 / GetModelParam(j->match_id, MODEL_PARAM_S, p));
          
          deriv[1] = rotate(pt->second - t_y, -GetModelParam(j->match_id, MODEL_PARAM_PHI, p));
          
          c_phi = cos(GetModelParam(m_idx, MODEL_PARAM_PHI, p));
          s_phi = sin(GetModelParam(m_idx, MODEL_PARAM_PHI, p));
          matrix[0] = -s_phi;
          matrix[1] = -c_phi;
          matrix[2] = c_phi;
          matrix[3] = -s_phi;
          
          deriv[1] = transform(deriv[1], matrix) * s_frac;
          deriv[2] = cvPoint2D32f(1.0, 0.0);
          deriv[3] = cvPoint2D32f(0.0, 1.0);
          
          // Linear combine.
          double* x_deriv = out + (m_idx - 1) * 4;
          x_deriv[0] = 2.0 * dot(res, deriv[0]) * pt->weight;
          x_deriv[1] = 2.0 * dot(res, deriv[1]) * pt->weight;
          x_deriv[2] = 2.0 * dot(res, deriv[2]) * pt->weight;
          x_deriv[3] = 2.0 * dot(res, deriv[3]) * pt->weight;
        }
        
        if (j->match_id) {
          deriv[0] = rotate(pt->second - t_y, phi_diff) * 
                         (-s_frac / GetModelParam(j->match_id, MODEL_PARAM_S, p));

          c_phi = cos(GetModelParam(j->match_id, MODEL_PARAM_PHI, p));
          s_phi = sin(GetModelParam(j->match_id, MODEL_PARAM_PHI, p));
          matrix[0] = -s_phi;
          matrix[1] = c_phi;
          matrix[2] = -c_phi;
          matrix[3] = -s_phi;
          
          deriv[1] = rotate(transform(pt->second - t_y, matrix),
                            GetModelParam(m_idx, MODEL_PARAM_PHI, p)) * s_frac;
          deriv[2] = rotate(cvPoint2D32f(-1.0, 0.0), phi_diff) * s_frac;
          deriv[3] = rotate(cvPoint2D32f(0.0, -1.0), phi_diff) * s_frac;
          
          // Linear combine.
          double* y_deriv = out + (j->match_id - 1) * 4;
          y_deriv[0] = 2.0 * dot(res, deriv[0]) * pt->weight;
          y_deriv[1] = 2.0 * dot(res, deriv[1]) * pt->weight;
          y_deriv[2] = 2.0 * dot(res, deriv[2]) * pt->weight;
          y_deriv[3] = 2.0 * dot(res, deriv[3]) * pt->weight;
        }
      }
    }
  }

  /*
  for (int i = 0; i < n; ++i) {
    std::copy(j_p + i * m, j_p + (i+1) * m, std::ostream_iterator<double>(std::cout, " "));
    std::cout << "\n";
    int cont;
    std::cin >> cont;
  }
  */
  ASSERT_LOG( out - m * n == j_p) << "Missing parameters in lev_jac";
}

void ForegroundTracker::BundleAdjustSimilarity(const list<FrameMatchList>& frame_matches,
                                               vector<SimilarityModel>* computed_models) {
  
  int num_models = frame_matches.size();
  // First compile list of pt. matches.
  
  vector< list<ModelMatch> > model_matches(num_models);
  vector<SimilarityModel> models(num_models);
  
  int num_residuals = 0;
  
  // TODO: external_model support.
  int idx = 0;
  for (list<FrameMatchList>::const_iterator m = frame_matches.begin();
       m != frame_matches.end();
       ++m, ++idx) {

    // Use concatenated models (w.r.t. previous frame) as initialization.
    if (idx > 0) 
      models[idx] = m->front().model.Concat(models[idx - 1]);
  
    
    for (FrameMatchList::const_iterator n = m->begin(); n != m->end(); ++n) {
      
      int matched_idx = n->match_id;
      // Add RegionFlow matches for each inlier region.
      ModelMatch model_match;
      model_match.match_id = matched_idx;
      
      ASSERT_LOG(matched_idx >= 0);
      
      ModelMatch model_match_inv;
      model_match_inv.match_id = idx;
      
      // Add pt matches for each inlier region.
      
      for (vector<int>::const_iterator inlier = n->inlier_regions.begin();
           inlier != n->inlier_regions.end();
           ++inlier) {
      /*
        model_match.pt_matches.reserve(model_match.pt_matches.size() +
                                       n->flow[*inlier].features.size() * 5);
      //  model_match_inv.pt_matches.reserve(model_match_inv.pt_matches.size() +
        //                                   n->flow[*inlier].features.size() * 5);
        
        for (FeatList::const_iterator f = n->flow[*inlier].features.begin();
             f != n->flow[*inlier].features.end();
             ++f) {
          model_match.pt_matches.push_back(PointMatch(f->loc, f->loc + f->vec, 1.0));
//          model_match_inv.pt_matches.push_back(PointMatch(f->loc + f->vec, f->loc));
          num_residuals++;
        }
      */
        
        CvPoint2D32f min_loc = n->flow[*inlier].min_match.first;
        CvPoint2D32f min_vec = n->flow[*inlier].min_match.second;

        CvPoint2D32f max_loc = n->flow[*inlier].max_match.first;
        CvPoint2D32f max_vec = n->flow[*inlier].max_match.second;

        CvPoint2D32f mean_loc = n->flow[*inlier].anchor_pt;
        CvPoint2D32f mean_vec = n->flow[*inlier].mean_vector;
        float weight = 1.0; // (float)n->flow[*inlier].features.size() / n->num_features;
        
        model_match.pt_matches.push_back(PointMatch(mean_loc, mean_loc + mean_vec, weight));
        
        model_match.pt_matches.push_back(PointMatch(min_loc, min_loc + min_vec, weight));
//        model_match_inv.pt_matches.push_back(PointMatch(min_loc + min_vec, min_loc, weight));
        
        model_match.pt_matches.push_back(PointMatch(max_loc, max_loc + max_vec, weight));
//        model_match_inv.pt_matches.push_back(PointMatch(max_loc + max_vec, max_loc, weight));
        
       num_residuals += 3;
      }
      
      // Add to corresponding matches.
      model_matches[idx].push_back(model_match);
  //    model_matches[matched_idx].push_back(model_match_inv);
    }
  }
  
  // Setup parameter vector. First transformation is assumed to be identity and not optimized.
  vector<double> p((num_models - 1) * 4);
  
  vector<double> cov(p.size() * p.size(), 0);
  double* out_cov = &cov[0];

  double e_x[4] = { 0, 0, 0, 0 };
  double e_x2[4] = { 0, 0, 0, 0 };
  for (int idx = 0; idx < num_models; ++idx) {
    e_x[0] += models[idx].s;
    e_x[1] += models[idx].phi;
    e_x[2] += models[idx].t_x;
    e_x[3] += models[idx].t_y;
    
    e_x2[0] += models[idx].s * models[idx].s;
    e_x2[1] += models[idx].phi * models[idx].phi;
    e_x2[2] += models[idx].t_x * models[idx].t_x;
    e_x2[3] += models[idx].t_y * models[idx].t_y;
  }
  
  double sigmas[4] = {(e_x[0] * e_x[0] - e_x2[0]) / num_models,
                      (e_x[1] * e_x[1] - e_x2[1]) / num_models,
                      (e_x[2] * e_x[2] - e_x2[2]) / num_models,
                      (e_x[3] * e_x[3] - e_x2[3]) / num_models };
  
  for (int i = 0, idx = 1; idx < num_models; i+=4, ++idx) {
    p[i] = models[idx].s;
    p[i + 1] = models[idx].phi;
    p[i + 2] = models[idx].t_x;
    p[i + 3] = models[idx].t_y;
    
    out_cov[0] = sigmas[0];
    out_cov += p.size() + 1;
    out_cov[0] = sigmas[1];
    out_cov += p.size() + 1;
    out_cov[0] = sigmas[2];
    out_cov += p.size() + 1;
    out_cov[0] = sigmas[3];
    out_cov += p.size() + 1;
  }
  
  double info[LM_INFO_SZ];
  float scale = 1.0f; // / sqrt(frame_width_ * frame_width_ + frame_height_ * frame_height_) * 2.0;
  LevmarParams params = {&model_matches, 2500 * scale};    // 50^2 as initial max error.
  
  // Run levmar to determine minimum.
  // Note:
  // The values we try to achieve after transformation are zero residuals.
  // We try to achieve zero residuals instead of point_y
  // as we will incorporate robustified residuals in lev_func.
  
  vector<double> y(num_residuals, 0);
  double opts[LM_OPTS_SZ] = { LM_INIT_MU, 1e-15, 1e-15, 1e-3,
                              LM_DIFF_DELTA };
  
//#define LEVMAR_BUNDLE_ADJUST      // comment line to deactivate bundle adjusment
#ifdef LEVMAR_BUNDLE_ADJUST  
  while (params.error_threshold > 100 * scale) {      // 5 iterations altogether.
    
    std::cout << "Levmar optimization with threshold: " << params.error_threshold << "...\n";
    dlevmar_der(&lev_func,
                &lev_jac,
                &p[0],
                &y[0],           // zero rhs.
                p.size(),
                num_residuals,
                200,
                NULL,
                info,
                NULL,         // work.get(),
                &cov[0],          // 
                (void*)&params);

    std::cout << "Initial error: " << info[0] 
              << " = " << sqrt(info[0]) / num_residuals << " pixels per sample.\n";
    std::cout << " Error now: " << info[1]
              << " = " << sqrt(info[1]) / num_residuals << " pixels per sample.\n";
    std::cout << "\nStopped after " << info[5] << " iterations. Reason: ";
    
    switch ((int)info[6]) {
      case 1:
        std::cout << "Small gradient.\n";
        break;
      case 2:
        std::cout << "Small difference in params.\n";
        break;
      case 3:
        std::cout << "Maximum iterations.\n";
        break;
      case 4:
        std::cout << "Singular matrix.\n";
        break;
      case 5:
        std::cout << "No error reduction possible.\n";
        break;
      case 6:
        std::cout << "Error is small. GREAT!\n";
        break;
      case 7:
        std::cout << "INVALID OPERATION occured.\n";
        break;
      default:
        break;
    }
    
    params.error_threshold /= 4;
  }
  
#endif // LEVMAR_BUNDLE_ADJUST
  
  // Get similarity transforms from p.
  (*computed_models)[0] = SimilarityModel();      // identity.
  double* src_p = &p[0];
  for (int i = 1; i < num_models; ++i, src_p += 4) {
    (*computed_models)[i] = SimilarityModel (src_p[0], src_p[1], src_p[2], src_p[3]);
  }
}

void ForegroundTracker::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  qApp->processEvents();
  
  // Get frame for each stream.
  const VideoFrame* video_frame = dynamic_cast<const VideoFrame*>(input->at(vid_stream_idx_).get());
  
  const RegionFlowFrame* flow_frame = 
      dynamic_cast<const RegionFlowFrame*>(input->at(flow_stream_idx_).get());
  
  frame_match_list_.push_back(FrameMatchList());
  
  float scale = 1.0f / (sqrt(frame_width_ * frame_width_ + frame_height_ * frame_height_) / 2);
  CvPoint2D32f trans = cvPoint2D32f(-frame_width_ / 2, -frame_height_ / 2);
  
  // Compute the similarity models for each frame and ALL of its matches.
  // vector holds inlier score for each region w.r.t. prev. frame.
  ComputeMatchedModels(flow_frame, trans, scale, &frame_match_list_.back());
  
  // Compare to area ratio.
  VideoFramework::ValueFrame<bool>* key_frame = new VideoFramework::ValueFrame<bool>();
  key_frame->set_value(false);

  // Evaluate if we reached a bundle adjustment point.
  bool ba_point_reached = false;
  int diff_frames = frame_number_ - last_ba_frame_; 
  if (flow_frame->IsKeyFrame())
    ba_point_reached = true;

 // if (diff_frames > 0) {
    //   if (diff_frames % ba_max_frames_ == 0) {
    //   ba_point_reached = true;
    //} else if (flow_frame->IsKeyFrame())
   // if (flow_frame->IsKeyFrame())
     // ba_point_reached = true;
//  }  
  
  if (frame_match_list_.back().size() > 1) {
    SimilarityModel last_model = frame_match_list_.back().back().model;
    SimilarityModel first_model = frame_match_list_.back().front().model;
    concat_model_ = first_model.Concat(concat_model_);
    
    SimilarityModel diff_model_keyframe = last_model;
    
    QPolygon frame_poly(QRect(0, 0, frame_width_, frame_height_), true);  // closed_poly
    QPolygon concat_poly = concat_model_.ToQTransform().map(frame_poly);
    QPolygon diff_poly = diff_model_keyframe.ToQTransform().map(frame_poly);
    
    QPolygon frame_inter = concat_poly.intersected(frame_poly);
    QPolygon model_inter = concat_poly.intersected(diff_poly);
    
    // Get area ratio
    const float area = frame_width_ * frame_height_;
    float frame_area_ratio = PolyArea(frame_inter, frame_height_) / area;
    float frame_area_error = PolyArea(model_inter, frame_height_) 
                             / PolyArea(concat_poly, frame_height_);
    
    // Too much deviation from frame-based tracking?
    // Erase key-frame constraint.
    if (frame_area_error < 0.95) {  
      frame_match_list_.back().pop_back();
      std::cout << "Erasing key-frame constraint!\n";
    }
    
    std::cout << "Frame: " << frame_number_ << " area ratio from key-frame " 
              << frame_area_ratio << " propagation error: " << frame_area_error << "\n";
    
    if (frame_area_ratio < ba_frame_area_ratio_ && !ba_point_reached) {
      // Signal that next frame should be a key_frame.
      key_frame->set_value(true);
    }
  }
  
  input->push_back(shared_ptr<ValueFrame<bool> >(key_frame));
  
  // First entry in frame_match_list_.back() is track to prev. frame.
  ValueFrame< vector<int> >* inlier_frame;
  if (frame_match_list_.back().size() > 0)
    inlier_frame = new ValueFrame< vector<int> >(frame_match_list_.back().front().inlier_regions);
  else 
    inlier_frame = new ValueFrame< vector<int> >();

  input->push_back(shared_ptr<ValueFrame<vector<int> > >(inlier_frame));
  
  // Add first model to tracked regions.
  const DataFrame* seg_frame = input->at(seg_stream_idx_).get();
  boost::shared_ptr<Segment::SegmentationDesc> seg_desc (new Segment::SegmentationDesc);
  seg_desc->ParseFromArray(seg_frame->get_data(), seg_frame->get_size());
 
  int num_regions = seg_desc->region_size();
  int max_id = seg_desc->max_id();

  ASSURE_LOG(num_regions == max_id) << "Region segmentation inconsistent."
                                        << " Regions " << num_regions 
                                        << " max ids " << max_id << "\n";
  
  /*
  
  // Initialize from outlier information.
  vector<RegionProb> region_probs(num_regions);
  vector<RegionType> region_type_list(num_regions, REGION_TYPE_UNTRACKED);
  vector<float> inlier_score(num_regions, 0.5f);
  vector<CvPoint2D32f> flow(num_regions, cvPoint2D32f(0.f, 0.f));
  
  if (frame_match_list_.back().size() > 0) {
    const FrameMatch& fm = frame_match_list_.back().front();
    for (vector<int>::const_iterator i = fm.outlier_regions.begin();
         i != fm.outlier_regions.end(); 
         ++i)
      region_type_list[fm.flow[*i].region_id] = REGION_TYPE_OUTLIER;
    
    for (vector<int>::const_iterator i = fm.inlier_regions.begin();
         i != fm.inlier_regions.end(); 
         ++i)
      region_type_list[fm.flow[*i].region_id] = REGION_TYPE_INLIER;
    
    
    for (int j = 0; j <  fm.inlier_score->size(); ++j) {
      inlier_score[fm.flow[j].region_id] = fm.inlier_score->at(j);
      flow[fm.flow[j].region_id] = fm.flow[j].mean_vector;
    }
  }
  
  
  for (int i = 0; i < num_regions; ++i) {
    const int region_id = seg_desc->region(i).id();
    region_probs[i].region_id = region_id;
    region_probs[i].region_type = region_type_list[region_id];
    region_probs[i].epipolar_dist = inlier_score[region_id];
    ASSURE_LOG (region_id == i);
  }
  
  // Build histogram for each region.
  Segment::SegmentationDescToIdImage((int*)id_img_->imageData, id_img_->widthStep, frame_width_, 
                                     frame_height_, 0, *seg_desc);
  
  IplImage vid_image;
  video_frame->ImageView(&vid_image);
  shared_ptr<IplImage> lab_img = cvCreateImageShared(frame_width_, frame_height_,
                                                     IPL_DEPTH_8U, 3);
  cvCvtColor(&vid_image, lab_img.get(), CV_RGB2Lab);
  
  
  // Collect histograms.
  for (int i = 0; i < frame_height_; ++i) {
    const unsigned char* img_ptr = RowPtr<unsigned char>(lab_img, i);
    int* id_ptr = RowPtr<int>(id_img_, i);
    
    for (int j = 0; j < frame_width_; ++j, ++id_ptr, img_ptr += 3) {
      region_probs[*id_ptr].hist.AddPixelInterpolated(img_ptr);
    }
  }
  
  // Convert to sparse.
  for (vector<RegionProb>::iterator i = region_probs.begin(); i != region_probs.end(); ++i) {
    i->hist.ConvertToSparse();
  }
  
  // Pre-construct nodes for GC optimization in PostProcess.
  PreConstructGCGraph(region_probs,
                      *seg_desc,
                      prev_probs_,
                      prev_seg_desc_.get());
  
  // Do we reached bundle adjustment pt?
  if (ba_point_reached) {
    // First model from key-frame is identity.
    vector<SimilarityModel> models(diff_frames + 1);
    
    QTime t;
    t.start();
    
    frame_match_list_.back().resize(1);   // TODO: REMOVE!
    BundleAdjustSimilarity(frame_match_list_, &models);
    
    std::cout << "Time elapsed: " << t.elapsed() / 1000.0 << "s\n";
    
    // Concat models with previous bundle adjust pt and output.
    for (vector<SimilarityModel>::iterator sm = models.begin(); sm != models.end(); ++sm) {
      *sm = sm->Concat(prev_ba_model_);
    }
    
    
     // Adjust model based on feature normalization.
 //    for (vector<SimilarityModel>::iterator sm = models.begin(); sm != models.end(); ++sm) {
 //    CvPoint2D32f trans_rot = rotate(trans, sm->phi);
 //    CvPoint2D32f trans_t = trans_rot * sm->s - trans +
 //    cvPoint2D32f(sm->t_x, sm->t_y) * (1.0f / scale);
 //    sm->t_x = trans_t.x;
 //    sm->t_y = trans_t.y;
 //    }
     
    
    // Skip the first model in each batch except it is the very first.
    for (vector<SimilarityModel>::iterator sm = models.begin() + 
         ((last_ba_frame_ != 0) ? 1 : 0);
         sm != models.end(); ++sm) {
      models_to_output_.push_back(*sm);
    }     
  
    prev_ba_model_ = models.back();
    concat_model_ = SimilarityModel();
    frame_match_list_.clear();
    last_ba_frame_ = frame_number_;
    frame_match_list_.push_back(FrameMatchList());
  }
  
  prev_seg_desc_.swap(seg_desc);
  prev_probs_.swap(region_probs);
  prev_id_img_.swap(id_img_);
 */
  
  /*
  
  // For each Region in Segmentation compute probablity of belonging to foreground.
  const int num_regions = level == 0 ? desc->region_size() : desc->hierarchy(level-1).region_size();
  vector<RegionProb> region_probs(num_regions);
  hash_map<int, int> region_map;
  
  for (int i = 0; i < num_regions; ++i) {
    region_probs[i].region_id = level == 0 ? desc->region(i).id() 
                                           : desc->hierarchy(level-1).region(i).id();
    region_probs[i].probability = 0.f;
    region_map[region_probs[i].region_id] = i;
  }
  */
  
  /*
  
  // Initialize region_probs based on previous region_probs.
  
  // Build histogram for each region.
  Segment::SegmentationDescToIdImage((int*)id_img_->imageData, id_img_->widthStep, frame_width_, 
                                     frame_height_, level, *desc);
  
  IplImage vid_image;
  video_frame->ImageView(&vid_image);
  
  // Collect histograms.
  for (int i = 0; i < frame_height_; ++i) {
    const unsigned char* img_ptr = RowPtr<unsigned char>(&vid_image, i);
    int* id_ptr = RowPtr<int>(id_img_, i);
    
    for (int j = 0; j < frame_width_; ++j, ++id_ptr, img_ptr += 3) {
      region_probs[region_map[*id_ptr]].hist.AddPixel(img_ptr);
    }
  }
  
  // Convert to sparse.
  for (vector<RegionProb>::iterator i = region_probs.begin(); i != region_probs.end(); ++i) {
    i->hist.ConvertToSparse();
  }
  
  
  if (frame_number_ > 0) {
    PropagateForeground(prev_id_img_.get(),
                        prev_probs_,
                        prev_region_map_,
                        *old_segmentation_,
                        region_flow,
                        *desc,
                        region_map,
                        &region_probs);
  }
  
  // Mark all significant outliers as clearly foregorund.
  // Determine inlier set.
  for (vector<RegionFlow>::const_iterator rf = region_flow.begin();
       rf != region_flow.end();
       ++rf) {
    // if (!Segment::VecInCylinder(model, rf->mean_vector, 0.5, 3*3.84)) {
    CvPoint2D32f diff = cvPoint2D32f(model.x - rf->mean_vector.x, model.y - rf->mean_vector.y);
    const float dist = diff.x * diff.x + diff.y * diff.y;
    int idx = region_map[rf->region_id];

    if (dist > 16*3.84) {   // outlier.
      region_probs[idx].probability = std::min(1.f, region_probs[idx].probability + .3f);
    }
    
    if (dist < 2*3.84) { // inlier.
    //  region_probs[idx].probability = std::max(0.f, region_probs[idx].probability - .2f);      
    }
  }
   
   */
    
  // Track previous frames based on color.
//  if (frame_number_ != 0)
//    ofs_feat_ << "\n";
  
  /*
  
  // Write to file.
  ofs_feat_ << "Frame# " << frame_number_ << " " << model.t_x << " " << model.t_y 
                                          << " " << model.s   << " " << model.phi << "\n";
  ofs_feat_ << "Level# " << level << "\n";
  ofs_feat_ << "ForegroundProbs:";
  
  for (vector<RegionProb>::const_iterator i = region_probs.begin();
       i != region_probs.end(); 
       ++i) {
    int val = std::max(0, std::min(255, static_cast<int>(i->probability * 255)));
    ofs_feat_ << " " << i->region_id << " " << val;
  }
    
  prev_probs_.swap(region_probs);
  old_segmentation_.swap(desc);
  prev_region_map_.swap(region_map);
  prev_id_img_.swap(id_img_);
  prev_level_ = cur_level_;
  
   */
   
  output->push_back(input);
  ++frame_number_;
}

void ForegroundTracker::PreConstructGCGraph(const vector<RegionProb>& region_probs,
                                            const Segment::SegmentationDesc& cur_seg,
                                            const vector<RegionProb>& prev_probs,
                                            const Segment::SegmentationDesc* prev_seg) {
  const int TRUE_POSITIVE_WEIGHT = 20;
  const int TRUE_NEGATIVE_WEIGHT = 20;
  const int FALSE_POSITIVE_WEIGHT = 600; // 200 for synthetic
  const int FALSE_NEGATIVE_WEIGHT = 8000;
  const float WEIGHT_SCALE = 400; // 800 for synthetic
  const float hist_sigma = 0.2; 
  const float hist_mean = 0.3;
  
  int id_shift = neighbor_indexes_.size();
  frame_id_shift_.push_back(id_shift);
  
  /*
  std::cout << "Star similarities\n";
  float t = region_probs[54].hist.L2Dist(region_probs[23].hist);
  float dist = std::max(0.f, t - hist_mean);
  float weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
                 exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  
  std::cout << weight << "\n";
  
  t = region_probs[54].hist.L2Dist(region_probs[24].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[49].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[6].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[14].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[43].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[5].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[7].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  t = region_probs[54].hist.L2Dist(region_probs[8].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";
  
  t = region_probs[54].hist.L2Dist(region_probs[9].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";
  
  t = region_probs[54].hist.L2Dist(region_probs[69].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";
  
  t = region_probs[54].hist.L2Dist(region_probs[13].hist, rad);
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";

  
  t = 0.75;
  dist = std::max(0.f, t - hist_mean);
  weight = 1.0 / sqrt(2 * M_PI) / hist_sigma * 
  exp(-.5 * dist * dist / (hist_sigma * hist_sigma)) * WEIGHT_SCALE;
  std::cout << weight << "\n";
*/
  /*
  int region_ids[13] = {6, 23, 54, 49, 24, 14, 43, 5, 7, 8, 9, 13, 69};
  for (int k = 0; k < 13; ++k) {
    float params[4*3];
    region_probs[region_ids[k]].hist.GMM(3, params);
    std::cout << "Region: " << region_ids[k];
    for (int l = 0; l < 12; ++l)
      std::cout << " " << params[l];
    std::cout << "\n";
  }
  */
  
  for (vector<RegionProb>::const_iterator r = region_probs.begin();
       r != region_probs.end();
       ++r) {
    
    float epipolar_weight = 1.0f; //r->epipolar_dist
    // DataCost.
    switch (r->region_type) {
      case REGION_TYPE_INLIER:
        data_costs_.push_back(std::make_pair(TRUE_POSITIVE_WEIGHT, 
                                             FALSE_POSITIVE_WEIGHT * epipolar_weight));
        break;
        
      case REGION_TYPE_OUTLIER:
        data_costs_.push_back(std::make_pair(FALSE_NEGATIVE_WEIGHT * epipolar_weight,
                                             TRUE_NEGATIVE_WEIGHT));
        break;
      case REGION_TYPE_UNTRACKED:
        data_costs_.push_back(std::make_pair(100, 20));
        break;
    }    
    
    vector<int> neighbor_indexes;
    vector<int> neighbor_weights;
    
    // For each neighbor compute weights.
    // First in space.
    /*
    for (int j = 0; j < cur_seg.region(r->region_id).neighbor_id_size(); ++j) {
      int n_id = cur_seg.region(r->region_id).neighbor_id(j);
      neighbor_indexes.push_back(n_id + id_shift);
      // Cut along high difference.
      float dist = std::max(0.f, r->hist.L2Dist(region_probs[n_id].hist) - hist_mean);
      float weight = 1.0 / sqrt(2. * M_PI) / hist_sigma * 
                     exp(-.5 * dist * dist / (hist_sigma * hist_sigma));
      
  //    std::cout << weight << "\n";
      neighbor_weights.push_back(weight * WEIGHT_SCALE);
    }
     */
    
    // In time if frame is tracked.
    // Obtain overlapping regions in prev. frame.
    if (prev_probs.size()) {
      typedef Segment::SegmentationDesc::Region2D::Scanline Scanline;
      typedef Segment::SegmentationDesc::Region2D::Scanline::Interval ScanlineInterval;
      const RepeatedPtrField<Scanline>& scanlines = cur_seg.region(r->region_id).scanline();
      const int top_y = cur_seg.region(r->region_id).top_y();
      
      // Region_id, Intersection size tuple.
      hash_map<int, int> prev_regions;
      
      for(RepeatedPtrField<Scanline>::const_iterator s = scanlines.begin();
          s != scanlines.end();
          ++s) {
        // Moved scanline still within image bounds?
        int s_idx = top_y + (r->flow.y + 0.5) + s - scanlines.begin();
        if (s_idx >= 0 && s_idx < frame_height_) {
          const int* id_ptr = RowPtr<int>(prev_id_img_, s_idx);
          for (int i = 0, sz = s->interval_size(); i < sz; ++i) {
            const ScanlineInterval& inter = s->interval(i);
            const int* id_ptr_local = id_ptr + inter.left_x() + (int)(r->flow.x + 0.5);
            
            for (int j = 0, len = inter.right_x() - inter.left_x() + 1;
                 j < len;
                 ++j, ++id_ptr_local) {
              int j_pos = inter.left_x() + (r->flow.x + 0.5) + j;
              
              if (j_pos >= 0 && j_pos < frame_width_) 
                ++prev_regions[*id_ptr_local];
            }
          }
        }
      }
      
      vector<int> neighbors_in_time;
      
      const int prev_id_shift = frame_id_shift_[frame_id_shift_.size() - 2];
      for (hash_map<int, int>::const_iterator pi = prev_regions.begin();
           pi != prev_regions.end(); 
           ++pi) {
        
        int n_id = pi->first;
        neighbors_in_time.push_back(n_id);
        // Add neighbors of n_id as well.
      //  for (int i = 0; i < prev_seg->region(n_id).neighbor_id_size(); ++i)
       //   neighbors_in_time.push_back(prev_seg->region(n_id).neighbor_id(i));
        
      }
      
      std::sort(neighbors_in_time.begin(), neighbors_in_time.end());
      std::vector<int>::const_iterator last_neighbor = std::unique(neighbors_in_time.begin(),
                                                                   neighbors_in_time.end());
      
      for (vector<int>::const_iterator n_id = neighbors_in_time.begin();
           n_id != last_neighbor; 
           ++n_id) {
        
        neighbor_indexes.push_back(*n_id + prev_id_shift);
        // Cut along high difference.
        float dist = std::max(0.f, r->hist.L2Dist(prev_probs[*n_id].hist) - hist_mean);
        float weight = 1.0 / sqrt(2. * M_PI) / hist_sigma * 
                       exp(-.5 * dist * dist / (hist_sigma * hist_sigma));
        
        neighbor_weights.push_back(weight * WEIGHT_SCALE);
      }
    }
    
    neighbor_indexes_.push_back(neighbor_indexes);
    neighbor_weights_.push_back(neighbor_weights);
  }
}

void ForegroundTracker::ComputeMatchedModels(const RegionFlowFrame* flow_frame,
                                             const CvPoint2D32f& trans,
                                             float scale,
                                             FrameMatchList* matches) {
  for (int frame_match_i = 0; frame_match_i < flow_frame->MatchedFrames(); ++frame_match_i) {
    vector<RegionFlow> region_flow;
    flow_frame->FillVector(&region_flow, frame_match_i);
    
    // Output number of regions and the sum of all features.
    std::cout << "Frame: " << frame_number_ << " match: " << frame_match_i << "\n";
    int num_feats = 0;
    for (vector<RegionFlow>::const_iterator rf = region_flow.begin();
         rf != region_flow.end();
         ++rf)
      num_feats += rf->features.size();
    std::cout << "Tracked regions: " << region_flow.size() << " Features: " << num_feats << "\n";
    
    // Last matched frame or not enough tracked regions?
    if (region_flow.size() < 3) {
      std::cerr << "----\n> FrameMatching aborted because number of tracked regions is low!\n";
      break;
    }
    
    // Dense tracking tracks more images than necessary for model estimation.
    if (!use_key_frame_tracking_) {
      int max_matches = frame_number_ - last_ba_frame_;
      if(frame_match_i >= max_matches)
        break;
    }
    
    // Normalize and center features.
    // We use Qt for visualization which has the same LHS coordinate system as the
    // feature locations.
    vector<RegionFlow> region_flow_transformed(region_flow);
    
    for (vector<RegionFlow>::iterator rf = region_flow_transformed.begin(); 
         rf != region_flow_transformed.end();
         ++rf) {
      rf->anchor_pt = (rf->anchor_pt + trans) * scale;
      rf->min_match.first = (rf->min_match.first + trans) * scale;
      rf->max_match.first = (rf->max_match.first + trans) * scale;
      
      rf->mean_vector = rf->mean_vector * scale;
      rf->min_match.second = rf->min_match.second * scale;
      rf->max_match.second = rf->max_match.second * scale;
      
      for (FeatList::iterator f = rf->features.begin(); f !=  rf->features.end(); ++f) {        
        f->loc = (f->loc + trans) * scale;
        f->vec = (f->vec) * scale;
      }
    }
    
    vector<int> best_inlier_regions;
    vector<int> outlier_regions;
    SimilarityModel model;
    int used_features_ = 0;
    shared_ptr< vector<float> > inlier_score;
    if (frame_match_i == 0)
      inlier_score.reset(new vector<float>);
    
    if (region_flow.size() > 0)
      used_features_ = EstimateModel(region_flow_transformed, 
                                     flow_frame->NumberOfFeatures(frame_match_i),
                                     scale,
                                     abs(flow_frame->FrameID(frame_match_i)),
                                     &best_inlier_regions,
                                     &outlier_regions,
                                     &model,
                                     inlier_score.get());
    
    // Adjust model based on feature normalization.
    CvPoint2D32f trans_rot = rotate(trans, model.phi);
    CvPoint2D32f trans_t = trans_rot * model.s - trans +
    cvPoint2D32f(model.t_x, model.t_y) * (1.0f / scale);
    model.t_x = trans_t.x;
    model.t_y = trans_t.y;
    
    FrameMatch fm = {model,
                     frame_match_list_.size() - 1 + flow_frame->FrameID(frame_match_i),
                     region_flow, 
                     best_inlier_regions,
                     outlier_regions,
                     inlier_score,
                     used_features_,
                     0,
                     0 };
    //level, num_regions};
    
    ASSERT_LOG(fm.match_id >= 0);
    
    frame_match_list_.back().push_back(fm);
  }  
  
}

typedef Segment::SegmentationDesc::Region2D SegRegion;
typedef Segment::SegmentationDesc::Region2D::Scanline Scanline;
typedef Segment::SegmentationDesc::Region2D::Scanline::Interval ScanlineInterval;

struct RegionMatch {
  RegionMatch(int r_id, float _dist = 0, float area = 0, float prob = 0) 
      : region_id(r_id), dist(_dist), area_fraction(area), probability(prob) {}
  int region_id;
  float dist;
  float area_fraction;
  float probability;
};

struct RegionMatchIDComp {
  bool operator() (const RegionMatch& lhs, const RegionMatch& rhs) {
    return lhs.region_id < rhs.region_id;
  }
};

struct RegionMatchDistComp {
  bool operator() (const RegionMatch& lhs, const RegionMatch& rhs) {
    return lhs.dist < rhs.dist;
  }
};


void ForegroundTracker::PropagateForeground(const IplImage* old_id_img_,
                                            const vector<RegionProb>& old_probs,
                                            const hash_map<int, int> old_prob_map,
                                            const Segment::SegmentationDesc& old_seg,
                                            const vector<RegionFlow>& cur_flow, 
                                            const Segment::SegmentationDesc& cur_seg,
                                            const hash_map<int, int> cur_prob_map,
                                            vector<RegionProb>* cur_probs) {
  // For each region in the current frame - based on intersection -
  // get the id's of regions in the previous frame, translated by the region flow (if applicable)
  // and count how often this happens.
  /*
  // Tuple of value*weight and weight indexed by region id.
  typedef hash_map<int, std::pair<float, float> > ProbMap;
  ProbMap prob_map;
  
  hash_map<int, vector<RegionMatch> > matches;
  
  // Build mapping from region_flow index to region id.
  hash_map<int, int> flow_map;
  for (vector<RegionFlow>::const_iterator i = cur_flow.begin(); i != cur_flow.end(); ++i)
    flow_map[i->region_id] = i - cur_flow.begin();
  
  const RepeatedPtrField<SegRegion>& regions = cur_seg.region();
  for (RepeatedPtrField<SegRegion>::const_iterator r = regions.begin();
       r != regions.end();
       ++r) {
    
    // Get id.
    int region_id;
    int region_sz;
    
    if (cur_level_ == 0) {
      region_id = r->id();
      region_sz = r->size();
    } else {
      // Traverse to parent region.
      int parent_id = r->parent_id();
      
      for (int l = 0; l < cur_level_ - 1; ++l) {
        ASSERT_LOG(cur_seg.hierarchy(l).region_size() >= parent_id);
        parent_id = cur_seg.hierarchy(l).region(parent_id).parent_id();
      }
      
      region_id = parent_id;
      region_sz = cur_seg.hierarchy(cur_level_ - 1).region(region_id).size();
    }
    
    // Region_id, Intersection size tuple.
    hash_map<int, int> prev_regions;
    
    // Get region flow vector if exists.
    hash_map<int, int>::const_iterator pos = flow_map.find(region_id);
    CvPoint flow_vec = cvPoint(0, 0);
    if (pos != flow_map.end()) {
      ASSERT_LOG(region_id == cur_flow[pos->second].region_id);
      flow_vec = cvPointFrom32f(cur_flow[pos->second].mean_vector);
    }
    
    const RepeatedPtrField<Scanline>& scanlines = r->scanline();
    const int* id_ptr = RowPtr<int>(old_id_img_, r->top_y() + flow_vec.y) + flow_vec.x;
                                          
    for(RepeatedPtrField<Scanline>::const_iterator s = scanlines.begin();
        s != scanlines.end();
        ++s) {
      int s_idx = r->top_y() + flow_vec.y + s-scanlines.begin();
      if (s_idx >= 0 && s_idx < frame_height_) {
        for (int i = 0, sz = s->interval_size(); i < sz; ++i) {
          const ScanlineInterval& inter = s->interval(i);
          const int* id_ptr_local = id_ptr + inter.left_x();
          
          for (int j = 0, len = inter.right_x() - inter.left_x() + 1;
               j < len;
               ++j, ++id_ptr_local) {
            int j_pos = inter.left_x() + flow_vec.x + j;
            
            if (j_pos >= 0 && j_pos < frame_width_) 
              ++prev_regions[*id_ptr_local];
          }
        }
      }
      id_ptr = PtrOffset(id_ptr, old_id_img_->widthStep);
    }
    
    int cur_prob_idx = cur_prob_map.find(region_id)->second;
    Segment::ColorHistogram& reg_hist = cur_probs->at(cur_prob_idx).hist;
    
    vector<RegionMatch>& region_match = matches[region_id];
    
    // Match with each of prev_regions based on color and get their RegionProbs.
    for (hash_map<int, int>::const_iterator pi = prev_regions.begin();
         pi != prev_regions.end(); 
         ++pi) {
      
      float area_ratio = (float)pi->second / (float)region_sz;      
      // Does Region_match already exists?
      vector<RegionMatch>::iterator i = std::lower_bound(region_match.begin(),
                                                         region_match.end(),
                                                         RegionMatch(pi->first),
                                                         RegionMatchIDComp());
      if (i != region_match.end() && i->region_id == pi->first) {
        // Update area fraction.
        i->area_fraction += area_ratio;
      } else {
        // Match histograms and insert.
        int old_prob_idx = old_prob_map.find(pi->first)->second;
        float chi_dist =  reg_hist.L2Dist(old_probs[old_prob_idx].hist);
        RegionMatch m(pi->first, chi_dist, area_ratio, old_probs[old_prob_idx].probability);
        region_match.insert(i, m);
      }
    }
  }
  
  // Set each regions probability.
  for (hash_map<int, vector<RegionMatch> >::iterator i = matches.begin(); i != matches.end(); ++i) {

    // Sort based on distance.
    std::sort(i->second.begin(), i->second.end(), RegionMatchDistComp());
    
    int cur_prob_idx = cur_prob_map.find(i->first)->second;
    (*cur_probs)[cur_prob_idx].probability = 0;
    
    // Consider the 3 best matches.
    float match_area = 0;
    for (int j = 0; j < std::min<int>(i->second.size(), 3); ++j) {
      if (i->second[j].dist > 0.15f)
        continue;
      else {
        match_area += i->second[j].area_fraction;
        (*cur_probs)[cur_prob_idx].probability += i->second[j].probability *
                                                  i->second[j].area_fraction;      
     }
    }
    
    if (match_area)
      (*cur_probs)[cur_prob_idx].probability = 
         std::min(1.f, (*cur_probs)[cur_prob_idx].probability / match_area);
    else 
      (*cur_probs)[cur_prob_idx].probability = .0f;
  }
   */
}


void ForegroundTracker::PostProcess(list<FrameSetPtr>* append) {
  // Process rest.
  /*
  vector<SimilarityModel> models(frame_match_list_.size());
   BundleAdjustSimilarity(frame_match_list_, &models);
  
  // Concat models with previous bundle adjust pt and output.
  for (vector<SimilarityModel>::iterator sm = models.begin(); sm != models.end(); ++sm) {
    *sm = sm->Concat(prev_ba_model_);
  }
  
  for (vector<SimilarityModel>::iterator sm = models.begin() + 
       ((last_ba_frame_ != 0) ? 1 : 0);
       sm != models.end(); ++sm) {
    models_to_output_.push_back(*sm);
  }
  
  prev_ba_model_ = models.back();
  
  
  // Run graph cut optimization.
  int num_nodes = neighbor_indexes_.size();
  GCoptimizationGeneralGraph gc_gg(num_nodes, 2);
  
  // Built helper vectors.
  vector<int> num_neighbors(num_nodes);
  vector<int*> neighbor_indexes_ptr(num_nodes);
  vector<int*> neighbor_weights_ptr(num_nodes);
  vector<int> data_costs(num_nodes * 2);
  
  for (int i = 0; i < num_nodes; ++i) {
    neighbor_indexes_ptr[i] = &neighbor_indexes_[i][0];
    neighbor_weights_ptr[i] = &neighbor_weights_[i][0];
    num_neighbors[i] = neighbor_indexes_[i].size();
    data_costs[i * 2] = data_costs_[i].first;
    data_costs[i * 2 + 1] = data_costs_[i].second;
  }
  
  gc_gg.setAllNeighbors(&num_neighbors[0],
                        &neighbor_indexes_ptr[0],
                        &neighbor_weights_ptr[0]);
  

  gc_gg.setDataCost(&data_costs[0]);

  int label_transistion[4] = { 0, 1, 1, 0 };
  gc_gg.setSmoothCost(label_transistion);
  
  gc_gg.expansion();
  
  // Construct output.
  frame_id_shift_.push_back(frame_id_shift_.size());    // close id shift.
 
  // Write models to file and get graphcut output.
  int frame = 0;
  for (vector<SimilarityModel>::iterator sm = models_to_output_.begin();
       sm != models_to_output_.end(); 
       ++sm, ++frame) {
    ofs_feat_ << "Frame# " << frame
              << " " << sm->t_x << " " << sm->t_y 
              << " " << sm->s   << " " << sm->phi << "\n";
    ofs_feat_ << "Level# " << 0 << "\n";
    ofs_feat_ << "ForegroundProbs:";
    
    for (int r_id = 0, node = frame_id_shift_[frame]; 
         node < frame_id_shift_[frame+1]; ++node, ++r_id) {
      int l = gc_gg.whatLabel(node) * 255.0;
      ofs_feat_ << " " << r_id << " " << l;
    }
    
    if (sm + 1 != models_to_output_.end())
      ofs_feat_ << "\n";
  }
    */
  /*for (vector<SimilarityModel>::iterator sm = models.begin() + 
       ((last_ba_frame_ != 0) ? 1 : 0);
       sm != models.end(); ++sm) {
    
    // Write to file.
    ofs_feat_ << "Frame# " << frame_number_ - (models.size() - 1) + (sm - models.begin())
              << " " << sm->t_x << " " << sm->t_y 
              << " " << sm->s   << " " << sm->phi << "\n";
    
    ofs_feat_ << "Level# " << 0 << "\n";
    ofs_feat_ << "ForegroundProbs:";
    
    if (is_debug_mode_)
      ofs_debug_ << sm->s << "\t" << sm->phi << "\t" << sm->t_x << "\t" << sm->t_y;
    
    if (sm + 1 != models.end()) {
      ofs_feat_ << "\n";
      if (is_debug_mode_)
        ofs_debug_ << "\n";
    }
  }     
  */
  
  ofs_feat_.close();
  ofs_debug_.close();
}

float PolyArea(const QPolygon& p, float frame_height) {
  float intersect_area = 0;
  for (int i = 1; i < p.size(); ++i) {
    float h = p[i].x() - p[i-1].x();
    intersect_area += 0.5 * h * (frame_height - 1.f - p[i].y() +
                                 frame_height - 1.f - p[i-1].y());
  }
  return intersect_area;
}