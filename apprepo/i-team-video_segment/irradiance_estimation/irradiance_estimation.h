/*
 *  irradiance_estimation.h
 *  irradiance_estimation
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef IRRADIANCE_ESTIMATION_H__
#define IRRADIANCE_ESTIMATION_H__

#include <fstream>
#include <boost/utility.hpp>
#include <ext/hash_map>

using __gnu_cxx::hash_map;
#include "optical_flow_unit.h"
#include "video_unit.h"

namespace cv {
  class Mat;
}

namespace VideoFramework {

typedef OpticalFlowFrame::TrackedFeature Feature;

// Weighted match of intensities values.
struct IntensityMatch {
  IntensityMatch(float int_1_, float int_2_, float weight_) :
    int_1(int_1_), int_2(int_2_), weight(weight_) { }

  // Lexicographic ordering.
  bool operator<(const IntensityMatch& rhs) const {
    return int_1 < rhs.int_1 ||
           (int_1 == rhs.int_1 && int_2 <= rhs.int_2);
  }

  float int_1;
  float int_2;
  float weight;    // derived from comparagram (gaussian weighted
  // sum over small patch).
};

// Models illumination change between two frames I, J according to:
// I = gain * J + bias for normalized intensities in [0, 1].
struct GainBiasModel {
  GainBiasModel() : gain(1.0f), bias(0.f), offset(0.f) { }
  float gain;
  float bias;
  float offset;   // unused.

  // Composition according to *this * rhs.
  GainBiasModel Compose(const GainBiasModel& rhs) {
    GainBiasModel result;
    result.gain = gain * rhs.gain;
    result.bias = gain * rhs.bias + bias;
    return result;
  }

  float ApplyModel(float val) const {
    return gain * val + bias;
  }
};

struct PolyModel {
  float coeffs[4];
  PolyModel() {
    memset(coeffs, 0, sizeof(float) * 4);
    coeffs[0] = 1.f;
  }

  float ApplyModel(float val) const {
    return coeffs[0] * val + coeffs[1] * pow(val, 1.5) +
           coeffs[2] * pow(val, 1 / 1.5) + coeffs[3];
  }
};

// List of matches across intensity range.
typedef vector<IntensityMatch> IntensityMatches;

// List of matches for arbitrary number of color dimensions.
typedef vector<shared_ptr<IntensityMatches> > ColorIntMatches;

// Itensity matches from current frame to matching frames.
struct FramePairIntMatch {
  FramePairIntMatch(int frame_idx_,
                    int rel_idx_,
                    const shared_ptr<IntensityMatches>& matches_,
                    float weight_ = 1.0f) : frame_idx(frame_idx_),
                                            rel_idx(rel_idx_),
                                            matches(matches_),
                                            weight(weight_),
                                            median(128),
                                            percentile_10(15),
                                            percentile_90(240) { }

  int frame_idx;
  int rel_idx;    // Relative index w.r.t. current frame.
  shared_ptr<IntensityMatches> matches;   // List of matches.
  float weight;   // Weight for this pair.

  // Intensity median, 10 and 90 percentile for domain 0 .. 255
  float median;
  float percentile_10;
  float percentile_90;
};

// All matching FramePairIntMatches for a frame.
typedef vector<FramePairIntMatch> FrameIntMatches;

// For each frame, hold list of matching frames intensity matches.
typedef vector<FrameIntMatches> FrameIntMatchesList;

class MixedEMORModel : boost::noncopyable {
public:
  // Fixed key-frame spacing EMOR mixture model using num_basis basis
  // functions created over num_frames. If fixed_model is set to true,
  // all mixtures are constrained to be equal.
  MixedEMORModel(int num_basis,
                 int keyframe_spacing,
                 int num_frames,
                 bool fixed_model,
                 bool upper_bound_constraint);

  void FitEmorModel(const FrameIntMatchesList& frame_pair_matches,
                    float adjaceny_scale = 0.01,
                    float bayes_prior_scale = 0.05);

  float MapToIrradiance(float intensity, int frame) const;
  void WriteResponsesToStream(int num_mixtures,
                              std::ostream& os) const;

  void WriteExposureToStream(int num_frames, std::ostream& os) const;

  // Maps underexposed, overexposed and mean intensity to irradiance. Mapped irradiance
  // is transformed via specified gain / bias model.
  void WriteEnvelopesToStream(int num_frames,
                              float gain,
                              float bias,
                              std::ostream& os) const;

  void SetBaseExposure(float exp);
  float GetLogExposure(int frame) const { return log_exposures_[frame]; }
  int NumLogExposures() const { return log_exposures_.size(); }
  float GetModelCoeff(int model, int coeff) const {
    return model_coeffs_[model][coeff];
  }

  void SetLogExpDiff(int frame, float diff) {
    fixed_log_exp_diff_[frame] = diff;
  }

  int NumBasis() const { return num_basis_; }
  int NumMixtures() const { return num_keyframes_; }
  int NumFrames() const { return num_frames_; }

  // For given frame index, returns corresponding mixtures indices and their corresponding
  // weights. Parameter alphas and mixtures are expected to point to arrays for size 2.
  void GetAlphasAndMixtures(int frame_idx, float* alphas, int* mixtures) const;

  void SetConstraintModel(MixedEMORModel* model);
protected:
  void EvaluateIRLSErrors(
    const FrameIntMatchesList& matches,
    vector<float> local_log_exposures,
    vector<float>* irls_weights);

  // Returns mean_weight applied to rows.
  float SetupSystem(
    const FrameIntMatchesList& matches,
    const vector<float>& irls_weights,
    const vector<float>& log_exp_mean,
    cv::Mat* A_mat,
    cv::Mat* b_mat);

  float SetupSystemConstraint(
    const FrameIntMatchesList& matches,
    const vector<float>& irls_weights,
    const vector<float>& log_exp_mean,
    cv::Mat* A_mat,
    cv::Mat* b_mat);

  void SetEquidistantKeyFrames();
  void SetAdaptiveKeyFrames(const vector<float>& log_exp_mean);
  void ComputeAlphaMixLUT();

protected:
  int num_basis_;
  int keyframe_spacing_;
  int num_frames_;
  int num_keyframes_;            // Number of mixtures.
  bool fixed_model_;
  bool upper_bound_constraint_;

  // Each elem is a 1024 float vector.
  vector<vector<float> > basis_functions_;
  vector<vector<float> > model_coeffs_;
  vector<float> log_exposures_;

  vector<int> keyframe_positions_;

  struct AlphaAndMixture {
    float alpha_1;
    float alpha_2;
    float mixture_1;
    float mixture_2;
  };

  // Lookup table for alphas and mixtures.
  vector<AlphaAndMixture> alpha_mix_lut_;

  // Maps frame idx to fixed log exposure.
  hash_map<int, float> fixed_log_exp_diff_;
  MixedEMORModel* prev_constraint_model_;
};

class IrradianceEstimator : public VideoUnit {
 public:
  // Mixture model estimation, over clip_size * 15 frames using one model as
  // constraint going forward.
  // If fixed_model_estimation is set all models are constrained to be equal.
  IrradianceEstimator(int num_emor_models,
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
                      const std::string& video_file,
                      const std::string& video_stream_name = "VideoStream",
                      const std::string& flow_stream_name = "OpticalFlow");

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

 protected:
  // Maps itensity to irradiance via EmoR models and add HDR tonemapped result to
  // frame_set_buffer.
  void HDRMapping(const vector<shared_ptr<MixedEMORModel> >& emor_models,
                  const vector<std::pair<float,float> >& min_max_irr,
                  list<FrameSetPtr>* frame_set_buffer);

  // Performs DP on comparagram to determine seam from top-left to bottom right.
  // Entries below MATCH_REJECT_THRESH are not deemed as valid matches.
  // Returns list of normalized intensity matches (x to y axis).
  void ComputeBTF(const IplImage* comparagram,
                  IntensityMatches* matches);

  // Returns 3 channel normalized intensity matches from prev and curr images.
  // Images are assumed to be 3 channel 8bit.
  void FindIntensityMatches(const IplImage* prev_image,
                            const IplImage* curr_image,
                            const vector<Feature>& features,
                            const ColorIntMatches& matches,
                            bool viz_comparagram);

  void FitGainBiasModel(const vector<shared_ptr<IntensityMatches> >& matches,
                        vector<GainBiasModel>* models);

  void FitPolyModel(const vector<shared_ptr<IntensityMatches> >& matches,
                    vector<PolyModel>* models);

  void RunAsSlidingWindowMode();

  void CalibrateAndOutputFrames(bool first_clip_set,
                                bool flush,
                                list<FrameSetPtr>* output);

 private:
  int num_emor_models_;
  float min_percentile_;
  float max_percentile_;
  float adjaceny_scale_;
  float bayes_prior_scale_;

  bool fixed_model_estimation_;
  bool upper_bound_constraint_;
  int sliding_window_mode_;
  int clip_size_;
  bool markup_outofbound_;
  float tone_scale_;

  std::string video_stream_name_;
  std::string flow_stream_name_;

  int video_stream_idx_;
  int flow_stream_idx_;

  int frame_width_;
  int frame_height_;
  int frame_num_;
  int irr_width_step_;
  int frames_per_clip_;
  int keyframe_spacing_;

  // Buffer smoothed input frames for intensity match extraction from
  // KLT flow.
  int max_smoothed_frame_buffer_;
  vector<shared_ptr<IplImage> > prev_images_smoothed_;

  // Buffer framesets and intensity matches.
  list<FrameSetPtr> frame_set_buffer_;

  // Intensity matches and mean intensities for the 3 colors.
  FrameIntMatchesList frame_int_matches_lists_[3];

  // ClipSet processing.
  vector<vector<float> > overlap_log_exposure_diffs_;
  vector<float> prev_clip_base_exposure_;
  vector<shared_ptr<MixedEMORModel> > prev_models_;
  int output_clips_;

  bool live_mode_;
  int _finalize;
  int _tmo_mode; // [0-4]
  int _tmo_sval; // gets divided by 10.0
  int exposure_; // gets divided by 10.0

  vector<std::pair<float,float> > min_max_irr_;

  string video_file_;
  bool unit_exhausted_;
};

// Performs affine motion alignment based on correlation.
class FrameAligner : public VideoUnit {
 public:
  FrameAligner(const std::string& video_stream_name = "VideoStream",
               const std::string& alignment_stream_name = "AlignmentStream",
               const std::string& registration_stream_name = "");

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);

 protected:
  void AlignFrames(const cv::Mat& curr_frame,
                   const cv::Mat& prev_frame,
                   cv::Mat* transform);

 private:
  string video_stream_name_;
  int video_stream_idx_;

  string alignment_stream_name_;
  string registration_stream_name_;

  int frame_number_;

  int frame_width_;
  int frame_height_;
  int scaled_width_;
  int scaled_height_;

  int canvas_width_;
  int canvas_height_;
  int canvas_origin_x_;
  int canvas_origin_y_;

  float downscale_;

  shared_ptr<cv::Mat> gray_input_;
  shared_ptr<cv::Mat> curr_gray_frame_;
  shared_ptr<cv::Mat> prev_gray_frame_;
  shared_ptr<cv::Mat> canvas_frame_;

  shared_ptr<cv::Mat> curr_transform_;
};

// Applies Lab based gain correction w.r.t first frame.
class GainCorrector : public VideoUnit {
 public:
  GainCorrector(const std::string& video_stream_name = "VideoStream")
      : video_stream_name_(video_stream_name) { }

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);

protected:
  void GainFromImages(const cv::Mat& curr_image,
                      const cv::Mat& prev_image,
                      vector<double>* gain_change,
                      vector<double>* bias_change);

 protected:
  string video_stream_name_;
  int video_stream_idx_;

  shared_ptr<cv::Mat> curr_image_;

  vector<shared_ptr<cv::Mat> > prev_images_;
  vector<vector<double> > prev_gain_changes_;
  vector<vector<double> > prev_bias_changes_;

  shared_ptr<cv::Mat> prev_gray_;
  shared_ptr<cv::Mat> curr_gray_;
  double curr_gain[3];
  double curr_bias[3];

  int frame_number_;
  int frame_width_;
  int frame_height_;

  int scaled_width_;
  int scaled_height_;
  int num_tracked_;

  float downscale_;
};

}

#endif  // IRRADIANCE_ESTIMATION_H__
