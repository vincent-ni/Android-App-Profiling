/*
 *  bilateral_filter_1D.h
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 8/9/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef BILATERAL_FILTER_1D_H__
#define BILATERAL_FILTER_1D_H__

#include <vector>

namespace ImageFilter {

void BilateralFilter1D(const std::vector<float>& input, float sigma_space, float sigma_signal,
                       std::vector<float>* output);

}

#endif // BILATERAL_FILTER_1D_H__