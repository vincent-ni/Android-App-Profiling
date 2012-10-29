/*
 * Copyright (c) ICG. All rights reserved.
 *
 * Institute for Computer Graphics and Vision
 * Graz University of Technology / Austria
 *
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the above copyright notices for more information.
 *
 *
 * Project     : vmgpu
 * Module      : FlowLib
 * Language    : C++/CUDA
 * Description : Constant values for Flow Calculations
 *
 * Author     : Manuel Werlberger
 * EMail      : werlberger@icg.tugraz.at
 *
 */

#ifndef CUDAFLOWLIBCONSTANTS_H
#define CUDAFLOWLIBCONSTANTS_H

#include <cstring>

static const size_t BLOCK_SIZE = 16;
//static const float FLOWLIB_DELTA = 1e-3f;
#define FLOWLIB_DELTA 1e-3f
#define FLOWLIB_DIFFUSION_DELTA 1e-5f
#define FLOWLIB_G_DELTA 1e-2f
static const size_t FLOWLIB_MAX_PYRAMIDAL_LEVELS = 30;

#endif // CUDAFLOWLIBCONSTANTS_H
