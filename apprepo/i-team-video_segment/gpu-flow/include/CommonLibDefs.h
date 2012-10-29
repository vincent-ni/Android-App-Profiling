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
* Class       : $RCSfile$
* Language    : C++/CUDA
* Description : Implementation of common CUDA functionality
*
* Author     : Santner
* EMail      : santner@icg.tugraz.at
*
*/

#ifndef COMMONLIBDEFS_H
#define COMMONLIBDEFS_H

#include "cudatemplates/devicememorypitched.hpp"
#include "cudatemplates/devicememorylinear.hpp"
#include "cudatemplates/hostmemoryreference.hpp"
#include "cudatemplates/hostmemoryheap.hpp"
#include "cudatemplates/copy.hpp"
#include "vmlib_check_cuda_error.h"

#define COMMONLIB_THROW_ERROR(message) \
{ \
  fprintf(stderr,"\n\nError: " #message "\n"); \
  fprintf(stderr,"  file:       %s\n",__FILE__); \
  fprintf(stderr,"  function:   %s\n",__FUNCTION__); \
  fprintf(stderr,"  line:       %d\n\n",__LINE__); \
  return false; \
}

#define COMMONLIB_CHECK_CUDA_ERROR() VMLIB_CHECK_CUDA_ERROR()

#define THROW_IF_NO_DOUBLE_CARD(major, minor) \
  if (major == 1 && minor < 3) \
    COMMONLIB_THROW_ERROR("Function using double precision can't be invoked from an < 1.3 card")


#endif //COMMONLIBDEFS_H

