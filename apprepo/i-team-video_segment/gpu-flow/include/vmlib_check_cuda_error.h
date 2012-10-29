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
* Author     : Werlberger
* EMail      : werlberger@icg.tugraz.at
*
*/

#ifndef __VMLIB_CHECK_CUDA_ERROR_WRAPPER_H__
#define __VMLIB_CHECK_CUDA_ERROR_WRAPPER_H__

#define VMLIB_CHECK_CUDA_ERROR()                                    \
  if (cudaError_t err = cudaGetLastError())                         \
  {                                                                 \
    fprintf(stderr,"\n\nCUDAError: %s\n",cudaGetErrorString(err));  \
    fprintf(stderr,"  file:       %s\n",__FILE__);                  \
    fprintf(stderr,"  function:   %s\n",__FUNCTION__);              \
    fprintf(stderr,"  line:       %d\n\n",__LINE__);                \
    return false;                                                   \
  }

#endif //__VMLIB_CUTIL_GL_ERROR_WRAPPER_H__
