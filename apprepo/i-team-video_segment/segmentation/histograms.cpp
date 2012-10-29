/*
 *  histograms.cpp
 *  segmentation
 *
 *  Created by Matthias Grundmann on 10/5/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "histograms.h"

#include <boost/lexical_cast.hpp>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
using boost::shared_ptr;
using boost::tuple;
using boost::get;
using boost::lexical_cast;

#include <cmath>
#include <cstring>

#include "assert_log.h"

namespace Segment {
  // Converts scalar index to 3 dim index.
  class ColorHistogramIndexLUT {
  public:
    ColorHistogramIndexLUT(int bin_x, int bin_y, int bin_z) :
    bin_x_(bin_x),
    bin_y_(bin_y),
    bin_z_(bin_z) {
      const int total_bins = bin_x * bin_y * bin_z;
      lut_.resize(total_bins);
      int idx = 0;
      for (int x = 0; x < bin_x; ++x) {
        for (int y = 0; y < bin_y; ++y) {
          for (int z = 0; z < bin_z; ++z, ++idx) {
            lut_[idx] = boost::make_tuple(x, y, z);
          }
        }
      }
    }

    const tuple<int, int, int>& Ind2Sub(int index) const { return lut_[index]; }

    int get_bin_x() const { return bin_x_; }
    int get_bin_y() const { return bin_y_; }
    int get_bin_z() const { return bin_z_; }

  private:
    const int bin_x_;
    const int bin_y_;
    const int bin_z_;
    vector< tuple<int, int, int> > lut_;
  };

  class ColorHistogramIndexLUTFactory : boost::noncopyable {
  private:
    static ColorHistogramIndexLUTFactory instance_;
    ColorHistogramIndexLUTFactory() {}
  public:
    static ColorHistogramIndexLUTFactory& Instance() { return instance_; }
    const ColorHistogramIndexLUT& GetLUT(int bin_x, int bin_y, int bin_z) {
      // Find table.
      for (int i = 0; i < tables_.size(); ++i) {
        const ColorHistogramIndexLUT* table = tables_[i].get();
        if (table->get_bin_x() == bin_x &&
            table->get_bin_y() == bin_y &&
            table->get_bin_z() == bin_z) {
          return *table;
        }
      }

      // Not found, insert.
      ColorHistogramIndexLUT* new_lut = new ColorHistogramIndexLUT(bin_x, bin_y, bin_z);
      tables_.push_back(shared_ptr<ColorHistogramIndexLUT>(new_lut));
      return *tables_.back();
    }

  private:
    vector<shared_ptr<ColorHistogramIndexLUT> > tables_;
  };

  ColorHistogramIndexLUTFactory ColorHistogramIndexLUTFactory::instance_;

  ColorHistogram::ColorHistogram(int lum_bins, int color_bins, bool sparse)
      : lum_bins_(lum_bins),
        color_bins_(color_bins),
        total_bins_(lum_bins * color_bins * color_bins),
        weight_sum_(0),
        is_sparse_(sparse) {
    if (sparse) {
      // Anticipate 10% load.
      sparse_bins_ = HashBins(total_bins_ / 10);
    } else {
      bins_ = vector<float>(total_bins_, 0);
    }
  }

  ColorHistogram::ColorHistogram(const ColorHistogram& rhs)
      : lum_bins_(rhs.lum_bins_),
        color_bins_(rhs.color_bins_),
        total_bins_(rhs.total_bins_),
        weight_sum_(rhs.weight_sum_) {
    bins_ = rhs.bins_;
    sparse_bins_ = rhs.sparse_bins_;
    is_sparse_ = rhs.is_sparse_;

    for (int c = 0; c < 3; ++c) {
      mean[c] = rhs.mean[c];
      var[c] = rhs.var[c];
    }
  }

  ColorHistogram::~ColorHistogram() {
  }

  void ColorHistogram::AddPixel(const uchar* pixel) {
    // Compute 3D bin position and increment
    const int bin = ((int)pixel[0] * lum_bins_ / 256) * color_bins_ * color_bins_ +
                    ((int)pixel[1] * color_bins_ / 256) * color_bins_ +
                    ((int)pixel[2] * color_bins_ / 256);
    if (is_sparse_) {
      ++sparse_bins_[bin];
    } else {
      ++bins_[bin];
    }

    weight_sum_ += 1;
  }

  void ColorHistogram::AddValueInterpolated(float x_bin,
                                            float y_bin,
                                            float z_bin,
                                            float weight) {
    const int int_x = x_bin;
    const int int_y = y_bin;
    const int int_z = z_bin;

    const float dx = x_bin - (float)int_x;
    const float dy = y_bin - (float)int_y;
    const float dz = z_bin - (float)int_z;

    const int int_x_bins[2] = { int_x, int_x + (dx >= 1e-6) };
    const int int_y_bins[2] = { int_y, int_y + (dy >= 1e-6) };
    const int int_z_bins[2] = { int_z, int_z + (dz >= 1e-6) };

    const float dx_vals[2] = { 1.0f - dx, dx };
    const float dy_vals[2] = { 1.0f - dy, dy };
    const float dz_vals[2] = { 1.0f - dz, dz };

    float value_sum = 0;
    for (int x = 0; x < 2; ++x) {
      const int slice_bin = int_x_bins[x] * color_bins_ * color_bins_;
      for (int y = 0; y < 2; ++y) {
        const int row_bin = slice_bin + int_y_bins[y] * color_bins_;
        for (int z = 0; z < 2; ++z) {
          const int bin = row_bin + int_z_bins[z];
          const float value = dx_vals[x] * dy_vals[y] * dz_vals[z] * weight;
          value_sum += value;
           if (is_sparse_) {
            sparse_bins_[bin] += value;
          } else {
            bins_[bin] += value;
          }
        }
      }
    }

    weight_sum_ += weight;
  }

  void ColorHistogram::AddPixelInterpolated(const uchar* pixel, float weight) {
    AddValueInterpolated((float)pixel[0] / 255.f * (lum_bins_ - 1),
                         (float)pixel[1] / 255.f * (color_bins_ - 1),
                         (float)pixel[2] / 255.f * (color_bins_ - 1),
                         weight);
  }

  void ColorHistogram::AddPixelValuesInterpolated(float lum,
                                                  float color_1,
                                                  float color_2,
                                                  float weight) {
    AddValueInterpolated(lum / 255.f * (lum_bins_ - 1),
                         color_1 / 255.f * (color_bins_ - 1),
                         color_2 / 255.f * (color_bins_ - 1),
                         weight);
  }


  void ColorHistogram::ConvertToSparse() {
    if(IsSparse()) {
      return;
    }

    sparse_bins_ = HashBins(total_bins_ / 10);
    for (int bin_idx = 0; bin_idx < total_bins_; ++bin_idx) {
      const float value = bins_[bin_idx];
      if (value) {
        sparse_bins_[bin_idx] = value;
      }
    }

    // Free memory.
    bins_ = vector<float>();
    is_sparse_ = true;
  }

  void ColorHistogram::MergeWithHistogram(const ColorHistogram& rhs) {
    const float n = weight_sum_ + rhs.weight_sum_;
    if (n == 0) {
      return;
    }

    // Weighted merge.
    const float n_l = weight_sum_ / n;
    const float n_r = rhs.weight_sum_ / n;
    weight_sum_ = n;

    float weighted_bin_sum = 0;
    if (!IsSparse()) {
      ASSURE_LOG(!rhs.IsSparse());
      for (int i = 0; i < total_bins_; ++i) {
        bins_[i] = bins_[i] * n_l + rhs.bins_[i] * n_r;
        weighted_bin_sum += bins_[i];
      }

      // Normalize.
      const float denom = 1.0f / weighted_bin_sum;
      for (vector<float>::iterator bin = bins_.begin(); bin != bins_.end(); ++bin) {
        *bin *= denom;
      }
    } else {
      for (HashBins::iterator bin_iter = sparse_bins_.begin();
           bin_iter != sparse_bins_.end();
           ++bin_iter) {
        HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.find(bin_iter->first);
        if (rhs_bin_iter != rhs.sparse_bins_.end()) {
          bin_iter->second = bin_iter->second * n_l + rhs_bin_iter->second * n_r;
        } else {
          bin_iter->second *= n_l;
        }
        weighted_bin_sum += bin_iter->second;
      }

      // Process rhs bins that we might have missed.
      for (HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.begin();
           rhs_bin_iter != rhs.sparse_bins_.end();
           ++rhs_bin_iter) {
        HashBins::iterator bin_iter = sparse_bins_.find(rhs_bin_iter->first);
        if (bin_iter == sparse_bins_.end()) {
          weighted_bin_sum +=
              (sparse_bins_[rhs_bin_iter->first] = rhs_bin_iter->second * n_r);
        }
      }

      // Normalize.
      const float denom = 1.0f / weighted_bin_sum;
      for (HashBins::iterator bin_iter = sparse_bins_.begin();
           bin_iter != sparse_bins_.end();
           ++bin_iter) {
        bin_iter->second *= denom;
      }
    }
  }

  void ColorHistogram::NormalizeToOne() {
    ASSURE_LOG(IsSparse()) << "Not implemented to dense histograms.";

    if (weight_sum_ == 0) {
      return;
    }

    // Re-evaluate sum, due to rounding errors (rel. error is within 1e-3).
    double sum = 0.0;
    for (HashBins::iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      sum += bin_iter->second;
    }

    weight_sum_ = sum;
    const float denom = 1.0f / weight_sum_;
    for (HashBins::iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      bin_iter->second *= denom;
    }
  }

  ColorHistogram ColorHistogram::ScaleHistogram(const vector<float>& gain) const {
    ASSURE_LOG(IsSparse()) << "Only valid for sparse histograms.";

    const ColorHistogramIndexLUT& lut =
         ColorHistogramIndexLUTFactory::Instance().GetLUT(
             lum_bins_, color_bins_, color_bins_);
    ColorHistogram result(lum_bins_, color_bins_, true);
    for (HashBins::const_iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      const tuple<int, int, int>& idx_3d = lut.Ind2Sub(bin_iter->first);
      const float bin_lum =  std::min(lum_bins_ - 1.f, (float)get<0>(idx_3d) * gain[0]);
      const float bin_col1 = std::min(color_bins_ - 1.f, (float)get<1>(idx_3d) * gain[1]);
      const float bin_col2 = std::min(color_bins_ - 1.f, (float)get<2>(idx_3d) * gain[2]);

      result.AddValueInterpolated(bin_lum, bin_col1, bin_col2, bin_iter->second);
    }

    return result;
  }

  float ColorHistogram::ChiSquareDist(const ColorHistogram& rhs) const {
    double sum = 0;

    if (!IsSparse()) {
      ASSURE_LOG(!rhs.IsSparse());
      for (int i = 0; i < total_bins_; ++i) {
        const double add = bins_[i] + rhs.bins_[i];
        if (add) {
          const double sub = bins_[i] - rhs.bins_[i];
          sum += sub * sub / add;
        }
      }

      return 0.5 * sum;
    }

    // Sparse processing.
    for (HashBins::const_iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.find(bin_iter->first);
      if (rhs_bin_iter != rhs.sparse_bins_.end()) {
        const double add = bin_iter->second + rhs_bin_iter->second;
        if (add) {
          const double sub = bin_iter->second - rhs_bin_iter->second;
          sum += sub * sub / add;
        }
      } else {
        sum += (double)bin_iter->second;
      }
    }

    // Process rhs bins that we might have missed.
    for (HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.begin();
         rhs_bin_iter != rhs.sparse_bins_.end();
         ++rhs_bin_iter) {
      HashBins::const_iterator bin_iter = sparse_bins_.find(rhs_bin_iter->first);
      if (bin_iter == sparse_bins_.end()) {
        sum += (double)rhs_bin_iter->second;
      }
    }

    return 0.5 * sum;
  }

  float ColorHistogram::KLDivergence(const ColorHistogram& rhs) const {
    double sum = 0;
    const double eps = 1e-10;

    if (!IsSparse()) {
      ASSURE_LOG(!rhs.IsSparse());
      for (int i = 0; i < total_bins_; ++i) {
        const double ratio = (bins_[i] + eps) / (rhs.bins_[i] + eps);
        sum += bins_[i] * log(ratio) + rhs.bins_[i] * log(1.0 / ratio);
      }

      return 0.5 * sum;
    }

    // Sparse processing.
    for (HashBins::const_iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.find(bin_iter->first);
      if (rhs_bin_iter != rhs.sparse_bins_.end()) {
        const double ratio = (bin_iter->second + eps) / (rhs_bin_iter->second + eps);
        sum += bin_iter->second * log(ratio) + rhs_bin_iter->second * log(1.0 / ratio);
      } else {
        const double ratio = (bin_iter->second + eps) / eps;
        sum += bin_iter->second * log(ratio);
      }
    }

    // Process rhs bins that we might have missed.
    for (HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.begin();
         rhs_bin_iter != rhs.sparse_bins_.end();
         ++rhs_bin_iter) {
      HashBins::const_iterator bin_iter = sparse_bins_.find(rhs_bin_iter->first);
      if (bin_iter == sparse_bins_.end()) {
        const double ratio = (rhs_bin_iter->second + eps) / eps;
        sum += rhs_bin_iter->second * log(ratio);
      }
    }

    return 0.5 * sum;
  }

  float ColorHistogram::JSDivergence(const ColorHistogram& rhs) const {
    double sum = 0;
    const double eps = 1e-10;

    if (!IsSparse()) {
      ASSURE_LOG(!rhs.IsSparse());
      for (int i = 0; i < total_bins_; ++i) {
        const double mean = (bins_[i] + rhs.bins_[i]) * 0.5;
        const double ratio_l = (bins_[i] + eps) / (mean + eps);
        const double ratio_r = (rhs.bins_[i] + eps) / (mean + eps);
        sum += bins_[i] * log(ratio_l) + rhs.bins_[i] * log(ratio_r);
      }

      return 0.5 * sum;
    }

    // Sparse processing.
    for (HashBins::const_iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.find(bin_iter->first);
      if (rhs_bin_iter != rhs.sparse_bins_.end()) {
        const double mean = (bin_iter->second + rhs_bin_iter->second) * 0.5;
        const double ratio_l = (bin_iter->second + eps) / (mean + eps);
        const double ratio_r = (rhs_bin_iter->second + eps) / (mean + eps);
        sum += bin_iter->second * log(ratio_l) + rhs_bin_iter->second * log(ratio_r);
      } else {
        const double mean = bin_iter->second * 0.5;
        const double ratio_l = (bin_iter->second + eps) / (mean + eps);
        sum += bin_iter->second * log(ratio_l);
      }
    }

    // Process rhs bins that we might have missed.
    for (HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.begin();
         rhs_bin_iter != rhs.sparse_bins_.end();
         ++rhs_bin_iter) {
      HashBins::const_iterator bin_iter = sparse_bins_.find(rhs_bin_iter->first);
      if (bin_iter == sparse_bins_.end()) {
        const double mean = rhs_bin_iter->second * 0.5f;
        const double ratio_r = (rhs_bin_iter->second + eps) / (mean + eps);
        sum += rhs_bin_iter->second * log(ratio_r);
      }
    }

    return 0.5 * sum;
  }

  float ColorHistogram::L2Dist(const ColorHistogram& rhs) const {
    double sum = 0;

    if (!IsSparse()) {
      ASSURE_LOG(!rhs.IsSparse());
      for (int i = 0; i < total_bins_; ++i) {
        const double diff = bins_[i] - rhs.bins_[i];
        sum += diff * diff;
      }
      return std::sqrt(sum);
    }

    // Sparse processing.
    for (HashBins::const_iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.find(bin_iter->first);
      if (rhs_bin_iter != rhs.sparse_bins_.end()) {
        const double diff = bin_iter->second - rhs_bin_iter->second;
        sum += diff * diff;
      } else {
        const double diff = bin_iter->second;
        sum += diff * diff;
      }
    }

    // Process rhs bins that we might have missed.
    for (HashBins::const_iterator rhs_bin_iter = rhs.sparse_bins_.begin();
         rhs_bin_iter != rhs.sparse_bins_.end();
         ++rhs_bin_iter) {
      HashBins::const_iterator bin_iter = sparse_bins_.find(rhs_bin_iter->first);
      if (bin_iter == sparse_bins_.end()) {
        const double diff = rhs_bin_iter->second;
        sum += diff * diff;
      }
    }

    return std::sqrt(sum);
  }

  void ColorHistogram::ComputeMeanAndVariance() {
    ASSURE_LOG(IsSparse()) << "Implemented for sparse histograms only.";

    memset(mean, 0, 3 * sizeof(mean[0]));
    memset(var, 0, 3 * sizeof(var[0]));
    const ColorHistogramIndexLUT& lut =
        ColorHistogramIndexLUTFactory::Instance().GetLUT(lum_bins_,
                                                        color_bins_,
                                                        color_bins_);
    for (HashBins::iterator bin_iter = sparse_bins_.begin();
         bin_iter != sparse_bins_.end();
         ++bin_iter) {
      const tuple<int, int, int>& idx_3d = lut.Ind2Sub(bin_iter->first);
      const float val = bin_iter->second;

      mean[0] += get<0>(idx_3d) * val;
      mean[1] += get<1>(idx_3d) * val;
      mean[2] += get<2>(idx_3d) * val;

      var[0] += get<0>(idx_3d) * get<0>(idx_3d) * val;
      var[1] += get<1>(idx_3d) * get<1>(idx_3d) * val;
      var[2] += get<2>(idx_3d) * get<2>(idx_3d) * val;
    }

    var[0] -= mean[0] * mean[0];
    var[1] -= mean[1] * mean[1];
    var[2] -= mean[2] * mean[2];
  }

  VectorHistogram::VectorHistogram(int angle_bins) : num_bins_(angle_bins),
  num_vectors_(0) {
    bins_.reset(new float[angle_bins]);
    memset(bins_.get(), 0, sizeof(float) * angle_bins);
  }

  VectorHistogram::VectorHistogram(const VectorHistogram& rhs) : num_bins_(rhs.num_bins_),
  num_vectors_(rhs.num_vectors_) {
    bins_.reset(new float[num_bins_]);
    memcpy(bins_.get(), rhs.bins_.get(), sizeof(float) * num_bins_);
  }

  VectorHistogram::~VectorHistogram() {
  }


  void VectorHistogram::AddVector(float x, float y) {
    float magn = hypot(x, y);
    // magn = std::min(magn, 10.f) / 10.f;

    // ensure angle \in (0, 1)
    float angle = atan2(y, x) / (2.0 * M_PI + 1e-4) + 0.5;
    angle *= (float)num_bins_;

    bins_[(int)angle] += magn;
    ++num_vectors_;
  }

  void VectorHistogram::IncrementBin(int bin_num) {
    ++bins_[bin_num];
    ++num_vectors_;
  }

  void VectorHistogram::AddVectorInterpolated(float x, float y) {
    // ensure angle \in (0, 1)
    float angle = atan2(y, x) / (2.0 * M_PI + 1e-4) + 0.5;
    angle *= (float)num_bins_;

    float magn = hypot(x, y);
    // magn = std::min(magn, 10.f) / 10.f;

    float int_angle = floor(angle);

    const float bin_center[3] = { int_angle - 1 + 0.5,
      int_angle + 0.5,
      int_angle + 1 + 0.5 };

    const int bin_idx[3] = { ((int)int_angle - 1 + num_bins_) % num_bins_,
      (int)int_angle,
      ((int)int_angle + 1) % num_bins_ };

    // Loop over.
    for (int i = 0; i < 3; ++i) {
      const float da = fabs(bin_center[i] - angle);
      if (da < 1)
        bins_[bin_idx[i]] += (1 - da) * magn;
    }

    ++num_vectors_;
  }

  void VectorHistogram::IncrementBinInterpolated(float value) {
    const int bin_idx = value;
    const float dx = value - (float)bin_idx;
    bins_[bin_idx] += 1.0f - dx;
    if (bin_idx + 1 < num_bins_) {
      bins_[bin_idx + 1] += dx;
    }

    ++num_vectors_;
  }

  void VectorHistogram::MergeWithHistogram(const VectorHistogram& rhs) {
    float n_l = num_vectors_;
    float n_r = rhs.num_vectors_;
    if (n_l + n_r > 0) {
      float n = 1.0f / (n_l + n_r);

      for (int i = 0; i < num_bins_; ++i) {
        bins_[i] = (bins_[i] * n_l + rhs.bins_[i] * n_r) * n;
      }

      num_vectors_ += rhs.num_vectors_;
      NormalizeToOne();
    }
  }

  float VectorHistogram::ChiSquareDist(const VectorHistogram& rhs) const {
    float* pl = bins_.get();
    float* pr = rhs.bins_.get();

    float add, sub;
    float sum = 0;

    for (int i = 0; i < num_bins_; ++i, ++pl, ++pr) {
      add = *pl + *pr;
      if (add) {
        sub = *pl - *pr;
        sum += sub * sub / add;
      }
    }

    //   std::cout << "VectorHistogram: " << 0.5 * sum << "\n";
    return 0.5 * sum;
  }

  float VectorHistogram::L2Dist(const VectorHistogram& rhs) const {
    float* pl = bins_.get();
    float* pr = rhs.bins_.get();

    float diff;
    float sum = 0;

    for (int i = 0; i < num_bins_; ++i, ++pl, ++pr) {
      diff = *pl - *pr;
      sum += diff * diff;
    }

    return std::sqrt(sum);
  }

  void VectorHistogram::Normalize() {
    if (num_vectors_ > 0) {
      float weight = 1.0f / num_vectors_;
      for (int i = 0; i < num_bins_; ++i) {
        bins_[i] *= weight;
      }
    }
  }

  void VectorHistogram::NormalizeToOne() {
    float sum = 0;
    for (int i = 0; i < num_bins_; ++i) {
      sum += bins_[i];
    }

    if (sum > 0) {
      sum = 1.0 / sum;

      for (int i = 0; i < num_bins_; ++i)
        bins_[i] *= sum;
    }
  }
}
