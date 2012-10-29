/*
 *  histograms.h
 *  segmentation
 *
 *  Created by Matthias Grundmann on 10/5/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef HISTOGRAMS_H__
#define HISTOGRAMS_H__

#include "common.h"

namespace Segment {
  typedef unsigned char uchar;

  // Region's color histogram for Lab color space.
  class ColorHistogram {
  public:
    ColorHistogram(int lum_bins, int color_bins, bool sparse = false);
    ColorHistogram(const ColorHistogram& rhs);

    ~ColorHistogram();

    void AddPixel(const uchar* pix);
    void AddPixelInterpolated(const uchar* pix, float weight = 1.0f);

    // Same as above direct values (within bounds [0, 255], not checked).
    void AddPixelValuesInterpolated(float lum,
                                    float color_1,
                                    float color_2,
                                    float weight = 1.0f);

    // Generalized version, no bound checking is performed.
    void AddValueInterpolated(float x_bin, float y_bin, float z_bin, float weight);

    // Non-reversible operation - frees memory.
    void ConvertToSparse();
    bool IsSparse() const { return is_sparse_; }

    void MergeWithHistogram(const ColorHistogram& rhs);
    void NormalizeToOne();

    // Returns sparse histogram, by scaling each bin location by corresponding
    // gain.
    ColorHistogram ScaleHistogram(const vector<float>& gain) const;

    float ChiSquareDist(const ColorHistogram& rhs) const;
    float KLDivergence(const ColorHistogram& rhs) const;
    float JSDivergence(const ColorHistogram& rhs) const;
    float L2Dist(const ColorHistogram& rhs) const;

    // Should be called on normalized histograms (not checked).
    void ComputeMeanAndVariance();

    int LuminanceBins() const { return lum_bins_; }
    int ColorBins() const { return color_bins_; }
    float WeightSum() const { return weight_sum_; }

    // Returns number of hash bins that are not zero.
    int NumSparseEntries() const { return sparse_bins_.size(); }

    // Call ComputeMeanAndVariance, before querying mean and variance
    float LuminanceMean() const { return mean[0]; }
    float ColorMeanA() const { return mean[1]; }
    float ColorMeanB() const { return mean[2]; }

    // Trace of covariance matrix equals
    // L1 norm of radius of ellipse described by covariance matrix.
    float L1CovarianceRadius() const { return var[0] + var[1] + var[2]; }

  private:
    // Non-assignable.
    ColorHistogram& operator=(const ColorHistogram&);
  private:
    const int lum_bins_;
    const int color_bins_;
    const int total_bins_;
    float weight_sum_;

    float mean[3];
    float var[3];

    bool is_sparse_;

    typedef unordered_map<int, float> HashBins;
    HashBins sparse_bins_;
    vector<float> bins_;
  };

  class VectorHistogram {
  public:
    VectorHistogram(int angle_bins);
    VectorHistogram(const VectorHistogram& rhs);
    ~VectorHistogram();

    void AddVector(float x, float y);
    void IncrementBin(int bin_num);

    // Adds vector with wrap around.
    void AddVectorInterpolated(float x, float y);
    void IncrementBinInterpolated(float bin);
    void MergeWithHistogram(const VectorHistogram& rhs);

    void Normalize();
    void NormalizeToOne();

    float ChiSquareDist(const VectorHistogram& rhs) const;
    float L2Dist(const VectorHistogram& rhs) const;

    int NumVectors() const { return num_vectors_; }
    int NumBins() const { return num_bins_; }
    const float* BinValues() const { return bins_.get(); }

  private:
    scoped_array<float> bins_;
    const int num_bins_;
    int num_vectors_;
  };
}

#endif  // HISTOGRAMS_H__
