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
* Module      : CommonLib
* Class       : none
* Language    : C++/CUDA
* Description : Implementation of common math functionality
*
* Author     : Manuel Werlberger
* EMail      : werlberger@icg.tugraz.at
*
*/

#ifndef COMMONLIB_MATH_H
#define COMMONLIB_MATH_H

#if !defined __CUDACC__ || !defined WIN32
#include "cutil_math.h"

// inline size_t max(size_t a, size_t b)
// {
//   return a > b ? a : b;
// }

inline unsigned long max(unsigned long a, unsigned long b)
{
  return a > b ? a : b;
}

// inline size_t min(size_t a, size_t b)
// {
//   return a < b ? a : b;
// }

inline unsigned long min(unsigned long a, unsigned long b)
{
  return a < b ? a : b;
}

#endif // __CUDACC__ || WIN32
#endif // COMMONLIB_MATH_H
