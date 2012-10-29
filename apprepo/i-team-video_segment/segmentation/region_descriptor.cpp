/*
 *  region_descriptor.cpp
 *  segmentation
 *
 *  Created by Matthias Grundmann on 10/4/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "region_descriptor.h"

#include <iomanip>

#include <opencv2/imgproc/imgproc.hpp>

#include "assert_log.h"
#include "image_util.h"
#include "segmentation_common.h"
#include "segmentation_util.h"

namespace Segment {

using namespace ImageFilter;

RegionDescriptorExtractor::~RegionDescriptorExtractor() { }
RegionDescriptor::~RegionDescriptor() { }

AppearanceExtractor::AppearanceExtractor(int luminance_bins,
                                         int color_bins,
                                         const cv::Mat& rgb_frame)
    : luminance_bins_(luminance_bins), color_bins_(color_bins) {
  lab_frame_.reset(new cv::Mat(rgb_frame.rows,
                               rgb_frame.cols,
                               CV_8UC3));

  cv::cvtColor(rgb_frame, *lab_frame_, CV_BGR2Lab);

  cv::Mat lab_float_frame(rgb_frame.rows, rgb_frame.cols, CV_32FC3);
  cv::Mat log_frame(rgb_frame.rows, rgb_frame.cols, CV_32FC3);
  lab_frame_->convertTo(lab_float_frame, CV_32FC3);
  cv::log(lab_float_frame, log_frame);

  // Compute average per-channel intensity.
  cv::Scalar mean = cv::mean(log_frame);
  log_average_values_.resize(3);
  for (int c = 0; c < 3; ++c) {
    log_average_values_[c] = mean[c];
  }
}

AppearanceDescriptor3D::AppearanceDescriptor3D(int luminance_bins,
                                               int color_bins) : is_populated_(false) {
  color_histogram_.reset(new ColorHistogram(luminance_bins, color_bins));
}

void AppearanceDescriptor3D::AddFeatures(const Rasterization& raster,
                                         const RegionDescriptorExtractor& extractor,
                                         int frame_number) {
  const AppearanceExtractor& appearance_extractor =
      dynamic_cast<const AppearanceExtractor&>(extractor);
  for (RepeatedPtrField<ScanInterval>::const_iterator scan_inter =
           raster.scan_inter().begin();
       scan_inter != raster.scan_inter().end();
       ++scan_inter) {
    const uchar* interval_ptr =
        appearance_extractor.LabFrame().ptr<uchar>(scan_inter->y()) +
        3 * scan_inter->left_x();

    for (int x = scan_inter->left_x();
         x <= scan_inter->right_x();
         ++x, interval_ptr += 3) {
      color_histogram_->AddPixelInterpolated(interval_ptr);
    }
  }
}

float AppearanceDescriptor3D::RegionDistance(const RegionDescriptor& rhs_uncast) const {
  const AppearanceDescriptor3D& rhs =
      dynamic_cast<const AppearanceDescriptor3D&>(rhs_uncast);
  return color_histogram_->ChiSquareDist(*rhs.color_histogram_);
}

void AppearanceDescriptor3D::PopulatingDescriptorFinished() {
  if (!is_populated_) {
    color_histogram_->ConvertToSparse();
    color_histogram_->NormalizeToOne();
    is_populated_ = true;
  }
}

void AppearanceDescriptor3D::MergeWithDescriptor(const RegionDescriptor& rhs_uncast) {
  const AppearanceDescriptor3D& rhs =
      dynamic_cast<const AppearanceDescriptor3D&>(rhs_uncast);
  color_histogram_->MergeWithHistogram(*rhs.color_histogram_);
}

RegionDescriptor* AppearanceDescriptor3D::Clone() const {
  AppearanceDescriptor3D* clone =
      new AppearanceDescriptor3D(color_histogram_->LuminanceBins(),
                                 color_histogram_->ColorBins());
  clone->color_histogram_.reset(new ColorHistogram(*color_histogram_));
  return clone;
}

void AppearanceDescriptor3D::AddToRegionFeatures(RegionFeatures* descriptor) const {
}

WindowedAppearanceDescriptor::WindowedAppearanceDescriptor(int window_size,
                                                           int luminance_bins,
                                                           int color_bins)
    : window_size_(window_size),
      compare_radius_(2),
      luminance_bins_(luminance_bins),
      color_bins_(color_bins),
      is_populated_(false) {
}

void WindowedAppearanceDescriptor::AddFeatures(const Rasterization& raster,
                                               const RegionDescriptorExtractor& extractor,
                                               int frame_number) {
  const AppearanceExtractor& appearance_extractor =
      dynamic_cast<const AppearanceExtractor&>(extractor);

  const int window_idx = frame_number / window_size_;

  // Store in separate histograms.
  if (window_idx >= window_.size()) {
    // Compact all previous ones.
    for (WindowHistograms::iterator win = window_.begin(); win != window_.end(); ++win) {
      if (*win != NULL && !(*win)->color_hist.IsSparse()) {
        (*win)->color_hist.ConvertToSparse();
        (*win)->color_hist.NormalizeToOne();
      }
    }

    window_.resize(window_idx + 1);
  }

  if (window_[window_idx] == NULL) {
    window_[window_idx].reset(
        new CalibratedHistogram(luminance_bins_,
                                color_bins_,
                                appearance_extractor.LogAverageValues()));
  }

  ColorHistogram& histogram = window_[window_idx]->color_hist;
  //default_gain[0] = 4.77562;
  //default_gain[1] = 4.87388;
  //default_gain[2] = 4.79157;
  vector<float> gain_change = GetGainChange(window_[window_idx]->log_average,
                                            appearance_extractor.LogAverageValues());

  for (RepeatedPtrField<ScanInterval>::const_iterator scan_inter =
           raster.scan_inter().begin();
       scan_inter != raster.scan_inter().end();
       ++scan_inter) {
    const uchar* interval_ptr =
        appearance_extractor.LabFrame().ptr<uchar>(scan_inter->y()) +
        3 * scan_inter->left_x();

    for (int x = scan_inter->left_x();
         x <= scan_inter->right_x();
         ++x, interval_ptr += 3) {
      histogram.AddPixelValuesInterpolated(
          std::min(255.f, (float)interval_ptr[0] * gain_change[0]),
          std::min(255.f, (float)interval_ptr[1] * gain_change[1]),
          std::min(255.f, (float)interval_ptr[2] * gain_change[2]));
    }
  }
}

float WindowedAppearanceDescriptor::RegionDistance(
    const RegionDescriptor& rhs_uncast) const {
  const WindowedAppearanceDescriptor& rhs =
      dynamic_cast<const WindowedAppearanceDescriptor&>(rhs_uncast);

  // Compute lower and upper bound of intersection.
  const int hist_max_idx = std::min(window_.size(), rhs.window_.size()) - 1;

  if (hist_max_idx < 0) {
    LOG(FATAL) << "Empty histograms shouldn't happen.";
  }

  int hist_min_idx = 0;
  while (window_[hist_min_idx] == NULL || rhs.window_[hist_min_idx] == NULL) {
    ++hist_min_idx;
    if (hist_min_idx >= hist_max_idx) {
      break;
    }
  }

  // Determine corresponding key-frame positions.
  const int lhs_key = std::max(0, hist_min_idx - compare_radius_);
  const int rhs_key = hist_max_idx + compare_radius_;

  double dist_sum = 0;
  double weight_sum = 0;

  for (int key = lhs_key; key <= rhs_key; ++key) {
    if (key >= window_.size() || window_[key] == NULL) {
      continue;
    }

    const ColorHistogram& curr_hist = window_[key]->color_hist;

    // Traverse matching windows.
    double match_weight_sum = 0;
    double match_sum = 0;
    for (int match_key = key - compare_radius_;
         match_key <= key + compare_radius_;
         ++match_key) {
      if (match_key < lhs_key || match_key > rhs_key) {
        continue;
      }

      // Quick test if there is anything to match in this interval.
      if (match_key >= rhs.window_.size() || rhs.window_[match_key] == NULL) {
        continue;
      }

      // Build windowed gain corrected histogram.
      vector<float> gain = GetGainChange(window_[key]->log_average,
                                         rhs.window_[match_key]->log_average);

      ColorHistogram match_hist(rhs.window_[match_key]->color_hist);//.ScaleHistogram(gain));

      // TODO: Add gaussian weight w.r.t. match distance.
      const double weight = std::min(curr_hist.WeightSum(), match_hist.WeightSum());
      const double dist = curr_hist.ChiSquareDist(match_hist);
      ASSURE_LOG(dist >= 0 && dist < 1.0 + 1e-3) << "Chi-Sq: " << dist;

      match_weight_sum += weight;
      match_sum += weight * dist;
    }  // end matching windows.

    dist_sum += match_sum;
    weight_sum += match_weight_sum;
  }

  if (weight_sum > 0) {
    const double result = dist_sum / weight_sum;
    ASSURE_LOG(result >= 0 && result < 1.0 + 1e-3) << "Distance: " << result;
    return result;
  } else {
    return 0.f;
  }
}

void WindowedAppearanceDescriptor::PopulatingDescriptorFinished() {
  if (!is_populated_) {
    for (WindowHistograms::iterator win = window_.begin(); win != window_.end(); ++win) {
      if (*win != NULL && !(*win)->color_hist.IsSparse()) {
        (*win)->color_hist.ConvertToSparse();
        (*win)->color_hist.NormalizeToOne();
      }
    }
    is_populated_ = true;
  }
}

void WindowedAppearanceDescriptor::MergeWithDescriptor(
    const RegionDescriptor& rhs_uncast) {
  const WindowedAppearanceDescriptor& rhs =
      dynamic_cast<const WindowedAppearanceDescriptor&>(rhs_uncast);

  window_.resize(std::max(window_.size(), rhs.window_.size()));
  for (size_t i = 0; i < rhs.window_.size(); ++i) {
    if (rhs.window_[i]) {
      if (window_[i]) {
        vector<float> gain = GetGainChange(window_[i]->log_average,
                                           rhs.window_[i]->log_average);
        window_[i]->color_hist.MergeWithHistogram(
            rhs.window_[i]->color_hist.ScaleHistogram(gain));
      } else {
        window_[i].reset(new CalibratedHistogram(*rhs.window_[i]));
      }
    }
  }
}

RegionDescriptor* WindowedAppearanceDescriptor::Clone() const {
  WindowedAppearanceDescriptor* clone = new WindowedAppearanceDescriptor(window_size_,
                                                                         luminance_bins_,
                                                                         color_bins_);
  clone->window_.resize(window_.size());

  for (WindowHistograms::const_iterator win = window_.begin();
       win != window_.end();
       ++win) {
    if (*win != NULL) {
      clone->window_[win - window_.begin()].reset(new CalibratedHistogram(**win));
    }
  }

  return clone;
}

void WindowedAppearanceDescriptor::AddToRegionFeatures(RegionFeatures* descriptor) const {
}

vector<float> WindowedAppearanceDescriptor::GetGainChange(
    const vector<double>& anchor_log,
    const vector<double>& frame_log_average) {
  vector<float> gain_change(3);
  for (int c = 0; c < 3; ++c) {
    gain_change[c] = exp(anchor_log[c] - frame_log_average[c]);
  }

  return gain_change;
}

FlowExtractor::FlowExtractor(int flow_bins,
                             const cv::Mat& flow_x,
                             const cv::Mat& flow_y) : flow_bins_(flow_bins),
                                                      valid_flow_(true),
                                                      flow_x_(flow_x),
                                                      flow_y_(flow_y) {
}

FlowExtractor::FlowExtractor(int flow_bins) : flow_bins_(flow_bins), valid_flow_(false) {

}

FlowDescriptor::FlowDescriptor(int flow_bins) : flow_bins_(flow_bins),
                                                is_populated_(false) {
}

void FlowDescriptor::AddFeatures(const Rasterization& raster,
                                 const RegionDescriptorExtractor& extractor,
                                 int frame_num) {
  const FlowExtractor& flow_extractor =
      dynamic_cast<const FlowExtractor&>(extractor);

  if (!flow_extractor.HasValidFlow()) {
    return;
  }

  if (frame_num >= flow_histograms_.size()) {
    flow_histograms_.resize(frame_num + 1);
  }

  if (flow_histograms_[frame_num] == NULL) {
    flow_histograms_[frame_num].reset(new VectorHistogram(flow_bins_));
  }

  for (RepeatedPtrField<ScanInterval>::const_iterator scan_inter =
           raster.scan_inter().begin();
       scan_inter != raster.scan_inter().end();
       ++scan_inter) {
    const float* flow_x_ptr =
        flow_extractor.FlowX().ptr<float>(scan_inter->y()) + scan_inter->left_x();
    const float* flow_y_ptr =
        flow_extractor.FlowY().ptr<float>(scan_inter->y()) + scan_inter->left_x();
    for (int x = scan_inter->left_x();
         x <= scan_inter->right_x();
         ++x, ++flow_x_ptr, ++flow_y_ptr) {
      flow_histograms_[frame_num]->AddVector(*flow_x_ptr, *flow_y_ptr);
    }
  }
}

float FlowDescriptor::RegionDistance(const RegionDescriptor& rhs_uncast) const {
  const FlowDescriptor& rhs = dynamic_cast<const FlowDescriptor&>(rhs_uncast);

  int max_idx = std::min(flow_histograms_.size(), rhs.flow_histograms_.size());
  float sum = 0;
  float sum_weight = 0;
  for (int i = 0; i < max_idx; ++i) {
    if (flow_histograms_[i] != NULL &&
        rhs.flow_histograms_[i] != NULL) {
      float weight = std::min(flow_histograms_[i]->NumVectors(),
                              rhs.flow_histograms_[i]->NumVectors());

      sum += flow_histograms_[i]->ChiSquareDist(*rhs.flow_histograms_[i]) * weight;
      sum_weight += weight;
    }
  }
  if (sum_weight > 0) {
    return sum / sum_weight;
  } else {
    return 0;
  }
}

void FlowDescriptor::PopulatingDescriptorFinished() {
  if (!is_populated_) {
    for(vector< shared_ptr<VectorHistogram> >::iterator hist_iter =
            flow_histograms_.begin();
        hist_iter != flow_histograms_.end();
        ++hist_iter) {
      if (*hist_iter != NULL) {
        (*hist_iter)->NormalizeToOne();
      }
    }

    is_populated_ = true;
  }
}

void FlowDescriptor::MergeWithDescriptor(const RegionDescriptor& rhs_uncast) {
  const FlowDescriptor& rhs = dynamic_cast<const FlowDescriptor&>(rhs_uncast);

  flow_histograms_.resize(std::max(flow_histograms_.size(),
                                   rhs.flow_histograms_.size()));

  for (size_t i = 0; i < rhs.flow_histograms_.size(); ++i) {
    if (rhs.flow_histograms_[i]) {
      if (flow_histograms_[i]) {
        flow_histograms_[i]->MergeWithHistogram(*rhs.flow_histograms_[i]);
      } else {
        flow_histograms_[i].reset(new VectorHistogram(*rhs.flow_histograms_[i]));
      }
    }
  }
}

RegionDescriptor* FlowDescriptor::Clone() const {
  FlowDescriptor* clone = new FlowDescriptor(flow_bins_);
  clone->flow_histograms_.resize(flow_histograms_.size());

  for (vector<shared_ptr<VectorHistogram> >::const_iterator i = flow_histograms_.begin();
       i != flow_histograms_.end();
       ++i) {
    if (*i) {
      clone->flow_histograms_[i - flow_histograms_.begin()].reset(
          new VectorHistogram(**i));
    }
  }

  return clone;
}

/*

MatchDescriptor::MatchDescriptor() : RegionDescriptor(MATCH_DESCRIPTOR) {

}

void MatchDescriptor::Initialize(int my_id) {
  my_id_ = my_id;
}

void MatchDescriptor::AddMatch(int match_id, float strength) {
  ASSERT_LOG(strength >= 0 && strength <= 1);
  // Convert strength to distance.
  strength = 1.0 - strength;
  strength = std::max(strength, 0.1f);

  MatchTuple new_match = { match_id, strength };

  vector<MatchTuple>::iterator insert_pos = std::lower_bound(matches_.begin(),
                                                             matches_.end(),
                                                             new_match);
  if(insert_pos != matches_.end() &&
     insert_pos->match_id == match_id) {
    ASSURE_LOG(insert_pos->strength == strength)
      << "Match already present in descriptor with different strength.";
  } else {
    matches_.insert(insert_pos, new_match);
  }
}

float MatchDescriptor::RegionDistance(const RegionDescriptor& rhs_uncast) const {
  const MatchDescriptor& rhs = dynamic_cast<const MatchDescriptor&>(rhs_uncast);

  MatchTuple lhs_lookup = { my_id_, 0 };
  vector<MatchTuple>::const_iterator lhs_match_iter =
      std::lower_bound(rhs.matches_.begin(), rhs.matches_.end(), lhs_lookup);

  MatchTuple rhs_lookup = { rhs.my_id_, 0 };
  vector<MatchTuple>::const_iterator rhs_match_iter =
      std::lower_bound(matches_.begin(), matches_.end(), rhs_lookup);

  float strength = 1;
  if (lhs_match_iter != rhs.matches_.end() &&
      lhs_match_iter->match_id == my_id_) {
    strength = lhs_match_iter->strength;
  }

  if (rhs_match_iter != matches_.end() &&
      rhs_match_iter->match_id == rhs.my_id_) {
    if (strength != 1) {
      strength = (strength + rhs_match_iter->strength) * 0.5;
      // LOG(WARNING) << "One sided match found!";
    } else {
      strength = rhs_match_iter->strength;
    }
  } else {
    if (strength != 1) {
    //  LOG(WARNING) << "One sided match found!";
    }
  }

  return strength;
}

void MatchDescriptor::MergeWithDescriptor(const RegionDescriptor& rhs_uncast) {
  const MatchDescriptor& rhs = dynamic_cast<const MatchDescriptor&>(rhs_uncast);

  if (my_id_ != rhs.my_id_) {
    // TODO: Think about this, no real merge! Winner takes it all.
    if (rhs.matches_.size() > matches_.size()) {
      if (matches_.size() > 0) {
        LOG(WARNING) << "Winner takes it all strategy applied.";
      }

      my_id_ = rhs.my_id_;
      matches_ = rhs.matches_;
    }
  }
}

RegionDescriptor* MatchDescriptor::Clone() const {
  return new MatchDescriptor(*this);
}

void MatchDescriptor::OutputToAggregated(AggregatedDescriptor* descriptor) const {
  SegmentationDesc::MatchDescriptor* match =
    descriptor->mutable_match();

  for (vector<MatchTuple>::const_iterator tuple = matches_.begin();
       tuple != matches_.end();
       ++tuple) {
    SegmentationDesc::MatchDescriptor::MatchTuple* add_tuple = match->add_tuple();
    add_tuple->set_match_id(tuple->match_id);
    add_tuple->set_strength(tuple->strength);
  }
}

LBPDescriptor::LBPDescriptor() : RegionDescriptor(LBP_DESCRIPTOR) {

}

void LBPDescriptor::Initialize(int frame_width, int frame_height) {
  frame_width_ = frame_width;
  frame_height_ = frame_height;
  lbp_histograms_.resize(3);
  var_histograms_.resize(3);
  for (int i = 0; i < 3; ++i) {
    lbp_histograms_[i].reset(new VectorHistogram(10));
    var_histograms_[i].reset(new VectorHistogram(16));
  }
}

void LBPDescriptor::AddSamples(const RegionScanlineRep& scanline_rep,
                               const vector<shared_ptr<IplImage> >& lab_inputs) {
  int scan_idx = scanline_rep.top_y;

  for (vector<IntervalList>::const_iterator scanline = scanline_rep.scanline.begin();
       scanline != scanline_rep.scanline.end();
       ++scanline, ++scan_idx) {
    for (int f = 0; f < 3; ++f) {
      const int max_radius = 1 << f;

      // Check if in bounds.
      if (scan_idx < max_radius || scan_idx >= frame_height_ - max_radius) {
        continue;
      }

      const uchar* row_ptr = RowPtr<uchar>(lab_inputs[f], scan_idx);
      for (IntervalList::const_iterator interval = scanline->begin();
           interval != scanline->end();
           ++interval) {
        const uchar* interval_ptr = row_ptr + interval->first;
        for (int x = interval->first; x <= interval->second; ++x, ++interval_ptr) {
          if (x < max_radius || x >= frame_width_ - max_radius) {
            continue;
          }
        }
        AddLBP(interval_ptr, f, lab_inputs[f]->widthStep);
      }
    }
  }
}

void LBPDescriptor::AddLBP(const uchar* lab_ptr, int sample_id, int width_step) {
  const int threshold = 5;
  const int rad = 1 << sample_id;
  int directions[] = { -rad * width_step - rad, -rad * width_step,
                       -rad * width_step + rad, -rad,
                        rad,                     rad * width_step - rad,
                        rad * width_step,        rad * width_step + rad };

  int center_val = *lab_ptr;
  int lbp = 0;
  float sum = 0;
  float sq_sum = 0;

  for (int i = 0; i < 8; ++i) {
    const int sample = (int)lab_ptr[directions[i]];
    int diff = sample - center_val;
    if (diff > threshold) {
      lbp |= (1 << i);
    }

    sum += sample;
    sq_sum += sample * sample;
  }

  // Add to LBP histogram.
  lbp_histograms_[sample_id]->IncrementBin(MapLBP(lbp));

  sq_sum /= 8.f;
  sum /= 8.f;
  // stdev is in 0 .. 128 ( = 8 * 16), usually not higher than 64 though.
  const float stdev = std::sqrt(sq_sum - sum * sum) / 4.f;
  var_histograms_[sample_id]->IncrementBinInterpolated(std::min(stdev, 15.f));
}

void LBPDescriptor::PopulatingDescriptorFinished() {
  for (vector<shared_ptr<VectorHistogram> >::iterator h = lbp_histograms_.begin();
       h != lbp_histograms_.end();
       ++h) {
    (*h)->NormalizeToOne();
  }

  for (vector<shared_ptr<VectorHistogram> >::iterator h = var_histograms_.begin();
       h != var_histograms_.end();
       ++h) {
    (*h)->NormalizeToOne();
  }
}

void LBPDescriptor::MergeWithDescriptor(const RegionDescriptor& rhs_uncast) {
  const LBPDescriptor& rhs = dynamic_cast<const LBPDescriptor&>(rhs_uncast);

  for (size_t i = 0; i < lbp_histograms_.size(); ++i) {
    lbp_histograms_[i]->MergeWithHistogram(*rhs.lbp_histograms_[i]);
    var_histograms_[i]->MergeWithHistogram(*rhs.var_histograms_[i]);
  }
}

RegionDescriptor* LBPDescriptor::Clone() const {
  LBPDescriptor* new_lbp = new LBPDescriptor();
  new_lbp->Initialize(frame_width_, frame_height_);
  for (int i = 0; i < 3; ++i) {
    new_lbp->lbp_histograms_[i].reset(new VectorHistogram(*lbp_histograms_[i]));
    new_lbp->var_histograms_[i].reset(new VectorHistogram(*var_histograms_[i]));
  }

  return new_lbp;
}

void LBPDescriptor::OutputToAggregated(AggregatedDescriptor* descriptor) const {
  for (int t = 0; t < 3; ++t) {
    SegmentationDesc::TextureDescriptor* texture =
        descriptor->add_texture();

    const float* lbp_values = lbp_histograms_[t]->BinValues();
    for (int i = 0; i < lbp_histograms_[t]->NumBins(); ++i, ++lbp_values) {
      texture->add_lbp_entry(*lbp_values);
    }
  }
}

float LBPDescriptor::RegionDistance(const RegionDescriptor& rhs_uncast) const {
  return 1;

  const LBPDescriptor& rhs = dynamic_cast<const LBPDescriptor&>(rhs_uncast);
  float var_dists[3];
  float lbp_dists[3];
  for (int i = 0; i < 3; ++i) {
    var_dists[i] = var_histograms_[0]->ChiSquareDist(*rhs.var_histograms_[0]);
    lbp_dists[i] = lbp_histograms_[0]->ChiSquareDist(*rhs.lbp_histograms_[0]);
  }

  const float var_dist =
      0.2 * var_dists[0] + 0.3 * var_dists[1] + 0.5 * var_dists[2];
  const float lbp_dist =
      0.2 * lbp_dists[0] + 0.3 * lbp_dists[1] + 0.5 * lbp_dists[2];

  return lbp_dist;
}

vector<int> LBPDescriptor::lbp_lut_;

void LBPDescriptor::ComputeLBP_LUT(int bits) {
  lbp_lut_.resize(1 << bits);
  for (int i = 0; i < (1 << bits); ++i) {
    // Determine number of bit changes and number of 1 bits.
    int ones = 0;
    int changes = 0;
    // Shift with last bit copied to first position and xor.
    const int change_mask = i xor ((i | (i & 1) << bits) >> 1);
    for (int j = 0; j < bits; ++j) {
      changes += (change_mask >> j) & 1;
      ones += (i >> j) & 1;
    }

    if (changes <= 2) {
      lbp_lut_[i] = ones;
    } else {
      lbp_lut_[i] = bits + 1;   // Special marker for non-uniform codes
                                // (more than two sign changes);
    }
  }
}
 */
}
