/*
 *  nnf.h
 *  ImageFilterLib
 *
 *  Created by Matthias Grundmann on 5/24/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

// Computation of nearest neighborfield
// Based on: Connelly Barnes, Eli Shechtman, Adam Finkelstein, Dan B Goldman
//           PatchMatch: A Randomized Correspondence Algorithm for Structural Image Editing,
//           SIGGRAPH 2009

#ifndef NNF_H__
#define NNF_H__

#include <opencv2/core/core_c.h>
#include <vector>
using std::vector;
#include <boost/array.hpp>

namespace ImageFilter {
  // Computes the nearest neighbor field for image src w.r.t the image ref.
  // Output is an offset field of size [src.width() - 2 * patch_rad, src.height() - 2 * patch_rad]
  // The offset field is specified by
  // a) offset: for each location i in src the closest patch in ref is ref[i + offset[i]]
  //      (must be matrix of type int)
  // b) ptr: for each location i in src the closest patch in ref is ptr[i]

  // Pass zero to any output that is not required by the caller.
  // Maximum supported patch size is 80 pixels.
  // Beyond that overflow in internal structures occurs.
  void ComputeNNF(const IplImage* src,
                  const IplImage* ref,
                  int patch_rad,
                  int iterations,
                  CvMat* offset,
                  vector<const uchar*>* offset_vec,
                  CvMat* min_values);

  // Variant of the above function for feature descriptor representation (e.g. SIFT).
  // Output offset is required, min_values is optional.
  typedef boost::array<float, 128> SIFT_DESC;
  void ComputeNNF(const int src_width,
                  const int src_height,
                  const vector<SIFT_DESC>& src,
                  const int ref_width,
                  const int ref_height,
                  const vector<SIFT_DESC>& ref,
                  const int patch_rad,
                  int iterations,
                  CvMat* offset,
                  CvMat* min_values = 0);

  // Creates Image from nearest neighor
  void AveragePatches(const IplImage* ref,
                      const vector<const uchar*>& offset,
                      int patch_rad,
                      CvMat* min_values,
                      IplImage* output);

}

#endif  // NNF_H__
