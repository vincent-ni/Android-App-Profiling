/*
 *  bilateral_filter_1D.cpp
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 8/9/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "bilateral_filter_1D.h"
#include "assert_log.h"
#define _USE_MATH_DEFINES
#include <cmath>

using namespace std;

namespace ImageFilter {

  void BilateralFilter1D(const std::vector<float>& input, float sigma_space, float sigma_signal,
                         std::vector<float>* output) {
    const int radius = std::ceil(sigma_space * 1.5);
    const int diam = 2*radius + 1;
    
    ASSURE_LOG(input.size() == output->size()) << "BilateralFilter1D: "
        << "Input and output must be of same size.";
    
    // Calculate space weights.
    std::vector<float> space_weights(diam);
    const float space_coeff = -0.5 / (sigma_space * sigma_space);
    
    int space_sz = 0;
    for (int i = -radius; i <= radius; ++i)
        space_weights[space_sz++] = exp(space_coeff * (float)(i*i));
    
    const int sz = input.size();
    vector<float> tmp_input(sz + 2 * radius);
    std::copy(input.begin(), input.end(), tmp_input.begin() + radius);
    
    // Copy border.
    std::copy(input.rbegin(), input.rbegin() + radius, tmp_input.end() - radius);
    std::copy(input.begin(), input.begin() + radius, tmp_input.rend() - radius);
    
    // Apply filter.
    const float signal_coeff = -0.5 / (sigma_signal * sigma_signal);
    
    for (int i = 0; i < sz; ++i) {
      const float center_val = tmp_input[radius+i];
      float val_sum = 0;
      float weight_sum = 0;
      
      for (int k = 0; k < diam; ++k) {
        const float val = tmp_input[i+k];
        const float diff = val - center_val;
        const float weight = space_weights[k] * exp(diff * diff * signal_coeff);
        weight_sum += weight;
        val_sum += val * weight;
      }
      
      output->at(i) = val_sum / weight_sum;
    }
  }
  
}
