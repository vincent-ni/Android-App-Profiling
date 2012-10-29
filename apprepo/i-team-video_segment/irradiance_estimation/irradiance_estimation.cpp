/*
 *  irradiance_estimation.h
 *  irradiance_estimation
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "irradiance_estimation.h"
#include <math.h>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <clapack.h>
#endif
#include "assert_log.h"
#include "image_util.h"

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace ImageFilter;
#define UNDEREXPOSED_THRESH 5
#define OVEREXPOSED_THRESH 250

#define MATCH_REJECT_THRESH 0.001
#define IRLS_TOTAL_ROUNDS 1

namespace {
float AtanWeight(float a) {
  //return 0.5f - atan((a-0.5)*5)/M_PI;
  const float z = 1 - (2 * a * a * a - 3 * a * a + 1);
  return 2 * z * z * z - 3 * z * z + 1;
  // const float z = 1 - (std::cos(a*M_PI)*0.5 + 0.5);
  // return std::cos(z*M_PI)*0.5 + 0.5;
}

float AtanWeightInv(float a) {
  return 1.0f - AtanWeight(a);
}
}

namespace VideoFramework {

MixedEMORModel::MixedEMORModel(int num_basis,
                               int keyframe_spacing,
                               int num_frames,
                               bool fixed_model,
                               bool upper_bound_constraint)
    : num_basis_(num_basis),
      keyframe_spacing_(keyframe_spacing),
      num_frames_(num_frames),
      fixed_model_(fixed_model),
      upper_bound_constraint_(upper_bound_constraint) {
  // Add one model for bias.
  basis_functions_.resize(num_basis + 1);

  // Load inverse emor model from file.
  std::ifstream emor_file("inv_emor.txt");
  ASSURE_LOG(emor_file) << "EMOR model txt file not present in current path.";

  for (int i = 0; i < num_basis + 1; ++i) {
    basis_functions_[i].resize(1024);
    for (int j = 0; j < 1024; ++j) {
      emor_file >> basis_functions_[i][j];
    }
  }

  log_exposures_.resize(num_frames);
  prev_constraint_model_ = NULL;
}

void MixedEMORModel::ComputeAlphaMixLUT() {
  alpha_mix_lut_.resize(num_frames_);
  for (int frame_idx = 0; frame_idx < num_frames_; ++frame_idx) {
    if (!fixed_model_) {
      // Find index in key_frame positions.
      vector<int>::const_iterator keyframe_pos =
          std::lower_bound(keyframe_positions_.begin(),
                           keyframe_positions_.end(),
                           frame_idx);
      ASSURE_LOG(keyframe_pos != keyframe_positions_.end());
      if (*keyframe_pos == frame_idx) {
        const int mix_idx = keyframe_pos - keyframe_positions_.begin();
        AlphaAndMixture alpha_mix = {1, 0, mix_idx, mix_idx };
        alpha_mix_lut_[frame_idx] = alpha_mix;
        continue;
      }

      const int lhs_key = keyframe_pos[-1];
      const int rhs_key = keyframe_pos[0];
      const int lhs_idx = keyframe_pos - 1 - keyframe_positions_.begin();
      const int rhs_idx = lhs_idx + 1;

      ASSURE_LOG(rhs_idx < num_keyframes_)
          << frame_idx << " of " << num_frames_ << ", "
          << lhs_key << " and " << rhs_key << " @ "
          << lhs_idx << " and " << rhs_idx << " of " <<  num_keyframes_;

      // Fractional distance.
      float diff = (float)(frame_idx - lhs_key) / (float)(rhs_key - lhs_key);
      AlphaAndMixture alpha_mix = { AtanWeight(diff),
                                    1.0f -  AtanWeight(diff),
                                    lhs_idx,
                                    rhs_idx };
      alpha_mix_lut_[frame_idx] = alpha_mix;
    } else {
      AlphaAndMixture alpha_mix = {1, 0, 0, 0};
      alpha_mix_lut_[frame_idx] = alpha_mix;
    }
  }
}

void MixedEMORModel::GetAlphasAndMixtures(int frame_idx,
                                          float* alphas,
                                          int* mixtures) const {
 /* if (frame_idx >= 0) {
    const AlphaAndMixture& alpha_mix = alpha_mix_lut_[frame_idx];
    alphas[0] = alpha_mix.alpha_1;
    alphas[1] = alpha_mix.alpha_2;
    mixtures[0] = alpha_mix.mixture_1;
    mixtures[1] = alpha_mix.mixture_2;
  } else {
    LOG(FATAL) << "Re-implement";
  }
*/

  if (!fixed_model_) {
    // TODO: Streaming mode, negative keyframes ...

    // Compute left keyframe.
    int lhs_key = (frame_idx / keyframe_spacing_) * keyframe_spacing_;

    // Negative, in case constraints are supplied.
    if (frame_idx < 0) {
      lhs_key -= keyframe_spacing_;
    }

    int rhs_key = std::min(num_frames_ - 1,
                           lhs_key + keyframe_spacing_);
    // Fractional distance.
    float diff = (float)(frame_idx - lhs_key);

    // Normalize if rhs_key != lhs_key (can happen at last frame).
    if (lhs_key != rhs_key) {
      diff /= (float)(rhs_key - lhs_key);
    }

    alphas[0] = AtanWeight(diff);
    alphas[1] = 1.0f - alphas[0];

    ASSERT_LOG(fabs(alphas[0] + alphas[1] - 1) < 1e-6);

    mixtures[0] = lhs_key / keyframe_spacing_;
    mixtures[1] = std::min(num_keyframes_ - 1, mixtures[0] + 1);
  } else {
    // Single mixture.
    alphas[0] = 1;
    alphas[1] = 0;
    mixtures[0] = 0;
    mixtures[1] = 0;
  }
}

void MixedEMORModel::SetConstraintModel(MixedEMORModel* model) {
  prev_constraint_model_ = model;
};

void MixedEMORModel::EvaluateIRLSErrors(
  const FrameIntMatchesList& matches,
  vector<float> local_log_exposures,
  vector<float>* irls_weights) {
  /*
  // Holds mixture weights for model and match.
  vector<float> model_g_weight[2];
  vector<float> match_g_weight[2];
  for (int i = 0; i < 2; ++i) {
    model_g_weight[i].resize(num_basis_);
    match_g_weight[i].resize(num_basis_);
  }

  // Evaluate errors.
  for (int model_idx = 0, row = 0; model_idx < matches.size(); ++model_idx) {
    // Fractional distance to key-frames.
    float model_alphas[2];
    int model_mixtures[2];
    GetAlphasAndMixtures(model_idx + 1, &model_alphas[0], &model_mixtures[0]);

    for (int match_idx = 0; match_idx < matches[model_idx].size(); ++match_idx) {
      float match_alphas[2];
      int match_mixtures[2];
      GetAlphasAndMixtures(model_idx - match_idx, &match_alphas[0], &match_mixtures[0]);
      for (IntensityMatches::const_iterator match =
             matches[model_idx][match_idx]->begin();
           match != matches[model_idx][match_idx]->end();
           ++match, ++row) {
        // Map to radiance.
        const int curr_bin = match->int_1 * 1024;   // const for all j
        const int prev_bin = match->int_2 * 1024;

        const float g0_diff = basis_functions_[0][prev_bin] -
                              basis_functions_[0][curr_bin];
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            match_g_weight[m][k] = basis_functions_[k + 1][prev_bin] * match_alphas[m];
            model_g_weight[m][k] = -basis_functions_[k + 1][curr_bin] * model_alphas[m];
          }
        }

        // Evaluate errors.
        float err = g0_diff;
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            err += model_coeffs_[match_mixtures[m]][k] * match_g_weight[m][k] +
                   model_coeffs_[model_mixtures[m]][k] * model_g_weight[m][k];
          }
        }

        for (int k = 0; k <= match_idx; ++k) {
          err += local_log_exposures[model_idx - k];
        }

        (*irls_weights)[row] = 1.0 / (fabs(err) + 1e-6);
      }  // end intensity matches.
    }   // end model matches.
  }   // end models.
  */
}

float MixedEMORModel::SetupSystem(
  const FrameIntMatchesList& matches,
  const vector<float>& irls_weights,
  const vector<float>& log_exp_mean,
  cv::Mat* A_mat,
  cv::Mat* b_mat) {
  // Holds scalar weights for linear combination of match.
  vector<float> model_g_scalar[2];
  vector<float> match_g_scalar[2];
  for (int i = 0; i < 2; ++i) {
    model_g_scalar[i].resize(num_basis_);
    match_g_scalar[i].resize(num_basis_);
  }

  float mean_weight = 0;
  float av_diff = 0;
  int total_matches = 0;

  int row = 0;
  for (FrameIntMatchesList::const_iterator frame_iter = matches.begin();
       frame_iter != matches.end();
       ++frame_iter) {
    for (FrameIntMatches::const_iterator match_iter = frame_iter->begin();
         match_iter != frame_iter->end();
         ++match_iter) {
      const int model_idx = match_iter->frame_idx;
      const int match_idx = model_idx + match_iter->rel_idx;
      ASSURE_LOG(match_idx >= 0) << "Negative index, out of bound.";

      float model_alphas[2];
      int model_mixtures[2];
      float match_alphas[2];
      int match_mixtures[2];

      GetAlphasAndMixtures(model_idx, &model_alphas[0], &model_mixtures[0]);
      GetAlphasAndMixtures(match_idx, &match_alphas[0], &match_mixtures[0]);
      float av_diff_denom = 1.0f / (model_idx - match_idx);

      total_matches += match_iter->matches->size();
      for (IntensityMatches::const_iterator match = match_iter->matches->begin();
           match != match_iter->matches->end();
           ++match, ++row) {
        // Map to radiance.
        const int curr_bin = match->int_1 * 1024;   // const for all matches
        const int prev_bin = match->int_2 * 1024;
        av_diff += fabs(match->int_1 - match->int_2) * av_diff_denom;

        ASSERT_LOG(prev_bin < 1024);
        ASSERT_LOG(curr_bin < 1024);

        float* a_ptr = A_mat->ptr<float>(row);
        float* b_ptr = b_mat->ptr<float>(row);
        float w = exp(-.5 * (curr_bin - 512) * (curr_bin - 512) / (600.f * 600.f));

        w *= sqrt(irls_weights[row]);
        mean_weight += w;

        // Compute diff in radiance.
        // prev(I) + K_p = curr(I)
        // and prev_prev(I) + K_pp = prev(I)
        // ==> prev_prev_(I) + K_pp + K_p = curr(I)

        float g0_diff = basis_functions_[0][prev_bin] - basis_functions_[0][curr_bin];
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            match_g_scalar[m][k] = basis_functions_[k + 1][prev_bin] * match_alphas[m];
            model_g_scalar[m][k] = -basis_functions_[k + 1][curr_bin] * model_alphas[m];
          }
        }

        // Write to corresponding matrix entries.
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            a_ptr[num_basis_ * match_mixtures[m] + k] += match_g_scalar[m][k] * w;
            a_ptr[num_basis_ * model_mixtures[m] + k] += model_g_scalar[m][k] * w;
          }
        }

        ASSURE_LOG(match_idx < model_idx);
        for (int k = match_idx; k < model_idx; ++k) {
          a_ptr[num_keyframes_ * num_basis_ + k] = 1.f * w;
          g0_diff += log_exp_mean[k];
        }

        *b_ptr = -g0_diff * w;
      }
    }
  }

  LOG(INFO) << "Average difference in color: " << av_diff / total_matches;
  return mean_weight;
}

float MixedEMORModel::SetupSystemConstraint(
  const FrameIntMatchesList& matches,
  const vector<float>& irls_weights,
  const vector<float>& log_exp_mean,
  cv::Mat* A_mat,
  cv::Mat* b_mat) {
  // Compared to unconstrained solve, all models are shifted by one, i.e.
  // model_idx = 0, reflects already estimated model, model_idx = 1 -> unknown 0
  // the first to be estimated model.

  // Holds scalar weights for linear combination of match.
  vector<float> model_g_scalar[2];
  vector<float> match_g_scalar[2];
  for (int i = 0; i < 2; ++i) {
    model_g_scalar[i].resize(num_basis_);
    match_g_scalar[i].resize(num_basis_);
  }

  float mean_weight = 0;
  float av_diff = 0;
  int total_matches = 0;

  // One before last one as constraint (also need the one before depending on weighting).
  const int prev_model_last_idx = prev_constraint_model_->num_keyframes_ - 2;
  const vector<vector<float> >& prev_coeffs = prev_constraint_model_->model_coeffs_;

  const vector<float>& prev_log_exps = prev_constraint_model_->log_exposures_;
  const int prev_last_exp_idx = prev_log_exps.size() -
                                prev_constraint_model_->keyframe_spacing_;

  int row = 0;
  for (FrameIntMatchesList::const_iterator frame_iter = matches.begin();
       frame_iter != matches.end();
       ++frame_iter) {
    for (FrameIntMatches::const_iterator match_iter = frame_iter->begin();
         match_iter != frame_iter->end();
         ++match_iter) {
      const int model_idx = match_iter->frame_idx;
      ASSURE_LOG(model_idx >= 0) << "Negative model idx.";
      const int match_idx = model_idx + match_iter->rel_idx;

      float model_alphas[2];
      int model_mixtures[2];
      float match_alphas[2];
      int match_mixtures[2];

      GetAlphasAndMixtures(model_idx, &model_alphas[0], &model_mixtures[0]);
      GetAlphasAndMixtures(match_idx, &match_alphas[0], &match_mixtures[0]);
      float av_diff_denom = 1.0f / (model_idx - match_idx);

      total_matches += match_iter->matches->size();
      for (IntensityMatches::const_iterator match = match_iter->matches->begin();
           match != match_iter->matches->end();
           ++match, ++row) {
        // Map to radiance.
        const int curr_bin = match->int_1 * 1024;   // const for all matches
        const int prev_bin = match->int_2 * 1024;
        av_diff += fabs(match->int_1 - match->int_2) * av_diff_denom;

        ASSERT_LOG(prev_bin < 1024);
        ASSERT_LOG(curr_bin < 1024);

        float* a_ptr = A_mat->ptr<float>(row);
        float* b_ptr = b_mat->ptr<float>(row);
        float w = exp(-.5 * (curr_bin - 512) * (curr_bin - 512) / (600.f * 600.f));

        w *= sqrt(irls_weights[row]);
        mean_weight += w;

        // Compute diff in radiance.
        // prev(I) + K_p = curr(I)
        // and prev_prev(I) + K_pp = prev(I)
        // ==> prev_prev_(I) + K_pp + K_p = curr(I)

        const float g0_diff = basis_functions_[0][prev_bin] -
                              basis_functions_[0][curr_bin];
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            match_g_scalar[m][k] = basis_functions_[k + 1][prev_bin] * match_alphas[m];
            model_g_scalar[m][k] = -basis_functions_[k + 1][curr_bin] * model_alphas[m];
          }
        }

        float rhs = -g0_diff;

        // Write to corresponding matrix entries.
        for (int m = 0; m < 2; ++m) {
          for (int k = 0; k < num_basis_; ++k) {
            // This is a previous constraint.
            if (match_mixtures[m] <= 0) {
              rhs -= match_g_scalar[m][k] *
                     prev_coeffs[prev_model_last_idx + match_mixtures[m]][k];
            } else {
              a_ptr[num_basis_ * (match_mixtures[m] - 1) + k] +=
                match_g_scalar[m][k] * w;
            }

            if (model_mixtures[m] <= 0) {
              rhs -= model_g_scalar[m][k] *
                     prev_coeffs[prev_model_last_idx + match_mixtures[m]][k];
            } else {
              a_ptr[num_basis_ * (model_mixtures[m] - 1) + k] +=
                model_g_scalar[m][k] * w;
            }
          }
        }

        for (int k = match_idx; k < model_idx; ++k) {
          if (k >= 0) {
            a_ptr[(num_keyframes_ - 1) * num_basis_ + k] = 1.f * w;
            rhs -= log_exp_mean[k];
          } else {
            rhs -= prev_log_exps[prev_last_exp_idx + k] -
                   prev_log_exps[prev_last_exp_idx + k - 1];
          }
        }

        *b_ptr = rhs * w;
      }
    }
  }

  return mean_weight;
}

void MixedEMORModel::SetEquidistantKeyFrames() {
  num_keyframes_ = ceil((float)(num_frames_ - 1) / keyframe_spacing_) + 1;
  keyframe_positions_.resize(num_keyframes_);
  for (int i = 0; i < num_keyframes_; ++i) {
    keyframe_positions_[i] = std::min(i * keyframe_spacing_,
                                      num_frames_ - 1);
    if (i > 0) {
      ASSURE_LOG(keyframe_positions_[i] > keyframe_positions_[i - 1]);
    }
  }

  ASSURE_LOG(keyframe_positions_.back() == num_frames_ - 1);
}

void MixedEMORModel::SetAdaptiveKeyFrames(const vector<float>& log_exp_mean) {
  keyframe_positions_.clear();
  keyframe_positions_.push_back(0);
  float exp_start = 0;
  float log_exp = 0;
  for (int k = 0; k < log_exp_mean.size(); ++k) {
    log_exp += log_exp_mean[k];
    if (fabs(exp_start - log_exp) > 0.4) {
      keyframe_positions_.push_back(k + 1);
      exp_start = log_exp;
    }
  }

  // Extend last interval to match end.
  keyframe_positions_.back() = num_frames_ - 1;

  num_keyframes_ = keyframe_positions_.size();
  for (int k = 0; k < num_keyframes_; ++k) {
    LOG(INFO) << "KeyFrame " << k << " : " << keyframe_positions_[k];
  }

}

namespace {

// Returns estimate of change in log-exposure.
void GetLogExpPrior(const FrameIntMatchesList& matches,
                    vector<float>* log_exp_mean,
                    vector<float>* log_exp_var) {
  ASSURE_LOG(log_exp_mean);
  ASSURE_LOG(log_exp_var);

  log_exp_mean->resize(matches.size() - 1);
  log_exp_var->resize(matches.size() - 1);
  for (int i = 0; i < matches.size(); ++i) {
    const int j = 0;
    const int model_idx = matches[i][j].frame_idx;
    const int match_idx = matches[i][j].frame_idx + matches[i][j].rel_idx;
    if (match_idx + 1 == model_idx && matches[i][j].matches->size() > 0) {
      double diff_sum = 0;
      double diff_sum_sq = 0;
      // Compute difference in log. average.
      for (IntensityMatches::const_iterator int_match = matches[i][j].matches->begin();
           int_match != matches[i][j].matches->end();
           ++int_match) {
        const double log_curr = log(int_match->int_1 + 1e-6);
        const double log_prev = log(int_match->int_2 + 1e-6);
        const double diff = log_curr - log_prev;
        diff_sum += diff;
        diff_sum_sq += diff * diff;
      }

      diff_sum /= matches[i][j].matches->size();
      diff_sum_sq /= matches[i][j].matches->size();

      (*log_exp_mean)[i - 1] = diff_sum;
      (*log_exp_var)[i - 1] = diff_sum_sq - diff_sum * diff_sum;
    }
  }

  /*
  int frame_matches = 0;
  for (int i = 0; i < matches.size(); ++i) {
    for (int j = 0; j < matches[i].size(); ++j) {
      if (matches[i][j].frame_idx + matches[i][j].rel_idx >= 0 &&
          matches[i][j].matches->size() > 0) {
        ++frame_matches;
      }
    }
  }

  // Get prior for change in intensity.
  const int num_exposure_diffs = matches.size() - 1;
  cv::Mat A(frame_matches, num_exposure_diffs, CV_32F);
  cv::Mat b(frame_matches, 1, CV_32F);

  for (int i = 0, row = 0; i < matches.size(); ++i) {
    for (int j = 0; j < matches[i].size(); ++j) {
      const int model_idx = matches[i][j].frame_idx;
      const int match_idx = matches[i][j].frame_idx + matches[i][j].rel_idx;
      if (match_idx >= 0 && matches[i][j].matches->size() > 0) {
        float av_log_prev = 0;
        float av_log_curr = 0;
        // Compute difference in log. average.
        for (IntensityMatches::const_iterator int_match = matches[i][j].matches->begin();
             int_match != matches[i][j].matches->end();
             ++int_match) {
          av_log_curr += log(int_match->int_1 + 1e-6);
          av_log_prev += log(int_match->int_2 + 1e-6);
        }
        float log_diff_irr = (av_log_curr - av_log_prev) / matches[i][j].matches->size();
        *b.ptr<float>(row) = log_diff_irr;
        float* row_ptr = A.ptr<float>(row);
        for (int k = match_idx; k < model_idx; ++k) {
          row_ptr[k] = 1.f;
        }

        ++row;
      }
    }
  }

  cv::Mat solution(num_exposure_diffs, 1, CV_32F);
  cv::solve(A, b, solution, cv::DECOMP_QR);
  log_exp_prior->resize(num_exposure_diffs);
  for (int i = 0; i < num_exposure_diffs; ++i) {
    (*log_exp_prior)[i] = *solution.ptr<float>(i);
  }
  */
}

} // namespace.

void MixedEMORModel::FitEmorModel(
  const FrameIntMatchesList& matches,
  float adjaceny_scale,
  float bayes_prior_scale) {
  ASSURE_LOG(num_frames_ == matches.size())
      << "Expecting one match for each frame pair, frames: " << num_frames_
      << " vs. matches " << matches.size();

  // N frames have N-1 differences in exposure.
  const int num_exposure_diffs = matches.size() - 1;

  // Compute total matches and frame matches.
  int total_matches = 0;
  for (int i = 0; i < num_frames_; ++i) {
    for (int j = 0; j < matches[i].size(); ++j) {
      total_matches += matches[i][j].matches->size();
    }
  }

  LOG(INFO) << "Building system over " << total_matches << " matches.";
  bool is_constraint_optimization = prev_constraint_model_ != NULL;

  vector<float> log_exp_mean;
  vector<float> log_exp_var;
  GetLogExpPrior(matches, &log_exp_mean, &log_exp_var);
  //log_exp_mean = vector<float>(log_exp_mean.size(), 0.f);

  // Allocate coeffs.
  if (!fixed_model_) {
    SetEquidistantKeyFrames();
 //   SetAdaptiveKeyFrames(log_exp_mean);
    ASSURE_LOG(num_keyframes_ > 1) << "keyframe_spacing < num_frames";
  } else {
    num_keyframes_ = 1;
  }
  ComputeAlphaMixLUT();

  model_coeffs_.resize(num_keyframes_);

  LOG(INFO) << "Creating mixed EMOR model with " << num_keyframes_ << " mixtures.";

  for (int i = 0; i < num_keyframes_; ++i) {
    model_coeffs_[i].resize(num_basis_);
  }

  if (is_constraint_optimization) {
    // Set first basis coefficents.
    for (int i = 0; i < num_basis_; ++i) {
      // Use one clip as overlap, constrain on previous to last one.
      model_coeffs_[0][i] =
          prev_constraint_model_->model_coeffs_[
              prev_constraint_model_->NumMixtures() - 2][i];
    }
  }

  // Compute number of unknowns: num_basis_ unknowns for each model plus log exposures.
  const int exp_var_start =
    (is_constraint_optimization ? (num_keyframes_ - 1) : num_keyframes_) * num_basis_;
  const int num_unknowns = exp_var_start + num_exposure_diffs;

  // Here we are solving for the coefficients of the Emor model mapping
  // intensity to log exposure, specifically:
  // We write the log-inverse g of the camera response function f as linear combination
  // of basis functions:
  // g = g_0 + sum_{i=1..3} g_i * c_i, with coeffs c_i unknown.
  // An intensity match (p, q) satisfies the constraint
  // g(p) - g(q) = log K, where K is the multiplicative exposure difference.
  // (denoting the inverse of f with g', we have g'(p) = g'(q) * K, taking the log,
  //  leads to the above constraints)
  // Note that here, we have to optimize over several unknown log exposure differences
  // K_i, one for each matching frame-pair.
  // Last rows contains origin constraint for each model of mixture.
  bool use_ambig_constraint = false;
  int ambig_constraints = use_ambig_constraint ? num_keyframes_ : 0 +
                          upper_bound_constraint_ ? num_keyframes_ : 0;

  const bool use_adjaceny = !fixed_model_;
  const int adj_constraints = use_adjaceny ? (num_keyframes_ - 1) * num_basis_ : 0;
  const int exp_diff_constraints = fixed_log_exp_diff_.size();

  const int num_rows = total_matches +
                       ambig_constraints +
                       adj_constraints +
                       exp_diff_constraints;

  cv::Mat A(num_rows, num_unknowns, CV_32F);
  cv::Mat b(num_rows, 1, CV_32F);

  // Optional IRLS weighting scheme.
  vector<float> irls_weights(total_matches, 1.0f);
  const int max_rounds = IRLS_TOTAL_ROUNDS;

  for (int round = 0; round < max_rounds; ++round) {
    A = 0.f;
    b = 0.f;
    float mean_weight;
    if (!is_constraint_optimization) {
      mean_weight = SetupSystem(matches,
                                irls_weights,
                                log_exp_mean,
                                &A,
                                &b);
    } else {
      mean_weight = SetupSystemConstraint(matches,
                                          irls_weights,
                                          log_exp_mean,
                                          &A,
                                          &b);
    }

    if (total_matches > 0) {
      mean_weight = mean_weight / total_matches;
    }

    LOG(INFO) << "Using mean weight: " << mean_weight;

    // Last row hard constraint to resolve exponential ambiguity, specifically
    // we have g(0.5) = log(0.5);
    float ambiguity_weight = 10 * mean_weight;
    const int mixture_offset = is_constraint_optimization ? 1 : 0;

    if (upper_bound_constraint_) {
      for (int m = 0; m < num_keyframes_ - mixture_offset; ++m) {
        float* a_ptr = A.ptr<float>(total_matches + 2 * m);
        float* a_ptr_next = A.ptr<float>(total_matches + 2 * m + 1);
        float* b_ptr = b.ptr<float>(total_matches + 2 * m);
        float* b_ptr_next = b.ptr<float>(total_matches + 2 * m + 1);

        const int under_bin = (float)UNDEREXPOSED_THRESH / 255.f * 1023.f;
        const int over_bin = (float)OVEREXPOSED_THRESH / 255.f * 1023.f;

        for (int k = 0; k < num_basis_; ++k) {
          a_ptr[num_basis_ * m + k] =
            basis_functions_[k + 1][under_bin] * ambiguity_weight;
          a_ptr_next[num_basis_ * m + k] =
            basis_functions_[k + 1][over_bin] * ambiguity_weight;
        }

        b_ptr[0] = (-basis_functions_[0][under_bin] + log(0.05)) * ambiguity_weight;
        b_ptr_next[0] = (-basis_functions_[0][over_bin] + log(0.95)) * ambiguity_weight;
      }
    } else if (use_ambig_constraint) {
      for (int m = 0; m < num_keyframes_ - mixture_offset; ++m) {
        float* a_ptr = A.ptr<float>(total_matches + m);
        float* b_ptr = b.ptr<float>(total_matches + m);

        const int under_bin = (float)UNDEREXPOSED_THRESH / 255.f * 1023.f;
        for (int k = 0; k < num_basis_; ++k) {
          a_ptr[num_basis_ * m + k] =
            basis_functions_[k + 1][under_bin] * ambiguity_weight;
        }

        b_ptr[0] = (-basis_functions_[0][under_bin] + log(0.05)) * ambiguity_weight;
      }
    }

    if (use_adjaceny) {
      const float adjaceny_weight = ambiguity_weight * adjaceny_scale;
      for (int m = 0; m < num_keyframes_ - 1; ++m) {
        for (int k = 0; k < num_basis_; ++k) {
          float* a_ptr = A.ptr<float>(total_matches + ambig_constraints +
                                      m * num_basis_ + k);
          float* b_ptr = b.ptr<float>(total_matches + ambig_constraints +
                                      m * num_basis_ + k);
          float rhs = 0;
          if (m - mixture_offset < 0) {
            const int prev_model_last_idx = prev_constraint_model_->num_keyframes_ - 1;
            rhs -= prev_constraint_model_->model_coeffs_[prev_model_last_idx][k] *
                   adjaceny_weight;
          } else {
            a_ptr[num_basis_ * m + k] = -adjaceny_weight;
          }

          a_ptr[num_basis_ * (m + 1) + k] = adjaceny_weight;
          b_ptr[0] = rhs * adjaceny_weight;
        }
      }
    }

    if (exp_diff_constraints > 0) {
      const float diff_weight = ambiguity_weight * 100;
      int exp_diff_counter = 0;
      for (hash_map<int, float>::const_iterator exp_diff_iter =
             fixed_log_exp_diff_.begin();
           exp_diff_iter != fixed_log_exp_diff_.end();
           ++exp_diff_iter, ++exp_diff_counter) {
        float* a_ptr = A.ptr<float>(total_matches + ambig_constraints +
                                     adj_constraints + exp_diff_counter);
        float* b_ptr = b.ptr<float>(total_matches + ambig_constraints +
                                     adj_constraints + exp_diff_counter);
        a_ptr[exp_var_start + exp_diff_iter->first] = diff_weight;
        b_ptr[0] = diff_weight * exp_diff_iter->second;
      }
    }

    // Prior enforcement.
    LOG(INFO) << "Solving system ...";
    cv::Mat lhs(num_unknowns, num_unknowns, CV_32F);
    cv::Mat rhs(num_unknowns, 1, CV_32F);
    cv::gemm(A, A, 1, cv::Mat(), 0, lhs, cv::GEMM_1_T);
    cv::gemm(A, b, 1, cv::Mat(), 0, rhs, cv::GEMM_1_T);

    // Std deviations for coefficient from singular values of SVD.
    float coeff_variance[] = { 15.6267, 9.3326, 3.3232, 2.9070, 2.1368,
                                1.9855, 1.3131, 1.2766, 1.0617, 0.8783 };

    const float prior_weight = ambiguity_weight * bayes_prior_scale;
    for (int i = 0; i < num_keyframes_ - mixture_offset; ++i) {
      for (int j = 0; j < num_basis_; ++j) {
        const int coeff = i * num_basis_ + j;
        float* ptr = lhs.ptr<float>(coeff) + coeff;
        *ptr += prior_weight / coeff_variance[j];
      }
    }

    for (int k = 0; k < log_exp_var.size(); ++k) {
      const int coeff = exp_var_start + k;
      float* ptr = lhs.ptr<float>(coeff) + coeff;
      *ptr += prior_weight / log_exp_var[k];
    }

    cv::Mat solution(num_unknowns, 1, CV_32F);
    cv::solve(lhs, rhs, solution);

    for (int k = 0; k < num_keyframes_ - mixture_offset; ++k) {
      for (int l = 0; l < num_basis_; ++l) {
        model_coeffs_[k + mixture_offset][l] = *solution.ptr<float>(k * num_basis_ + l);
      }
    }

    vector<float> local_log_exposures(num_exposure_diffs);
    for (int i = 0; i < num_exposure_diffs; ++i) {
      local_log_exposures[i] = *solution.ptr<float>(exp_var_start + i) + log_exp_mean[i];
    }

    if (round + 1 == max_rounds) {
      // Last iteration, no error computation.
      // Save summed exposure.
      log_exposures_[0] = 0;
      for (int i = 0; i < num_exposure_diffs; ++i) {
        log_exposures_[i + 1] = log_exposures_[i] + local_log_exposures[i];
      }
    } else {
      EvaluateIRLSErrors(matches, local_log_exposures, &irls_weights);
    }
  }

  LOG(INFO) << "Model Coeffs: ";
  for (int k = 0; k < num_keyframes_; ++k) {
    LOG(INFO) << "Model " << k << " at " << k* keyframe_spacing_ << ": ";
    for (int l = 0; l < num_basis_; ++l) {
      LOG(INFO) << "\tCoeff " << l << " : "  << model_coeffs_[k][l] ;
    }
  }
}

namespace {

float GetValueInterpolated(float x, const vector<float>& data) {
  const int bin_x = x;
  const float dx = 0; //x - (float)bin_x;
  const int inc_x = 0; //dx <= 1e-6;

  return (1.0f - dx) * data[bin_x] + dx * data[bin_x + inc_x];
}

}  // namespace.

float MixedEMORModel::MapToIrradiance(float intensity, int frame) const {
  float bin = intensity / 255.f * 1023.f;
  float irr = GetValueInterpolated(bin, basis_functions_[0]);

  float alphas[2];
  int mixtures[2];
  GetAlphasAndMixtures(frame, &alphas[0], &mixtures[0]);

  for (int m = 0; m < 2; ++m) {
    for (int k = 0; k < num_basis_; ++k) {
      irr += GetValueInterpolated(bin, basis_functions_[k + 1]) *
             model_coeffs_[mixtures[m]][k] * alphas[m];
    }
  }

  irr = exp(irr - log_exposures_[frame]);
  return irr;
}

void MixedEMORModel::WriteResponsesToStream(int num_mixtures,
    std::ostream& os) const {
  for (int m = 0; m < num_mixtures; ++m) {
    if (m != 0) {
      os << "\n";
    }

    for (int i = 0; i < 1024; ++i) {
      float res_value = basis_functions_[0][i];
      for (int k = 0; k < num_basis_; ++k) {
        res_value += basis_functions_[k + 1][i] * model_coeffs_[m][k];
      }

      if (i != 0) {
        os << " ";
      }

      os << res_value;
    }
  }
}

void MixedEMORModel::WriteExposureToStream(int num_frames,
    std::ostream& os) const {
  for (int i = 0; i < num_frames; ++i) {
    if (i != 0) {
      os << " ";
    }

    os << exp(log_exposures_[i]);
  }
}

void MixedEMORModel::WriteEnvelopesToStream(int num_frames,
                                            float gain,
                                            float bias,
                                            std::ostream& os) const {
  // Write lower envelope.
  for (int f = 0; f < num_frames; ++f) {
    if (f != 0) {
      os << "\n";
    }

    float out_value = MapToIrradiance(UNDEREXPOSED_THRESH, f) * gain + bias;
    os << out_value << " ";
    out_value = MapToIrradiance(128, f) * gain + bias;
    os << out_value << " ";
    out_value = MapToIrradiance(OVEREXPOSED_THRESH, f) * gain + bias;
    os << out_value;
  }
}

void MixedEMORModel::SetBaseExposure(float exp) {
  for (vector<float>::iterator le = log_exposures_.begin();
       le != log_exposures_.end();
       ++le) {
    *le += exp;
  }
}

IrradianceEstimator::IrradianceEstimator(int num_emor_models,
                                         int key_frame_spacing,
                                         float min_percentile,
                                         float max_percentile,
                                         float adjaceny_scale,
                                         float bayes_prior_scale,
                                         bool fixed_model_estimation,
                                         bool upper_bound_constraint,
                                         int sliding_window_mode,
                                         bool markup_outofbound,
                                         int clip_size,
                                         int tmo_mode,
                                         float exposure,
                                         float mode_param,
                                         bool live_mode,
                                         const string& video_file,
                                         const std::string& video_stream_name,
                                         const std::string& flow_stream_name)
    : num_emor_models_(num_emor_models),
      min_percentile_(min_percentile),
      max_percentile_(max_percentile),
      adjaceny_scale_(adjaceny_scale),
      bayes_prior_scale_(bayes_prior_scale),
      fixed_model_estimation_(fixed_model_estimation),
      upper_bound_constraint_(upper_bound_constraint),
      sliding_window_mode_(sliding_window_mode),
      markup_outofbound_(markup_outofbound),
      clip_size_(clip_size),
      video_stream_name_(video_stream_name),
      flow_stream_name_(flow_stream_name),
      _tmo_mode(tmo_mode),
      exposure_(exposure * 10.f),
      _tmo_sval(mode_param * 10.f),
      video_file_(video_file),
      live_mode_(live_mode) {
  keyframe_spacing_ = key_frame_spacing;
  max_smoothed_frame_buffer_ = 50;
  frames_per_clip_ = clip_size_ * keyframe_spacing_ + 1;
  frame_num_ = 0;
  ASSURE_LOG(clip_size_ == 0 || clip_size_ > 2) << "Clip size at least two!";
  ASSURE_LOG(!(live_mode_ && clip_size_)) << "Cannot run streaming solve in live mode";
}

bool IrradianceEstimator::OpenStreams(StreamSet* set) {
  video_stream_idx_ = FindStreamIdx(video_stream_name_, set);

  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  const VideoStream* vid_stream =
    dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();

  // Get optical flow stream.
  flow_stream_idx_ = FindStreamIdx(flow_stream_name_, set);

  if (flow_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find optical flow stream.\n";
    return false;
  }

  irr_width_step_ = vid_stream->width_step();
  VideoStream* irr_stream = new VideoStream(frame_width_,
      frame_height_,
      irr_width_step_,
      vid_stream->fps(),
      vid_stream->frame_count(),
      PIXEL_FORMAT_BGR24,
      "IrradianceStream");

  set->push_back(shared_ptr<VideoStream>(irr_stream));

  unit_exhausted_ = false;
  output_clips_ = 0;
  return true;
}

namespace {

// Fast bilinear interpolation for 3 channel uint image, return values in
// float.
inline void InterpolateRGB(const IplImage* C3, float x, float y,
                           float* out_c1, float* out_c2, float* out_c3) {
  const int left = x;
  const int top = y;

  const float dx = x - (float)left;
  const float dy = y - (float)top;

  const int inc_x = 0; //dx <= 1e-6;
  const int inc_x3 = 3 * inc_x;
  const int inc_y = 0; //dy <= 1e-6;

  const float _1dx = 1 - dx;
  const float _1dy = 1 - dy;

  const float _1dx_1dy = _1dx * _1dy;
  const float _1dxdy = _1dx * dy;
  const float _1dydx = _1dy * dx;
  const float dxdy = dx * dy;

  const uchar* top_left = RowPtr<uchar>(C3, top) + left * 3;
  const uchar* bottom_left = PtrOffset<uchar>(top_left, inc_y * C3->widthStep);
  *out_c1 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
            _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

  ++top_left, ++bottom_left;
  *out_c2 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
            _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);

  ++top_left, ++bottom_left;
  *out_c3 = _1dx_1dy * *top_left + _1dydx * *(top_left + inc_x3) +
            _1dxdy * *bottom_left + dxdy * *(bottom_left + inc_x3);
}

inline void IncrementInterpolated(IplImage* img, float x, float y) {
  const int left = x;
  const int top = y;

  const float dx = 0; //x - (float)left;
  const float dy = 0; //y - (float)top;

  const int inc_x = 0; //dx <= 1e-6;
  const int inc_y = 0; //dy <= 1e-6;

  const float _1dx = 1 - dx;
  const float _1dy = 1 - dy;

  float* top_left = RowPtr<float>(img, top) + left;
  float* bottom_left = PtrOffset<float>(top_left, inc_y * img->widthStep);
  top_left[0] += _1dx * _1dy;
  top_left[inc_x] += dx * _1dy;
  bottom_left[0] += _1dx * dy;
  bottom_left[inc_x] += dx * dy;
}

}  //namespace.

void IrradianceEstimator::ComputeBTF(const IplImage* comparagram,
                                     IntensityMatches* intensity_matches) {
  ASSURE_LOG(comparagram->nChannels == 1);
  ASSURE_LOG(intensity_matches);

  shared_ptr<IplImage> summed_comp = cvCreateImageShared(comparagram->width,
                                                         comparagram->height,
                                                         IPL_DEPTH_64F,
                                                         1);
  cvConvertScale(comparagram, summed_comp.get());
  // Monotonic DP from top left to bottom right using N-8.
  for (int i = 0; i < comparagram->height; ++i) {
    double* curr_row_ptr = RowPtr<double>(summed_comp, i);
    double* prev_row_ptr = RowPtr<double>(summed_comp, i - 1);

    for (int j = 0;
         j < comparagram->width;
         ++j, ++curr_row_ptr, ++prev_row_ptr) {
      double max_neighbor = 0.0;
      // First test, no max necessary.
      if (j > 0) {
        max_neighbor = curr_row_ptr[-1];
      }

      if (i > 0) {
        max_neighbor = std::max(max_neighbor, prev_row_ptr[0]);
        if (j > 0) {
          max_neighbor = std::max(max_neighbor, prev_row_ptr[-1]);
        }
      }

      curr_row_ptr[0] += max_neighbor;
    }
  }

  // Backtrack from bottom right, creating list of intensity matches.
  int curr_x = comparagram->width - 1;
  int curr_y = comparagram->height - 1;

  // We perform a box filter on the comparagram, only adding those matches with sufficient
  // support.
  // TODO: Does that actually help anything?
  vector<int> ptr_offsets;
  const int box_radius = 1;
  for (int i = -box_radius; i <= box_radius; ++i) {
    for (int j = -box_radius; j <= box_radius; ++j) {
      ptr_offsets.push_back(i * comparagram->widthStep + j * sizeof(float));
    }
  }

  const float weight_denom = 1.0f / ptr_offsets.size();

  const float inv_domain_x = 1.0f / (float)(comparagram->width - 1);
  const float inv_domain_y = 1.0f / (float)(comparagram->height - 1);

  while (curr_x != 0 && curr_y != 0) {
    // Insert current position.
    // Skip under and over-exposed regions.
    const float intensity_x = (float)curr_x * inv_domain_x;
    const float intensity_y = (float)curr_y * inv_domain_y;
    if (intensity_x >= UNDEREXPOSED_THRESH / 255.0f &&
        intensity_y >= UNDEREXPOSED_THRESH / 255.f &&
        intensity_x < OVEREXPOSED_THRESH / 255.f &&
        intensity_y < OVEREXPOSED_THRESH / 255.f ) {
      float weight_sum = 0;
      const float* center_ptr = RowPtr<float>(comparagram, curr_y) + curr_x;
      for (int k = 0; k < ptr_offsets.size(); ++k) {
        weight_sum += *PtrOffset(center_ptr, ptr_offsets[k]);
      }

      weight_sum *= weight_denom;
      if (weight_sum >= MATCH_REJECT_THRESH &&
          fabs(intensity_x - intensity_y) / intensity_x > 0.01) {
        intensity_matches->push_back(IntensityMatch(intensity_x,
                                                    intensity_y,
                                                    weight_sum));
      }
    }

    // Backtrack.
    double max_neighbor = 0.f;
    int inc_x = 0;
    int inc_y = 0;

    if (curr_x > 0) {
      const double neighbor_val = RowPtr<double>(summed_comp, curr_y)[curr_x - 1];
      if (neighbor_val >= max_neighbor) {
        max_neighbor = neighbor_val;
        inc_x = -1;
        inc_y = 0;
      }
    }

    if (curr_y > 0) {
      const double neighbor_val = RowPtr<double>(summed_comp, curr_y - 1)[curr_x];
      if (neighbor_val >= max_neighbor) {
        max_neighbor = neighbor_val;
        inc_x = 0;
        inc_y = -1;
      }

      if (curr_x > 0) {
        const double neighbor_val = RowPtr<double>(summed_comp, curr_y - 1)[curr_x - 1];
        if (neighbor_val >= max_neighbor) {
          max_neighbor = neighbor_val;
          inc_x = -1;
          inc_y = -1;
        }
      }
    }

    ASSERT_LOG(inc_x != 0 || inc_y != 0);
    curr_x += inc_x;
    curr_y += inc_y;
  }
}

// Extract set of matching intensities from feature matches.
void IrradianceEstimator::FindIntensityMatches(
  const IplImage* prev_image,
  const IplImage* curr_image,
  const vector<Feature>& features,
  const ColorIntMatches& matches,
  bool viz_comparagram) {
  ASSURE_LOG(prev_image->nChannels == curr_image->nChannels &&
             prev_image->nChannels == 3);
  ASSURE_LOG(prev_image->depth == curr_image->depth &&
             prev_image->depth == IPL_DEPTH_8U);

  // Extract patches of 5x5 size around each feature point.
  int patch_rad = 5;
  int patch_diam = 2 * patch_rad + 1;
  ASSURE_LOG(matches.size() == 3);
  ASSURE_LOG(prev_image->widthStep == curr_image->widthStep);

  const float scale = 1.0f / 255.0f;
  const float inv_scale = 255.0f;

  // Create patch offset.
  vector<int> offsets;
  for (int i = -patch_rad; i <= patch_rad; ++i) {
    for (int j = -patch_rad; j <= patch_rad; ++j) {
      offsets.push_back(i * curr_image->widthStep + 3 * j);
    }
  }

  // One comparagram per color channel.
  shared_ptr<IplImage> comparagram[3];
  const int comparagram_dim = 256;
  for (int c = 0; c < 3; ++c) {
    comparagram[c] = cvCreateImageShared(comparagram_dim,
                                         comparagram_dim,
                                         IPL_DEPTH_32F,
                                         1);
    cvZero(comparagram[c].get());
  }

  // Stat variables.
  int num_samples = 0;
  int rejected[3] = {0, 0, 0};

  for (vector<Feature>::const_iterator f = features.begin(); f != features.end(); ++f) {
    int x = f->loc.x;
    int y = f->loc.y;

    if (x < patch_rad ||
        x >= frame_width_ - patch_rad ||
        y < patch_rad ||
        y >= frame_height_ - patch_rad) {
      // Feature too close to boundary.
      continue;
    }

    float m_x = f->loc.x + f->vec.x;
    float m_y = f->loc.y + f->vec.y;

    if (m_x - 0.1f < patch_rad ||
        m_x > frame_width_ - 1.1f - patch_rad ||
        m_y - 0.1f < patch_rad ||
        m_y > frame_height_ - 1.1f - patch_rad) {
      continue;
    }

    // Compile local matches, only add to comparagram if variance of patch error is
    // low (avoid misregistration).
    // Intensity matches are pair of (matching image, current image) intensities.
    vector<std::pair<float, float> > local_int_matches[3];

    const uchar* curr_ptr = RowPtr<uchar>(curr_image, y) + 3 * x;
    num_samples += patch_diam * patch_diam;

    float error_sum[3] = {0, 0, 0};
    float error_sq_sum[3] = {0, 0, 0};

    for (int m = -patch_rad, k = 0; m <= patch_rad; ++m) {
      for (int n = -patch_rad; n <= patch_rad; ++n, ++k) {
        // Get interpolated values.
        float match[3];
        InterpolateRGB(prev_image,
                       m_x + n,
                       m_y + m,
                       &match[0],
                       &match[1],
                       &match[2]);

        for (int c = 0; c < 3; ++c) {
          const float val_1 = match[c];
          const float val_2 = PtrOffset(curr_ptr, offsets[k])[c];
          const float error = abs(val_1 - val_2) * scale;

          error_sum[c] += error;
          error_sq_sum[c] += error * error;

          // Ignore saturated pixels.
          if (val_1 >= OVEREXPOSED_THRESH ||
              val_2 >= OVEREXPOSED_THRESH ||
              val_1 < UNDEREXPOSED_THRESH ||
              val_2 < UNDEREXPOSED_THRESH) {
            ++rejected[c];
            continue;
          }

          // Save normalized.
          local_int_matches[c].push_back(std::make_pair(val_1 * scale, val_2 * scale));
        }
      }
    }  // end of this patch.

    for (int c = 0; c < 3; ++c) {
      // Only take this patch into account if error variance is low.
      error_sum[c] /= patch_diam * patch_diam;
      error_sq_sum[c] /= patch_diam * patch_diam;
      const float error_stdev = sqrt(error_sq_sum[c] - error_sum[c] * error_sum[c]);

      if (error_stdev < 0.1) {
        for (int k = 0; k < local_int_matches[c].size(); ++k) {
          // Convert to comparagram dimensions.
          // X-axis is current frame (second pair).
          float y = local_int_matches[c][k].first * (comparagram_dim - 1);
          float x = local_int_matches[c][k].second * (comparagram_dim - 1);

          IncrementInterpolated(comparagram[c].get(), x, y);
        }
      }
    }
  }  // end feature traversal.

  /*
  LOG(INFO) << "Rejected, red: " << (float)rejected[0] / num_samples
            << " green: " << (float)rejected[1] / num_samples
            << " blue: " << (float)rejected[2] / num_samples;
  */

  // Normalize comparagrams and compute BTF.
  for (int c = 0; c < 3; ++c) {
    double min_val, max_val;
    cvMinMaxLoc(comparagram[c].get(), &min_val, &max_val);
    // LOG(INFO) << "MAX: " << max_val;
    cvScale(comparagram[c].get(), comparagram[c].get(), 1.0 / (max_val + 1e-6));

    // Compute BTF as intensity matches from comparagram via DP.
    // TODO: Some kind of outlier or robustness handling. Dilation / Median filter?
    ComputeBTF(comparagram[c].get(), matches[c].get());

    // For visualization only: Highlight BTF by xor in comparagram and scale.
    for (IntensityMatches::iterator match = matches[c]->begin();
         match != matches[c]->end();
         ++match) {
      const int bin_x =  match->int_1 * comparagram[c]->width;
      const int bin_y =  match->int_2 * comparagram[c]->height;
      float* bin_ptr = RowPtr<float>(comparagram[c], bin_y) + bin_x;

      *bin_ptr = (1.f - *bin_ptr);
      ASSURE_LOG(match->int_1 <= 1.f) << match->int_1;
      ASSURE_LOG(match->int_2 <= 1.f) << match->int_2;
    }
  }

  static bool comp_init = true;
  if (viz_comparagram) {
    if (comp_init) {
      cvNamedWindow("comparagram 1"); cvMoveWindow("comparagram 1", 100, 10);
      cvNamedWindow("comparagram 2"); cvMoveWindow("comparagram 2", 400, 10);
      cvNamedWindow("comparagram 3"); cvMoveWindow("comparagram 3", 700, 10);
      comp_init = false;
    }
    cvShowImage("comparagram 1", comparagram[0].get());
    cvShowImage("comparagram 2", comparagram[1].get());
    cvShowImage("comparagram 3", comparagram[2].get());
    cvWaitKey(20);
  }

}

void IrradianceEstimator::FitGainBiasModel(
  const vector<shared_ptr<IntensityMatches> >& matches,
  vector<GainBiasModel>* models) {
  for (int match_id = 0; match_id != matches.size(); ++match_id) {
    // Per-frame pair model estimation.
    const int num_matches = matches[match_id]->size();
    const int unknowns = 2;

    shared_ptr<CvMat> A_mat = cvCreateMatShared(num_matches, unknowns, CV_32F);
    cvZero(A_mat.get());

    shared_ptr<CvMat> b_mat = cvCreateMatShared(num_matches, 1, CV_32F);
    cvZero(b_mat.get());

    int row = 0;
    for (IntensityMatches::const_iterator match = matches[match_id]->begin();
         match != matches[match_id]->end();
         ++match, ++row) {
      // Each element row weighted by match weight.
      float* a_ptr = RowPtr<float>(A_mat, row);
      float* b_ptr = RowPtr<float>(b_mat, row);
      float w = match->weight;

      a_ptr[0] = match->int_1 * w;
      a_ptr[1] = 1.f * w;
      b_ptr[0] = match->int_2 * w;
    }

    shared_ptr<CvMat> lhs_mat = cvCreateMatShared(unknowns, unknowns, CV_32F);
    shared_ptr<CvMat> rhs_mat = cvCreateMatShared(unknowns, 1, CV_32F);

    cvGEMM(A_mat.get(), A_mat.get(), 1.0, NULL, 0, lhs_mat.get(), CV_GEMM_A_T);
    cvGEMM(A_mat.get(), b_mat.get(), 1.0, NULL, 0, rhs_mat.get(), CV_GEMM_A_T);

    shared_ptr<CvMat> sol_mat = cvCreateMatShared(unknowns, 1, CV_32F);
    cvSolve(lhs_mat.get(), rhs_mat.get(), sol_mat.get());

    GainBiasModel model;
    model.gain = cvGet2D(sol_mat.get(), 0, 0).val[0];
    model.bias = cvGet2D(sol_mat.get(), 1, 0).val[0];
    models->push_back(model);
  }
}

void IrradianceEstimator::FitPolyModel(const vector<shared_ptr<IntensityMatches> >& matches,
                                       vector<PolyModel>* models) {
  for (int match_id = 0; match_id != matches.size(); ++match_id) {
    // Per-frame pair model estimation.
    const int num_matches = matches[match_id]->size();
    const int unknowns = 4;

    shared_ptr<CvMat> A_mat = cvCreateMatShared(num_matches, unknowns, CV_32F);
    cvZero(A_mat.get());

    shared_ptr<CvMat> b_mat = cvCreateMatShared(num_matches, 1, CV_32F);
    cvZero(b_mat.get());

    int row = 0;
    for (IntensityMatches::const_iterator match = matches[match_id]->begin();
         match != matches[match_id]->end();
         ++match, ++row) {
      // Each element row weighted by match weight.
      float* a_ptr = RowPtr<float>(A_mat, row);
      float* b_ptr = RowPtr<float>(b_mat, row);
      float w = 1;// match->weight;

      a_ptr[0] = match->int_1 * w;
      a_ptr[1] = pow(match->int_1,  1.5f) * w;
      a_ptr[2] = pow(match->int_1, 1 / 1.5f) * w;
      a_ptr[3] = 1.f * w;
      b_ptr[0] = match->int_2 * w;
    }

    shared_ptr<CvMat> lhs_mat = cvCreateMatShared(unknowns, unknowns, CV_32F);
    shared_ptr<CvMat> rhs_mat = cvCreateMatShared(unknowns, 1, CV_32F);

    cvGEMM(A_mat.get(), A_mat.get(), 1.0, NULL, 0, lhs_mat.get(), CV_GEMM_A_T);
    cvGEMM(A_mat.get(), b_mat.get(), 1.0, NULL, 0, rhs_mat.get(), CV_GEMM_A_T);

    shared_ptr<CvMat> sol_mat = cvCreateMatShared(unknowns, 1, CV_32F);
    cvSolve(lhs_mat.get(), rhs_mat.get(), sol_mat.get());

    PolyModel model;
    #pragma omp parallel for
    for (int i = 0; i < unknowns; ++i) {
      model.coeffs[i] = cvGet2D(sol_mat.get(), i, 0).val[0];
    }

    models->push_back(model);
  }
}

void IrradianceEstimator::ProcessFrame(FrameSetPtr input,
                                       list<FrameSetPtr>* output) {
  VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  IplImage image;
  frame->ImageView(&image);

  // Get optical flow.
  OpticalFlowFrame* flow_frame =
    dynamic_cast<OpticalFlowFrame*>(input->at(flow_stream_idx_).get());

  // Smooth input and add to images.
  shared_ptr<IplImage> curr_image = cvCreateImageShared(frame_width_,
                                    frame_height_,
                                    IPL_DEPTH_8U,
                                    3);

  // Bilateral presmoothing, with 7x7 spatial filter and color_sigma 20.
  cvSmooth(&image, curr_image.get(), CV_BILATERAL, 0, 0, 20, 2);

  // Overwrite input with bilateral.
  cvCopy(curr_image.get(), &image);

  // Compute median, 10% and 90% intensity value.
  vector<float> median(3);
  vector<float> percentile_10(3);
  vector<float> percentile_90(3);

  // Convert input to flat channel represenation
  vector<uchar> flat_channels[3];
  IplImage channel_views[3];
  const int num_pixels = frame_height_ * frame_width_;
  for (int c = 0; c < 3; ++c) {
    flat_channels[c].resize(num_pixels);
    cvInitImageHeader(&channel_views[c],
                      cvSize(frame_width_, frame_height_),
                      IPL_DEPTH_8U,
                      1);
    cvSetData(&channel_views[c], &flat_channels[c][0], frame_width_);
  }

  cvSplit(curr_image.get(), &channel_views[0], &channel_views[1], &channel_views[2],
          NULL);
  for (int c = 0; c < 3; ++c) {
    const int median_idx = num_pixels / 2;
    const int percent_10_idx = num_pixels * min_percentile_;
    const int percent_90_idx = num_pixels * max_percentile_;
    std::nth_element(flat_channels[c].begin(),
                     flat_channels[c].begin() + median_idx,
                     flat_channels[c].end());
    median[c] = flat_channels[c][median_idx];
    std::nth_element(flat_channels[c].begin(),
                     flat_channels[c].begin() + percent_10_idx,
                     flat_channels[c].end());
    percentile_10[c] = std::max(UNDEREXPOSED_THRESH,
                                (int)flat_channels[c][percent_10_idx]);
    std::nth_element(flat_channels[c].begin(),
                     flat_channels[c].begin() + percent_90_idx,
                     flat_channels[c].end());
    percentile_90[c] = std::min((int)flat_channels[c][percent_90_idx],
                                OVEREXPOSED_THRESH);
  }

  // Buffer smoothed images for intensity match extraction.
  prev_images_smoothed_.insert(prev_images_smoothed_.begin(), curr_image);
  if (prev_images_smoothed_.size() > max_smoothed_frame_buffer_) {
    prev_images_smoothed_.resize(max_smoothed_frame_buffer_);
  }

  // Add match lists.
  for (int c = 0; c < 3; ++c) {
    frame_int_matches_lists_[c].push_back(FrameIntMatches());
  }

  if (flow_frame->MatchedFrames() > 0) {
    // Ensure index of last tracked frames is below buffer.
    ASSURE_LOG(-flow_frame->FrameID(flow_frame->MatchedFrames() - 1) <
               prev_images_smoothed_.size());

    for (int k = 0; k < flow_frame->MatchedFrames(); ++k) {
      flow_frame->ImposeLocallyConsistentGridFlow(frame_width_,
          frame_height_,
          k,      // frame_num
          0.2,    // 5 regions in each dimension.
          0.2,
          1,
          1);

      const int rel_idx = flow_frame->FrameID(k);

      vector<Feature> features(flow_frame->NumberOfFeatures(k));
      flow_frame->FillVector(&features, k);

      // Extract matches across the three channels for current frame pair.
      ColorIntMatches color_matches(3);
      for (int c = 0; c < 3; ++c) {
        color_matches[c].reset(new IntensityMatches());
      }

      // Intensity matches from current image -> previous image.
      bool viz_comparagram = false;

      FindIntensityMatches(prev_images_smoothed_[-rel_idx].get(),  // inverse ordering.
                           prev_images_smoothed_[0].get(),
                           features,
                           color_matches,
                           viz_comparagram);

      // Save matches.
      for (int c = 0; c < 3; ++c) {
        FramePairIntMatch frame_pair_match(frame_num_, rel_idx, color_matches[c]);
        frame_pair_match.median = median[c];
        frame_pair_match.percentile_10 = percentile_10[c];
        frame_pair_match.percentile_90 = percentile_90[c];

        frame_int_matches_lists_[c].back().push_back(frame_pair_match);
      }
    }
  } else {
    // Add dummy match frame for min and max evaluation.
    for (int c = 0; c < 3; ++c) {
      FramePairIntMatch frame_pair_match(
          frame_num_, 0, shared_ptr<IntensityMatches>(new IntensityMatches));
      frame_pair_match.median = median[c];
      frame_pair_match.percentile_10 = percentile_10[c];
      frame_pair_match.percentile_90 = percentile_90[c];

      frame_int_matches_lists_[c].back().push_back(frame_pair_match);
    }
  }

  frame_set_buffer_.push_back(input);
  LOG(INFO) << "Processed frame " << frame_num_;
  ++frame_num_;

  // If streaming mode -> output if clip is filled, otherwise output
  // on PostProcess.
  if (clip_size_ > 0 && frame_set_buffer_.size() % frames_per_clip_ == 0) {
    CalibrateAndOutputFrames(output_clips_ == 0,    // first clip?
                             false,
                             output);
  }
}

void IrradianceEstimator::RunAsSlidingWindowMode() {
  LOG(FATAL) << "Outdated!, brush up!";
  /*
      const int win_sz = sliding_window_mode_;
      std::ofstream response_ofs("response_window.txt");
      std::ofstream exp_ofs("exposure_window.txt");
      std::ofstream coeff_ofs("coeffs_window.txt");

      float prev_exp = 0;
      vector<float> prev_diffs;
      for (int f = 0; f < intensity_matches_g_.size() - win_sz; ++f) {
          if (f != 0) {
              response_ofs << "\n";
              exp_ofs << "\n";
              coeff_ofs << "\n";
          }

          LOG(INFO) << "Fitting sliding window " << f << " of "
                    << intensity_matches_g_.size() - win_sz;
          // Get window size of matches.
          vector<vector<shared_ptr<IntensityMatches> > > local_matches(win_sz);
          for (int i = 0; i < win_sz; ++i) {
              for (int j = 0;
                      j < std::min<int>(intensity_matches_g_[f + i].size(), i + 1);
                      ++j) {
                  local_matches[i].push_back(intensity_matches_g_[f + i][j]);
              }
          }

          MixedEMORModel model(7, win_sz, win_sz + 1, true, true);
          if (f > 0) {
              for (int i = 0; i < prev_diffs.size() / 2; ++i) {
                  model.SetLogExpDiff(i, prev_diffs[i + 1]);
              }
          }

          model.FitEmorModel(local_matches);
          model.SetBaseExposure(prev_exp);

          for (int coeff = 0 ; coeff < 7; ++coeff) {
              if (coeff != 0) {
                  coeff_ofs << " ";
              }
              coeff_ofs << model.GetModelCoeff(0, coeff);
          }

          model.WriteResponsesToStream(model.NumMixtures(), response_ofs);
          model.WriteExposureToStream(model.NumFrames(), exp_ofs);
          prev_exp = model.GetLogExposure(1);
          prev_diffs.clear();
          for (int i = 0; i < model.NumLogExposures() - 1; ++i) {
              prev_diffs.push_back(model.GetLogExposure(i + 1) - model.GetLogExposure(i));
          }
      }

      coeff_ofs.close();
      exp_ofs.close();
      response_ofs.close();
      return;
      */
}

cv::Mat runHDR(cv::Mat& hdr, int _tmo_mode, float _tmo_sval, float _exposure,
               const vector<std::pair<float,float> >& min_max_irr) {

  /* hdr is: [0-1+] input -> [0-255] output */

  float ir_min = std::min(std::min(min_max_irr[0].first,min_max_irr[1].first),
                          min_max_irr[2].first);
  float ir_max = std::max(std::max(min_max_irr[0].second,min_max_irr[1].second),
                          min_max_irr[2].second);


  if (_tmo_mode == 4) {

    hdr /= ir_max;
    pow(hdr,_exposure, hdr);// * 0.7f + 0.15f;
    hdr *= 255.f;

    cv::Mat xyz(hdr.rows, hdr.cols, CV_32FC3);
    cv::cvtColor(hdr, xyz, CV_BGR2XYZ);

    vector<cv::Mat> lplanes;
    cv::split(xyz, lplanes);
    cv::Mat X(hdr.rows, hdr.cols, CV_32FC1);
    cv::Mat Y(hdr.rows, hdr.cols, CV_32FC1);
    cv::Mat Z(hdr.rows, hdr.cols, CV_32FC1);
    lplanes[0].convertTo(X, CV_32FC1);
    lplanes[1].convertTo(Y, CV_32FC1);
    lplanes[2].convertTo(Z, CV_32FC1);
    cv::Mat localcontrast(hdr.rows, hdr.cols, CV_32FC1);

    // blur-scale Y channel
    cv::Mat imgY = Y;
    cv::Mat blurredY; double sigmaY = 1;
    cv::bilateralFilter(imgY, blurredY, 0, 0.1*255.f, 2);
    cv::GaussianBlur(blurredY, blurredY, cv::Size(), sigmaY, sigmaY);
    cv::divide(imgY, blurredY, localcontrast);

    cv::Mat scale(hdr.rows, hdr.cols, CV_32FC1);
    cv::pow(localcontrast, _tmo_sval, scale);
    cv::multiply(X, scale, X);
    cv::multiply(Y, scale, Y);
    cv::multiply(Z, scale, Z);

    cv::Mat rgb[] = {X, Y, Z};
    cv::merge(rgb, 3, hdr);
    cv::cvtColor(hdr, hdr, CV_XYZ2BGR);

    // sharpen image using "unsharp mask" algorithm
    cv::Mat img = hdr;
    cv::Mat blurred; double sigma = 7, threshold = 0, amount = 0.3f;
    cv::GaussianBlur(img, blurred, cv::Size(), sigma, sigma);
    cv::Mat sharpened = img * (1 + amount) + blurred * (-amount);
    cv::Mat lowContrastMask = abs(img - blurred) < threshold;
    img.copyTo(sharpened, lowContrastMask);
    hdr = sharpened;

  } else if (_tmo_mode == 3) {

    // LCE power tone
    cv::Mat img = hdr;
    cv::Mat blurred; double sigma = 1, threshold = 0, amount = _tmo_sval;
    cv::bilateralFilter(img, blurred, 0, 0.1, 2);
    cv::GaussianBlur(blurred, blurred, cv::Size(), sigma, sigma);
    cv::Mat localcontrast;
    cv::divide(img, blurred, localcontrast);
    cv::pow(localcontrast, amount, localcontrast);
    blurred = (blurred - ir_min) / (ir_max - ir_min) * 0.7f + 0.15f;
    cv::pow(blurred,_exposure, blurred);
    blurred = blurred * 0.7f + 0.15f;
    cv::multiply(blurred,localcontrast,hdr);
    hdr *= 255;

    // sharpen image using "unsharp mask" algorithm
    img = hdr;
    sigma = 7; threshold = 0; amount = 0.3f;
    cv::GaussianBlur(img, blurred, cv::Size(), sigma, sigma);
    cv::Mat sharpened = img * (1 + amount) + blurred * (-amount);
    cv::Mat lowContrastMask = abs(img - blurred) < threshold;
    img.copyTo(sharpened, lowContrastMask);
    hdr = sharpened;

  } else if (_tmo_mode == 2) {

    // pass through
    hdr *= 255;

  } else if (_tmo_mode == 1) {

    cv::pow(hdr,_exposure, hdr);// * 0.7f + 0.15f;

    // sharpen image using "unsharp mask" algorithm
    cv::Mat img = hdr;
    cv::Mat blurred; double sigma = 7, threshold = 0, amount = _tmo_sval;
    cv::GaussianBlur(img, blurred, cv::Size(), sigma, sigma);
    cv::Mat sharpened = img * (1 + amount) + blurred * (-amount);
    cv::Mat lowContrastMask = abs(img - blurred) < threshold;
    img.copyTo(sharpened, lowContrastMask);
    hdr = sharpened;

    hdr *= 255;

  } else {

    cv::pow(hdr,_exposure, hdr);// * 0.7f + 0.15f;

    // gamma
    float cc = _tmo_sval;
    cv::Mat tmp = hdr;
    hdr = (hdr - ir_min) / (ir_max-ir_min);
    cv::pow(tmp, cc, hdr);
    hdr *= 255;

  }

  // clamp
  hdr = min(hdr, 255);
  hdr = max(hdr, 0);

  // output
  return hdr;
}

void IrradianceEstimator::HDRMapping(
    const vector<shared_ptr<MixedEMORModel> >& emor_models,
    const vector<std::pair<float,float> >& min_max_irr,
    list<FrameSetPtr>* frame_set_buffer) {
  int frame_idx = 0;
  for (list<FrameSetPtr>::iterator frame_iter = frame_set_buffer->begin();
       frame_iter != frame_set_buffer->end();
       ++frame_iter, ++frame_idx) {
    FrameSetPtr input = *frame_iter;

    // Get view on video frame.
    VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image0;
    frame->ImageView(&image0);

    // Allocate new output frame.
    VideoFrame* irr_frame = new VideoFrame(frame_width_,
                                           frame_height_,
                                           3,
                                           irr_width_step_,
                                           frame->pts());

    input->push_back(shared_ptr<DataFrame>(irr_frame));

    // Result.
    IplImage irr_view;
    irr_frame->ImageView(&irr_view);
    cv::Mat hdr(irr_view.height, irr_view.width, CV_32FC3);
    IplImage ihdr = hdr;

    // Smooth.
    cv::Mat tmpd(&image0, true);    // Deep copy.

    // TODO: Joint bilateral here <- noise suppression
    // Mat tmps(irr_view.height, irr_view.width, tmpd.type());
    // tmps = tmpd.clone();
    // bilateralFilter(tmps, tmpd, 5, 20, 10); // 5,?,10

    IplImage image = tmpd;
    cv::Mat out_of_bound_mat = cv::Mat::zeros(hdr.rows, hdr.cols, CV_32FC1);

    // live preview
    float tmo_tone = (float)exposure_ / 10.0f;
    float tmo_sval = _tmo_sval / 10.0f;

    cv::Mat temp;
    tmpd.convertTo(temp, CV_8UC3);
    cv::imshow("raw", temp);

    // Apply model and exposure correction.
    #pragma omp parallel for
    for (int i = 0; i < frame_height_; ++i) {
      const uchar* src_ptr = RowPtr<uchar>(&image, i);
      float* hdr_ptr = RowPtr<float>(&ihdr, i);
      uchar* dst_ptr = RowPtr<uchar>(&irr_view, i);
      float* bnd_ptr = out_of_bound_mat.ptr<float>(i);
      for (int j = 0; j < frame_width_; ++j, src_ptr += 3, dst_ptr += 3, hdr_ptr += 3) {
        float out_of_bound = 0;
        for (int c = 0; c < 3; ++c) {

          if (src_ptr[c] < UNDEREXPOSED_THRESH || src_ptr[c] > OVEREXPOSED_THRESH) {
            ++out_of_bound;
          }

          // Map channel to irradiance.
          float src_clamped = clamp(src_ptr[c], UNDEREXPOSED_THRESH, OVEREXPOSED_THRESH);
          hdr_ptr[c] = emor_models[c]->MapToIrradiance(src_clamped, frame_idx);
        }

        if (markup_outofbound_ && out_of_bound > 0) {
          bnd_ptr[j] = out_of_bound;
        }

      }
    }

    // get irr image
    hdr = cv::Mat(&ihdr, true);  // Deep copy.

    // tonemap
    hdr = runHDR(hdr, _tmo_mode, tmo_sval, tmo_tone, min_max_irr);

    // output
    #pragma omp parallel for
    for (int i = 0; i < frame_height_; ++i) {
      uchar* dst_ptr = RowPtr<uchar>(&irr_view, i);
      float* hdr_ptr = hdr.ptr<float>(i);
      float* bnd_ptr = out_of_bound_mat.ptr<float>(i);

      for (int j = 0; j < frame_width_; ++j, hdr_ptr += 3, dst_ptr += 3) {
        // markup
        if (markup_outofbound_ && bnd_ptr[j] > 0) {
          const float out_of_bound = bnd_ptr[j]/3;
          hdr_ptr[0] = (1.0f - out_of_bound) * (float)hdr_ptr[0] + out_of_bound * 102;
          hdr_ptr[1] = (1.0f - out_of_bound) * (float)hdr_ptr[1] + out_of_bound * 51;
          hdr_ptr[2] = (1.0f - out_of_bound) * (float)hdr_ptr[2] + out_of_bound * 51;
        }

        dst_ptr[0] = hdr_ptr[0];
        dst_ptr[1] = hdr_ptr[1];
        dst_ptr[2] = hdr_ptr[2];

      }
    } // output

    // preview
    hdr.convertTo(temp, CV_8UC3);
    cv::imshow("out", temp);
    cv::waitKey(5);

  }   // end frame loop.
}

void IrradianceEstimator::CalibrateAndOutputFrames(bool first_clip_set,
                                                   bool flush,
                                                   list<FrameSetPtr>* output) {
  // Solve for model.
  vector<shared_ptr<MixedEMORModel> > emor_models(3);
  for (int c = 0; c < 3; ++c) {
    int keyframe_spacing = fixed_model_estimation_ ? frame_set_buffer_.size() - 1
                                                   : keyframe_spacing_;
    LOG(INFO) << "Frame buffer size: " << frame_set_buffer_.size();
    emor_models[c].reset(new MixedEMORModel(num_emor_models_,
                                            keyframe_spacing,
                                            frame_set_buffer_.size(),
                                            fixed_model_estimation_,
                                            upper_bound_constraint_));
  }

  if (!first_clip_set) {
    // Set constraints from previous clip.
    ASSURE_LOG(overlap_log_exposure_diffs_.size() > 0);
    // Constraint model.
    for (int c = 0; c < 3; ++c) {
      emor_models[c]->SetConstraintModel(prev_models_[c].get());

      // Constrain 50% of the overlap (half key_frame_spacing) to agree with previous
      // exposures.
      for (int i = 0; i < overlap_log_exposure_diffs_[c].size() / 2; ++i) {
        emor_models[c]->SetLogExpDiff(i, overlap_log_exposure_diffs_[c][i]);
      }
    }

    // Normalize computed frame indicies to start at zero.
    for (int c = 0; c < 3; ++c) {
      int frame_idx = 0;
      for (FrameIntMatchesList::iterator frame_matches =
               frame_int_matches_lists_[c].begin();
           frame_matches != frame_int_matches_lists_[c].end();
           ++frame_matches, ++frame_idx) {
        for (FrameIntMatches::iterator pair_match = frame_matches->begin();
             pair_match != frame_matches->end();
             ++pair_match) {
          pair_match->frame_idx = frame_idx;
        }
      }
    }
  }

  // Solve.
  for (int c = 0; c < 3; ++c) {
    emor_models[c]->FitEmorModel(frame_int_matches_lists_[c],
                                 adjaceny_scale_,
                                 bayes_prior_scale_);
  }

  // Determine overlap start.
  int overlap_start = frame_set_buffer_.size();   // no overlap by default.
  if (!flush) {
    // One clip as overlap.
    overlap_start = frames_per_clip_ - 1 - keyframe_spacing_;
  }

  // We need to set exposure from last clip as start for consistency.
  if (!first_clip_set) {
    for (int c = 0; c < 3; ++c) {
      emor_models[c]->SetBaseExposure(prev_clip_base_exposure_[c]);
    }
  } else {
    // Compute color twist using gray world assumption.
    // Map for first frame 128 of each color channel.
    float mean_c[3] = {0, 0, 0};
    const float denom = 1.0f / overlap_start;
    for (int c = 0; c < 3; ++c) {
      for (int frame = 0; frame < overlap_start; ++frame) {
        mean_c[c] += emor_models[c]->MapToIrradiance(128, frame) * denom;
      }

      LOG(INFO) << "BEFORE mean_c[" << c << "]: " << mean_c[c];
    }

    float mean = (mean_c[0] + mean_c[1] + mean_c[2]) / 3;
    for (int c = 0; c < 3; ++c) {
      // Map to mean.
      const float ratio = mean / mean_c[c];
      emor_models[c]->SetBaseExposure(-log(ratio));
      mean_c[c] = emor_models[c]->MapToIrradiance(128, 0);
      LOG(INFO) << "AFTER mean_c[" << c << "]: " << mean_c[c];
    }
  }

   if (!flush) {
    // Save exposure and model constraints if not last clip.
    overlap_log_exposure_diffs_.clear();
    overlap_log_exposure_diffs_.resize(3);
    prev_clip_base_exposure_ = vector<float>(3);

    for (int c = 0; c < 3; ++c) {
      for (int i = overlap_start; i < emor_models[c]->NumLogExposures() - 1; ++i) {
        overlap_log_exposure_diffs_[c].push_back(
          emor_models[c]->GetLogExposure(i + 1) - emor_models[c]->GetLogExposure(i));
      }
      prev_clip_base_exposure_[c] = emor_models[c]->GetLogExposure(overlap_start);
    }

    prev_models_ = emor_models;
  }

  // Find range (only for first clip set for consistency).
  if (first_clip_set) {
    min_max_irr_.resize(3);
    for (int c = 0; c < 3; ++c) {
      min_max_irr_[c] = std::make_pair(1e6f, 0.f);
    }

    for (int c = 0; c < 3; ++c) {
      for (int frame = 0; frame < overlap_start; ++frame) {
         float irr_min = emor_models[c]->MapToIrradiance(
            frame_int_matches_lists_[c][frame][0].percentile_10, frame);
         float irr_max = emor_models[c]->MapToIrradiance(
            frame_int_matches_lists_[c][frame][0].percentile_90, frame);
         min_max_irr_[c].first  = std::min(irr_min, min_max_irr_[c].first);
         min_max_irr_[c].second = std::max(irr_max, min_max_irr_[c].second);
      }
    }
  }

  // min/max scaling
  float ir_min = std::min(std::min(min_max_irr_[0].first, min_max_irr_[1].first),
                          min_max_irr_[2].first);
  float ir_max = std::max(std::max(min_max_irr_[0].second, min_max_irr_[1].second),
                          min_max_irr_[2].second);

  const string video_base = video_file_.substr(0, video_file_.find_last_of("."));
  for (int c = 0; c < 3; ++c) {
    // Only truncate for first clip set.
    std::ofstream exp_ofs((video_base + ".exp" +
                           lexical_cast<string>(c) + ".txt").c_str(),
                           first_clip_set ? std::ios_base::out : std::ios_base::app);
    if (!first_clip_set) {
      exp_ofs << " ";
    }

    emor_models[c]->WriteExposureToStream(overlap_start, exp_ofs);

    std::ofstream response_ofs((video_base + ".response" +
                                lexical_cast<string>(c) + ".txt").c_str(),
                               first_clip_set ? std::ios_base::out : std::ios_base::app);
    if (!first_clip_set) {
      response_ofs << "\n";
    }

    emor_models[c]->WriteResponsesToStream(flush ? emor_models[c]->NumMixtures()
                                           : emor_models[c]->NumMixtures() - 1,
                                           response_ofs);
    response_ofs.close();

    std::ofstream envelope_ofs((video_base + ".env" +
                                lexical_cast<string>(c) + ".txt").c_str(),
                               first_clip_set ? std::ios_base::out
                               : std::ios_base::app);
    if (!first_clip_set) {
      envelope_ofs << "\n";
    }

    const float gain = 255.0f / (ir_max - ir_min);
    emor_models[c]->WriteEnvelopesToStream(overlap_start,
                                           gain,
                                           -ir_min * gain,
                                           envelope_ofs);
    envelope_ofs.close();
  }

  int frames_output = 0;


  // preview
  if (live_mode_) {
    int finalize = 0;
    cv::namedWindow("in" , CV_WINDOW_KEEPRATIO);
    cvMoveWindow("in" ,  10, 10);
    cv::namedWindow("raw", CV_WINDOW_KEEPRATIO);
    cvMoveWindow("raw",  210, 10);
    cv::namedWindow("out", CV_WINDOW_KEEPRATIO);
    cvMoveWindow("out",  410, 10);
    cv::namedWindow("trackbar", CV_WINDOW_KEEPRATIO);
    cv::imshow("trackbar", cv::Mat::zeros(150, 250, CV_8UC1));

    cv::createTrackbar("exposure",   "trackbar", &exposure_, 30, 0);
    cv::createTrackbar("adjustment", "trackbar", &_tmo_sval, 20, 0);
    cv::createTrackbar("tmo-mode",   "trackbar", &_tmo_mode,  4, 0);
    cv::createTrackbar("finalize",   "trackbar", &finalize,   1, 0);
    cv::imshow("trackbar", cv::Mat::zeros(150, 250, CV_8UC1));
    cvMoveWindow("trackbar", 10, 410);

    do {
      // Local copy.
      list<FrameSetPtr> frame_set_buffer(frame_set_buffer_.size());
      std::copy(frame_set_buffer_.begin(),
                frame_set_buffer_.end(),
                frame_set_buffer.begin());

      cv::waitKey(10);
      LOG(INFO) << "live-preview... ";
      LOG(INFO) << "... mode:" << _tmo_mode
                << " exposure:" << (float)exposure_ / 10.0f
                << " adjustment:" << _tmo_sval / 10.0f;

      HDRMapping(emor_models, min_max_irr_, &frame_set_buffer);

      if (finalize) {
        // Push local copy to output.
        output->insert(output->end(), frame_set_buffer.begin(), frame_set_buffer.end());
        frames_output = frame_set_buffer.size();
      }
    } while (finalize == 0);
  } else {
    list<FrameSetPtr> frame_set_buffer;
    list<FrameSetPtr>::const_iterator list_iter = frame_set_buffer_.begin();
    for (int f = 0; f < overlap_start; ++f, ++list_iter) {
      frame_set_buffer.push_back(*list_iter);
    }

    HDRMapping(emor_models, min_max_irr_, &frame_set_buffer);
    frames_output = frame_set_buffer.size();

    output->insert(output->end(), frame_set_buffer.begin(), frame_set_buffer.end());
  }

  // Remove from buffer FrameSets that were pushed to output.
  for (int f = 0; f < frames_output; ++f) {
    frame_set_buffer_.pop_front();
  }

  if (flush) {
    for (int c = 0; c < 3; ++c) {
      frame_int_matches_lists_[c].clear();
    }
  } else {
    for (int c = 0; c < 3; ++c) {
      frame_int_matches_lists_[c].erase(frame_int_matches_lists_[c].begin(),
                                        frame_int_matches_lists_[c].begin() +
                                        frames_output);
    }
  }

  LOG(INFO) << "Wrote " << frames_output << " frames.";
  ++output_clips_;
}

void IrradianceEstimator::PostProcess(list<FrameSetPtr>* append) {
  if (unit_exhausted_ == false) {
    unit_exhausted_ = true;

    if (sliding_window_mode_ > 0) {
      RunAsSlidingWindowMode();
    } else {
      CalibrateAndOutputFrames(output_clips_ == 0, true, append);
    }
  }
}

FrameAligner::FrameAligner(const std::string& video_stream_name,
                           const std::string& alignment_stream_name,
                           const std::string& registration_stream_name)
    : video_stream_name_(video_stream_name),
      alignment_stream_name_(alignment_stream_name),
      registration_stream_name_(registration_stream_name) {
}

bool FrameAligner::OpenStreams(StreamSet* set) {
  video_stream_idx_ = FindStreamIdx(video_stream_name_, set);

  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  const VideoStream* vid_stream =
    dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();
  frame_number_ = 0;
  downscale_ = .5f;
  scaled_width_ = frame_width_ * downscale_;
  scaled_height_ = frame_height_ * downscale_;

  gray_input_.reset(new cv::Mat(frame_height_, frame_width_, CV_8U));
  curr_gray_frame_.reset(new cv::Mat(scaled_height_, scaled_width_, CV_8U));
  prev_gray_frame_.reset(new cv::Mat(scaled_height_, scaled_width_, CV_8U));

  set->push_back(shared_ptr<DataStream>(new DataStream(alignment_stream_name_)));

  canvas_width_ = frame_width_ * 2;
  canvas_height_ = frame_height_ * 2;
  canvas_origin_x_ = (canvas_width_ + frame_width_) / 2;
  canvas_origin_y_ = (canvas_height_ + frame_height_) / 2;

  if (!registration_stream_name_.empty()) {
    canvas_frame_.reset(new cv::Mat(canvas_height_, canvas_width_, CV_8UC3));
    const float canvas_step = canvas_frame_->step[0];
    shared_ptr<VideoStream> canvas_stream(new VideoStream(canvas_width_,
                                                          canvas_height_,
                                                          canvas_step,
                                                          vid_stream->fps(),
                                                          vid_stream->frame_count(),
                                                          PIXEL_FORMAT_BGR24,
                                                          registration_stream_name_));
    set->push_back(canvas_stream);
  }

  curr_transform_.reset(new cv::Mat(3, 3, CV_32F));
  cv::setIdentity(*curr_transform_);

  return true;
}

void FrameAligner::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  cv::Mat image;
  frame->MatView(&image);

  // Convert to gray and resize.
  cv::cvtColor(image, *gray_input_, CV_BGR2GRAY);
  cv::resize(*gray_input_, *curr_gray_frame_, curr_gray_frame_->size());

  cv::Mat transform(3, 3, CV_32F);
  cv::setIdentity(transform);

  if (frame_number_ > 0) {
    AlignFrames(*curr_gray_frame_, *prev_gray_frame_, &transform);
  }

  input->push_back(shared_ptr<DataFrame>(new ValueFrame<cv::Mat>(transform)));
  *curr_transform_ = *curr_transform_ * transform;

  if (!registration_stream_name_.empty()) {
    cv::Mat origin_trans(3, 3, CV_32F);
    cv::setIdentity(origin_trans);
    origin_trans.at<float>(0, 2) = canvas_origin_x_;
    origin_trans.at<float>(1, 2) = canvas_origin_y_;

    cv::Mat canvas_warp(3, 3, CV_32F);
    canvas_warp = origin_trans * *curr_transform_;
    cv::warpAffine(image,
                   *canvas_frame_,
                   canvas_warp.rowRange(0, 2),
                   canvas_frame_->size(),
                   cv::INTER_CUBIC,
                   cv::BORDER_TRANSPARENT);
    shared_ptr<VideoFrame> canvas_frame(new VideoFrame(canvas_width_,
                                                       canvas_height_,
                                                       3,
                                                       canvas_frame_->step[0],
                                                       frame->pts()));
    cv::Mat canvas_image;
    canvas_frame->MatView(&canvas_image);
    canvas_frame_->copyTo(canvas_image);
    input->push_back(canvas_frame);
  }

  output->push_back(input);
  curr_gray_frame_.swap(prev_gray_frame_);
  ++frame_number_;
}

void FrameAligner::AlignFrames(const cv::Mat& curr_frame,
                               const cv::Mat& prev_frame,
                               cv::Mat* transform) {

}

bool GainCorrector::OpenStreams(StreamSet* set) {
 video_stream_idx_ = FindStreamIdx(video_stream_name_, set);

  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  const VideoStream* vid_stream =
    dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();
  frame_number_ = 0;
  num_tracked_ = 1;

  downscale_ = .5f;
  scaled_width_ = frame_width_ * downscale_;
  scaled_height_ = frame_height_ * downscale_;

  curr_image_.reset(new cv::Mat(scaled_height_, scaled_width_, CV_8UC3));
  prev_gray_.reset(new cv::Mat(scaled_height_, scaled_width_, CV_8U));
  curr_gray_.reset(new cv::Mat(scaled_height_, scaled_width_, CV_8U));
  cv::namedWindow("test");
  return true;
}

void GainCorrector::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  cv::Mat image;
  frame->MatView(&image);

  cv::resize(image, *curr_image_, curr_image_->size());

  if (frame_number_ == 0) {
    curr_gain[0] = 0.95;
    curr_gain[1] = 0.95;
    curr_gain[2] = 0.95;
    curr_bias[0] = log(0.9);
    curr_bias[1] = log(0.9);
    curr_bias[2] = log(0.9);
  } else {
    const int num_tracks = prev_images_.size();
    vector<vector<double> > gain_changes(num_tracks, vector<double>(3));
    vector<vector<double> > bias_changes(num_tracks, vector<double>(3));

    for (int k = 0; k < num_tracks; ++k) {
      GainFromImages(*curr_image_, *prev_images_[k], &gain_changes[k], &bias_changes[k]);

      // Divide already computed gain change to yield change w.r.t. prev. image.
      if (k != num_tracks - 1) {
        for (int l = k; l < prev_gain_changes_.size(); ++l) {
          for (int c = 0; c < 3; ++c) {
            gain_changes[k][c] /= prev_gain_changes_[l][c];
            bias_changes[k][c] = (bias_changes[k][c] - prev_bias_changes_[k][c]) /
                prev_gain_changes_[l][c];
          }
        }
      }
    }

    // Gain change is geom. mean across computed changes.
    vector<double> gain_change(3, 1.0);
    vector<double> bias_change(3, 0.0);
    for (int k = 0; k < num_tracks; ++k) {
       for (int c = 0; c < 3; ++c) {
         gain_change[c] *= gain_changes[k][c];
         bias_change[c] += bias_changes[k][c];
       }
    }

    for (int c = 0; c < 3; ++c) {
      gain_change[c] = pow(gain_change[c], 1.0 / num_tracks);
      bias_change[c] /= num_tracks;
    }

    prev_gain_changes_.push_back(gain_change);
    if (prev_gain_changes_.size() == num_tracked_) {
      prev_gain_changes_.erase(prev_gain_changes_.begin());
    }

    prev_bias_changes_.push_back(bias_change);
    if (prev_bias_changes_.size() == num_tracked_) {
      prev_bias_changes_.erase(prev_bias_changes_.begin());
    }

    for (int c = 0; c < 3; ++c) {
      curr_bias[c] += curr_gain[c] * bias_change[c];
      curr_gain[c] *= gain_change[c];
    }
  }

  vector<cv::Mat> lplanes;
  cv::split(image, lplanes);
  cv::Mat tmp, conv32f;
  for (int c = 0; c < 3; ++c) {
    lplanes[c].convertTo(conv32f, CV_32F);
    cv::log(conv32f, tmp);
    cv::exp(tmp * curr_gain[c] + curr_bias[c], conv32f);
    conv32f.convertTo(lplanes[c], CV_8U);
  }

  cv::merge(lplanes, image);

  if (prev_images_.size() == num_tracked_) {
    prev_images_.erase(prev_images_.begin());
  }

  prev_images_.push_back(shared_ptr<cv::Mat>(new cv::Mat(curr_image_->clone())));

  output->push_back(input);
  ++frame_number_;

}

void GainCorrector::GainFromImages(const cv::Mat& curr_image,
                                   const cv::Mat& prev_image,
                                   vector<double>* gain_change,
                                   vector<double>* bias_change) {
  // Reduce view along row and columns.
  cv::cvtColor(curr_image, *curr_gray_, CV_BGR2GRAY);
  cv::cvtColor(prev_image, *prev_gray_, CV_BGR2GRAY);

  cv::Mat curr_row_sum(curr_gray_->rows, 1, CV_32F);
  cv::Mat curr_col_sum(curr_gray_->cols, 1, CV_32F);
  cv::reduce(*curr_gray_, curr_row_sum, 1, CV_REDUCE_SUM, CV_32F);
  cv::reduce(curr_gray_->t(), curr_col_sum, 1, CV_REDUCE_SUM, CV_32F);

  cv::Mat prev_row_sum(prev_gray_->rows, 1, CV_32F);
  cv::Mat prev_col_sum(prev_gray_->cols, 1, CV_32F);
  cv::reduce(*prev_gray_, prev_row_sum, 1, CV_REDUCE_SUM, CV_32F);
  cv::reduce(prev_gray_->t(), prev_col_sum, 1, CV_REDUCE_SUM, CV_32F);

  curr_row_sum = curr_row_sum.t() / (255 * curr_row_sum.cols);
  curr_col_sum = curr_col_sum.t() / (255 * curr_col_sum.cols);
  prev_row_sum = prev_row_sum.t() / (255 * prev_row_sum.cols);
  prev_col_sum = prev_col_sum.t() / (255 * prev_col_sum.cols);

  const float range = 0.05f;
  curr_row_sum = curr_row_sum.colRange(
      cv::Range(range * scaled_height_, (1.0f - range)  * scaled_height_));
  curr_col_sum = curr_col_sum.colRange(
      cv::Range(range * scaled_width_, (1.0f - range)  * scaled_width_));

  cv::Mat corr_row(1, prev_row_sum.cols - curr_row_sum.cols + 1, CV_32F);
  cv::Mat corr_col(1, prev_col_sum.cols - curr_col_sum.cols + 1, CV_32F);

  matchTemplate(prev_row_sum, curr_row_sum, corr_row, CV_TM_CCORR_NORMED);
  matchTemplate(prev_col_sum, curr_col_sum, corr_col, CV_TM_CCORR_NORMED);

  double row_max, col_max;
  cv::Point row_loc, col_loc;
  cv::minMaxLoc(corr_row, NULL, &row_max, NULL, &row_loc);
  cv::minMaxLoc(corr_col, NULL, &col_max, NULL, &col_loc);

  const int dy = row_loc.x - range * scaled_height_;
  const int dx = col_loc.x - range * scaled_width_;

  cv::Mat curr_view(curr_image,
                    cv::Range(range * scaled_height_, (1.0f - range) * scaled_height_),
                    cv::Range(range * scaled_width_, (1.0f - range) * scaled_width_));

  cv::Mat prev_view(prev_image,
                    cv::Range(row_loc.x, row_loc.x + curr_view.rows),
                    cv::Range(col_loc.x, col_loc.x + curr_view.cols));

  // Only use those values properly exposed in both images.
  cv::Mat color_mask = (curr_view >= 5) &
                       (curr_view <= 250) &
                       (prev_view >= 5) &
                       (prev_view <= 250);
  vector<cv::Mat> mask_planes(3);
  cv::split(color_mask, mask_planes);
  vector<cv::Mat> curr_planes(3);
  cv::split(curr_view, curr_planes);
  vector<cv::Mat> prev_planes(3);
  cv::split(prev_view, prev_planes);

  ASSURE_LOG(gain_change->size() == 3);
  LOG(INFO) << "Frame: " << frame_number_;
  for (int c = 0; c < 3; ++c) {
    // x denotes current and y previous.
    // Need to find gain, m and b such that y = mx + b;
    const int rows = curr_planes[c].rows;
    const int cols = curr_planes[c].cols;

    const int divs = 5;
    const float dy = (float)rows / divs;
    const float dx = (float)cols / divs;

    vector<double> xs;
    vector<double> ys;
    for (int i = 0; i < divs; ++i) {
      for (int j = 0; j < divs; ++j) {
        cv::Mat curr_block (curr_planes[c], cv::Range(dy * i, dy * (i + 1)),
                                            cv::Range(dx * j, dx * (j + 1)));
        cv::Mat prev_block (prev_planes[c], cv::Range(dy * i, dy * (i + 1)),
                                            cv::Range(dx * j, dx * (j + 1)));
        cv::Mat mask_block (mask_planes[c], cv::Range(dy * i, dy * (i + 1)),
                                            cv::Range(dx * j, dx * (j + 1)));

        cv::Scalar prev_mean = cv::mean(prev_block, mask_block);
        cv::Scalar curr_mean = cv::mean(curr_block, mask_block);
        cv::Scalar block_mean = cv::mean(mask_block);
        double block_ratio = block_mean[0] / 255;
        double log_x_val = log(1e-6 + curr_mean[0]);
        double log_y_val = log(1e-6 + prev_mean[0]);
        xs.push_back(log_x_val);
        ys.push_back(log_y_val);
      }
    }

    const int totals = xs.size();
    vector<double> irls_weights(totals, 1.0);
    for (int r = 0; r < 20; ++r) {
      double weight_sum = 0;
      double sum_x = 0;
      double sum_y = 0;
      double sum_xx = 0;
      double sum_xy = 0;
      double sum_yy = 0;
      for (int k = 0; k < totals; ++k) {
        const double w = irls_weights[k];
        const double log_x_val = xs[k];
        const double log_y_val = ys[k];
        sum_x += log_x_val * w;
        sum_y += log_y_val * w;
        sum_xx += log_x_val * log_x_val * w;
        sum_xy += log_x_val * log_y_val * w;
        sum_yy += log_y_val * log_y_val * w;
        weight_sum += w;
      }


/*    if (totals == 0) {
      (*bias_change)[c] = 0;
      (*gain_change)[c] = 1;
      continue;
    }
*/
      double mean_x = sum_x / weight_sum;
      double mean_y = sum_y / weight_sum;

      (*gain_change)[c] = (sum_xy - weight_sum * mean_x * mean_y) /
          (sum_xx - weight_sum * mean_x * mean_x);
      (*bias_change)[c] = (sum_y - sum_x * (*gain_change)[c]) / weight_sum;

      if (r + 1 < 20) {
        // Evaluate error.
        for (int k = 0; k < totals; ++k) {
          const double err = fabs((*gain_change)[c] * xs[k] + (*bias_change)[c] - ys[k]);
          irls_weights[k] = 1.0 / (1e-10 + err);
        }
      }
    } // end irls.
  } // end colors.
}

}  // namespace VideoFramework.
