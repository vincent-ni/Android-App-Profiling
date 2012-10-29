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

#ifndef COMMONLIB_H
#define COMMONLIB_H

// Windows specific includes
#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#include <time.h>
#else
#include <sys/time.h>
#include <cudatemplates/cuda_gcc43_compat.hpp>
#endif

// CUDA Interface
#include "CommonLibDefs.h"
#include "CommonLibInterface.cuh"
#include "CommonLibMath.h"

#include <cuda.h>
#include <vector_types.h>
//#include <cmath>

///////////////////////////////////////////////////////////////////////////////
// Includes of external implementations
///////////////////////////////////////////////////////////////////////////////
#ifndef __CUDACC__ // TODO needed?
#include "IO/CudaPnmImage.hpp"
#endif

namespace CommonLib{

  ///////////////////////////////////////////////////////////////////////////////
  // COMMON SUPPORT FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////
  //-----------------------------------------------------------------------------
  //! Returns current system time in milliseconds.
  //! @return Current system time in milliseconds
  double getTime();

  //-----------------------------------------------------------------------------
  //! Round a / b to nearest higher integer value.
  //! @param[in] a Numerator
  //! @param[in] b Denominator
  //! @return a / b rounded up
  inline unsigned int divUp(unsigned int a, unsigned int b) {
    return (a % b != 0) ? (a / b + 1) : (a / b);
  }

  //-----------------------------------------------------------------------------
  //! Clamps x between a and b.
  //! @param[in] x Input value
  //! @param[in] a Minimum
  //! @param[in] b Maximum
  //! @return x clamped to range [a, b]
  //! @note \a Datatypes:  \n
  template <typename T, typename U, typename V>
  inline T clamp(T x, U a, V b){
    T temp = (b < x) ? b : x;
    return (a > temp) ? a : temp;
  }

  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  inline bool printToFile(const Cuda::HostMemoryHeap<float, 2> &data, const char *name = "file.txt"){
    FILE * pFile;
    pFile = fopen (name,"w");
    if (pFile == NULL)
      COMMONLIB_THROW_ERROR("Unable to open file");
    for (unsigned int y = 0; y < data.size[1]; y++){
      for (unsigned int x = 0; x < data.size[0]; x++){
        fprintf(pFile, "%3.4f ", data.getBuffer()[y * data.stride[0] + x]);
      }
      fprintf(pFile, "\n");
    }
    fclose (pFile);
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  inline bool printToFile(const Cuda::HostMemoryHeap<int, 2> &data, const char *name = "file.txt"){
    FILE * pFile;
    pFile = fopen (name,"w");
    if (pFile == NULL)
      COMMONLIB_THROW_ERROR("Unable to open file");
    for (unsigned int y = 0; y < data.size[1]; y++){
      for (unsigned int x = 0; x < data.size[0]; x++){
        fprintf(pFile, "%5df ", data.getBuffer()[y * data.stride[0] + x]);
      }
      fprintf(pFile, "\n");
    }
    fclose (pFile);
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional HostMemoryHeap to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  inline bool printToFile(const Cuda::HostMemoryHeap<float, 1> &data, const char *name = "file.txt"){
    FILE * pFile;
    pFile = fopen (name,"w");
    if (pFile == NULL)
      COMMONLIB_THROW_ERROR("Unable to open file");
    for (unsigned int x = 0; x < data.size[0]; x++)
      fprintf(pFile, "%3.4f ", data.getBuffer()[x]);
    fclose (pFile);
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional HostMemoryHeap to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  inline bool printToFile(const Cuda::HostMemoryHeap<int, 1> &data, const char *name = "file.txt"){
    FILE * pFile;
    pFile = fopen (name,"w");
    if (pFile == NULL)
      COMMONLIB_THROW_ERROR("Unable to open file");
    for (unsigned int x = 0; x < data.size[0]; x++)
      fprintf(pFile, "%5df ", data.getBuffer()[x]);
    fclose (pFile);
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional DeviceMemoryPitched to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  template<class T>
  inline bool printToFile(const Cuda::DeviceMemoryPitched<T, 2> &data, const char *name = "file.txt"){
    Cuda::HostMemoryHeap<T, 2> data_copy(data);
    return printToFile(data_copy, name);
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional DeviceMemoryLinear to a file
  // @param[in] data This structure is printed to the file
  // @param[in] name This string names the file.
  template<class T>
  inline bool printToFile(const Cuda::DeviceMemoryLinear<T, 1> &data, const char *name = "file.txt"){
    Cuda::HostMemoryHeap<T, 1> data_copy(data);
    return printToFile(data_copy, name);
  }

  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<float, 2> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d x %d)\n", name, (int)data.size[0], (int)data.size[1]);
    for (unsigned int y = 0; y < data.size[1]; y++){
      for (unsigned int x = 0; x < data.size[0]; x++){
        printf("%3.4f ", data.getBuffer()[y * data.stride[0] + x]);
      }
      printf("\n");
    }
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<float, 1> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d)\n", name, (int)data.size[0]);
    for (unsigned int x = 0; x < data.size[0]; x++)
      printf("%3.4f ", data.getBuffer()[x]);
    printf("\n");
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<unsigned char, 2> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d x %d)\n", name, (int)data.size[0], (int)data.size[1]);
    for (unsigned int y = 0; y < data.size[1]; y++){
      for (unsigned int x = 0; x < data.size[0]; x++){
        printf("%3d ", data.getBuffer()[y * data.stride[0] + x]);
      }
      printf("\n");
    }
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<int, 1> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d x %d)\n", name, (int)data.size[0], (int)data.size[1]);
    for (unsigned int x = 0; x < data.size[0]; x++)
      printf("%3d ", data.getBuffer()[x]);
    printf("\n");
    return true;
  }
    //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<int, 2> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d x %d)\n", name, (int)data.size[0], (int)data.size[1]);
    for (unsigned int y = 0; y < data.size[1]; y++){
      for (unsigned int x = 0; x < data.size[0]; x++){
        printf("%3d ", data.getBuffer()[y * data.stride[0] + x]);
      }
      printf("\n");
    }
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional HostMemoryHeap to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  inline bool printToScreen(const Cuda::HostMemoryHeap<unsigned char, 1> &data, const char *name = ""){
    printf("\nprinting Matrix: %s (%d)\n", name, (int)data.size[0]);
    for (unsigned int x = 0; x < data.size[0]; x++)
      printf("%3d ", data.getBuffer()[x]);
    printf("\n");
    return true;
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional DeviceMemoryLinear to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  template<class T>
  inline bool printToScreen(const Cuda::DeviceMemoryLinear<T,1> &data, const char *name = ""){
    Cuda::HostMemoryHeap<T, 1> data_copy(data);
    return printToScreen(data_copy, name);
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional DeviceMemoryPitched to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  template<class T>
  inline bool printToScreen(const Cuda::DeviceMemoryPitched<T, 2> &data, const char *name = ""){
    Cuda::HostMemoryHeap<T, 2> data_copy(data);
    return printToScreen(data_copy, name);
  }
  //-----------------------------------------------------------------------------
  // Prints a 1-dimensional HostMemoryReference to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  template<class T>
  inline bool printToScreen(const Cuda::HostMemoryReference<T, 1> &data, const char *name = ""){
    Cuda::HostMemoryHeap<T, 1> data_copy(data);
    return printToScreen(data_copy, name);
  }
  //-----------------------------------------------------------------------------
  // Prints a 2-dimensional HostMemoryReference to the console
  // @param[in] data This structure is printed to the console
  // @param[in] name This string names the structure and is printed too.
  template<class T>
  inline bool printToScreen(const Cuda::HostMemoryReference<T, 2> &data, const char *name = ""){
    Cuda::HostMemoryHeap<T, 2> data_copy(data);
    return printToScreen(data_copy, name);
  }

  ///////////////////////////////////////////////////////////////////////////////
  // DEVICE SUPPORT FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Identifys the number of CUDA enabled graphics adapters.
  //! @param[out] number Number of CUDA enables graphics cards
  //! @param[in] verbose If true, the function will print detailed information on every graphics adapter
  //! @return True if call was successful
  inline bool deviceQuery(int *number, bool verbose = 0) {
    return iDeviceQuery(number, verbose);
  }

  //-----------------------------------------------------------------------------
  //! Allows for specifying a certain device for computation.
  //! @param[in] device_id  The device's number
  //! @return True if call was successful
  inline bool selectDevice(unsigned int device_id) {
    return iSelectDevice(device_id);
  }

  //-----------------------------------------------------------------------------
  //! Shows all available graphics adapters in a console dialog and lets the
  //! user choose which one to select.
  //! @return True if call was successful
  inline bool selectDeviceDialog() {
    return iSelectDeviceDialog();
  }

  //-----------------------------------------------------------------------------
  //! Returns the compute capability of the current graphics adapter.
  //! @param[out] major Major revision number
  //! @param[out] minor Minor revision number
  //! @return True if call was successful
  inline bool getCurrentComputeCapability(int *major, int *minor) {
    return iGetCurrentComputeCapability(major, minor);
  }

  //-----------------------------------------------------------------------------
  //! Determines SM13 support. This can be used to determine at runtime whether
  //! a specific graphics card can invoke a function compiled for c.c. 1.3
  //! @return True if library has been compiled with SM13 support.
  inline bool isSupportedSM13() {
    return iIsSupportedSM13();
  }

  //-----------------------------------------------------------------------------
  // setBlockSize
  // Sets block size to one of the given cc parameters depending on the compute
  // capability expressed by major and minor
  //! @param[in] major Major revision number
  //! @param[in] minor Minor revision number
  //! @param[in] cc10 Value to be returned if compute capability 1.0 is detected
  //! @param[in] cc11 Value to be returned if compute capability 1.1 is detected
  //! @param[in] cc12 Value to be returned if compute capability 1.2 is detected
  //! @param[in] cc13 Value to be returned if compute capability 1.3 is detected
  //! @return Block size
  inline unsigned int setBlockSize(int major, int minor, int cc10, int cc11, int cc12, int cc13){
    if (major == 1 && minor == 0)
      return cc10;
    if (major == 1 && minor == 1)
      return cc11;
    if (major == 1 && minor == 2)
      return cc12;
    if (major == 1 && minor == 3)
      return cc13;
    return 0;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // IMAGE STATISTICS FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Fills the given structure with random values between [a,b]
  //! @param data           structure to be filled with randoms
  //! @param a              lower limit for random numbers
  //! @param b              upper limit for random numbers
  //! @return True if call was successful
  template<typename T>
  inline bool randomNumbers(Cuda::DeviceMemory<T, 2>* data,
    const T a, const T b) {
      return iRandomNumbers(data, a, b);
  }
  //-----------------------------------------------------------------------------
  //! Fills the given structure with random values between [a,b]
  //! @param data           structure to be filled with randoms
  //! @param a              lower limit for random numbers
  //! @param b              upper limit for random numbers
  //! @return True if call was successful
  inline bool randomNumbers(Cuda::HostMemory<int, 2>* data,
    const int a, const int b, bool force_gpu = false) {
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<int,2> data_dmp(Cuda::Size<2>(data->size[0], data->size[1]));
        if (!iRandomNumbers(&data_dmp, a, b))
          return false;
        Cuda::copy(*data, data_dmp);
      }
      else {
#ifdef WIN32
        srand((unsigned int)time(NULL));
#else
        struct timeval TV;
        gettimeofday(&TV, NULL);
        const unsigned seedNum = (unsigned) time(NULL) + TV.tv_usec;
        srand(seedNum);
#endif
        for (size_t col = data->region_ofs[0]; col < data->region_size[0]; col++){
          for (size_t row = data->region_ofs[1]; row < data->region_size[1]; row++){
            float elem = (float)rand() / (RAND_MAX + 1.0f);
            data->getBuffer()[row * data->stride[0] + col] = (int)((b-a) * elem + a + 0.5f);
          }
        }
      }
      return true;
  }

  //-----------------------------------------------------------------------------
  //! Fills the given structure with random values between [a,b]
  //! @param data           structure to be filled with randoms
  //! @param a              lower limit for random numbers
  //! @param b              upper limit for random numbers
  //! @return True if call was successful
  inline bool randomNumbers(Cuda::HostMemory<float, 2>* data,
    const float a, const float b, bool force_gpu = false) {
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<float,2> data_dmp(Cuda::Size<2>(data->size[0], data->size[1]));
        if (!iRandomNumbers(&data_dmp, a, b))
          return false;
        Cuda::copy(*data, data_dmp);
      }
      else {
#ifdef WIN32
        srand((unsigned int)time(NULL));
#else
        struct timeval TV;
        gettimeofday(&TV, NULL);
        const unsigned seedNum = (unsigned) time(NULL) + TV.tv_usec;
        srand(seedNum);
#endif
        for (size_t col = data->region_ofs[0]; col < data->region_size[0]; col++){
          for (size_t row = data->region_ofs[1]; row < data->region_size[1]; row++){
            float elem = (float)rand() / (RAND_MAX + 1.0f);
            data->getBuffer()[row * data->stride[0] + col] = (b-a) * elem + a;
          }
        }
      }
      return true;
  }


  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 2D arrays (textures) locate on the device.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI:  \n
  template<class T, typename U>
  inline bool computeRMSE( const Cuda::Array<T, 2> &data_a,
    const Cuda::Array<T, 2> &data_b, U *rmse)
  {
    return iComputeRMSE(data_a, data_b, rmse);
  }
  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 2D device memory.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T, typename U>
  inline bool computeRMSE( const Cuda::DeviceMemory<T, 2> &data_a,
    const Cuda::DeviceMemory<T, 2> &data_b, U *rmse)
  {
    return iComputeRMSE(data_a, data_b, rmse);
  }

  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 3D arrays (textures) locate on the device.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T, typename U>
  inline bool computeRMSE( const Cuda::Array<T, 3> &data_a,
    const Cuda::Array<T, 3> &data_b, U *rmse)
  {
    return iComputeRMSE(data_a, data_b, rmse);
  }
  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 3D device memory.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T, typename U>
  inline bool computeRMSE(const Cuda::DeviceMemory<T, 3> &data_a,
    const Cuda::DeviceMemory<T, 3> &data_b, U *rmse)
  {
    return iComputeRMSE(data_a, data_b, rmse);
  }

  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 2D host memory.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T, typename U>
  inline bool computeRMSE( const Cuda::HostMemory<T, 2> &data_a,
    const Cuda::HostMemory<T, 2> &data_b,
    U *rmse, bool force_gpu = false)
  {
    if (force_gpu) {
      Cuda::Array<T, 2> in_a(data_a);
      Cuda::Array<T, 2> in_b(data_b);
      return iComputeRMSE(in_a, in_b, rmse);
    }
    else {
      int ofs_x = max(data_a.region_ofs[0], data_b.region_ofs[0]);
      int ofs_y = max(data_a.region_ofs[1], data_b.region_ofs[1]);
      int end_x = min(data_a.region_ofs[0]+data_a.region_size[0],
        data_b.region_ofs[0]+data_b.region_size[0]);
      int end_y = min(data_a.region_ofs[1]+data_a.region_size[1],
        data_b.region_ofs[1]+data_b.region_size[1]);
      int width = end_x-ofs_x;
      int height = end_y-ofs_y;
      int pitch = data_a.stride[0];
      int offset = ofs_y*pitch+ofs_x;

      const T *buffer_a = &data_a.getBuffer()[offset];
      const T *buffer_b = &data_b.getBuffer()[offset];
      // compute rmse columnwise
      U *rmse_column = new U[width];
      memset(rmse_column, 0, sizeof(U) * width);
      for (int i = 0; i < width; i++)
      {
        for (int j = 0; j < height; j++)
        {
          int idx = j*pitch + i;
          rmse_column[i] += fabs((U)buffer_a[idx] - buffer_b[idx]);
        }
      }
      // sum up columns
      *rmse = 0;
      for (int i = 0; i < width; i++)
        *rmse += rmse_column[i];
      // divide through number of pixels
      *rmse = *rmse / (width*height);
    }
    return true;
  }

  //-----------------------------------------------------------------------------
  //! Evaluates the root mean squared error between two datasets. This function
  //! uses 3D host memory.
  //! @param[in] data_a First dataset
  //! @param[in] data_b Second dataset
  //! @param[out] rmse Root Mean Squared Error
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T, typename U>
  inline bool computeRMSE( const Cuda::HostMemory<T, 3> &data_a,
    const Cuda::HostMemory<T, 3> &data_b,
    U *rmse, bool force_gpu = false)
  {
    if (force_gpu) {
      Cuda::Array<T, 3> in_a(data_a);
      Cuda::Array<T, 3> in_b(data_b);
      return iComputeRMSE(in_a, in_b, rmse);
    }
    else
    {
      int ofs_x = max(data_a.region_ofs[0], data_b.region_ofs[0]);
      int ofs_y = max(data_a.region_ofs[1], data_b.region_ofs[1]);
      int ofs_z = max(data_a.region_ofs[2], data_b.region_ofs[2]);
      int end_x = min(data_a.region_ofs[0]+data_a.region_size[0],
        data_b.region_ofs[0]+data_b.region_size[0]);
      int end_y = min(data_a.region_ofs[1]+data_a.region_size[1],
        data_b.region_ofs[1]+data_b.region_size[1]);
      int end_z = min(data_a.region_ofs[2]+data_a.region_size[2],
        data_b.region_ofs[2]+data_b.region_size[2]);
      int width = end_x-ofs_x;
      int height = end_y-ofs_y;
      int depth = end_z-ofs_z;
      int pitch = data_a.stride[0];
      int pitchPlane = data_a.stride[1];
      int offset = ofs_z*pitchPlane + ofs_y*pitch + ofs_x;

      const T *buffer_a = &data_a.getBuffer()[offset];
      const T *buffer_b = &data_b.getBuffer()[offset];

      // compute rmse columnwise
      U *rmse_projection = new U[width * height];
      memset(rmse_projection, 0, sizeof(U) * width * height);

      *rmse = 0;
      for (int i = 0; i < width; i++)
      {
        U tempY = 0;
        for (int j = 0; j < height; j++)
        {
          U tempZ = 0;
          for (int k = 0; k < depth; k++)
          {
            int idx = k*pitchPlane + j*pitch + i;
            tempZ += fabs((U)buffer_a[idx] - buffer_b[idx]);
          }
          tempY += tempZ;
        }
        *rmse += tempY;
      }

      // divide through number of voxels
      *rmse = *rmse / (width * height * depth);
    }
    return true;
  }

  //-----------------------------------------------------------------------------
  //! Sums up all elements in an array (using textures) on the device.
  //! @param[in] data Input image
  //! @param[out] sum Computed sum
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: working up to 1.3 \n
  //!       \a ROI: working \n
  template<class T, typename U>
  inline bool computeSum(const Cuda::Array<T, 2> &data, U *sum) {
    return iComputeSum(data, sum);
  }
  //-----------------------------------------------------------------------------
  //! Sums up all elements in an image located on the device.
  //! @param[in] data Input image
  //! @param[out] sum Computed sum
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: working up to 1.3 \n
  //!       \a ROI: working \n
  template<class T, typename U>
  inline bool computeSum(const Cuda::DeviceMemory<T, 2> &data, U *sum) {
    return iComputeSum(data, sum);
  }
  //-----------------------------------------------------------------------------
  //! Sums up all elements in a 3D image located on the device.
  //! @param[in] data Input image
  //! @param[out] sum Computed sum
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: should be ok \n
  //!       \a ROI: working \n
  template<class T, typename U>
  inline bool computeSum(const Cuda::DeviceMemory<T, 3> &data, U *sum) {
    return iComputeSum(data, sum);
  }
  //-----------------------------------------------------------------------------
  //! Sums up all elements in an image located on the host
  //! @param[in] data Input image
  //! @param[out] sum Computed sum
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: working up to 1.3 \n
  //!       \a ROI: working \n
  template<class T, typename U>
  inline bool computeSum(const Cuda::HostMemory<T, 2> &data, U *sum, bool force_gpu = false) {
    if (force_gpu) {
      Cuda::Array<T, 2> data_d(data);
      return iComputeSum(data_d, sum);
    }
    else {
      unsigned int ofs_x = data.region_ofs[0];
      unsigned int ofs_y = data.region_ofs[1];
      unsigned int width = data.region_size[0];
      unsigned int height = data.region_size[1];
      unsigned int pitch = data.stride[0];
      unsigned int offset = ofs_y*pitch+ofs_x;

      const T *buffer = &data.getBuffer()[offset];
      *sum = 0;
      for (unsigned int i = 0; i < width; i++)
      {
        U temp = 0;
        for (unsigned int j = 0; j < height; j++)
          temp += buffer[j * pitch+i];
        *sum += temp;
      }
      return true;
    }
  };
  //-----------------------------------------------------------------------------
  //! Sums up all elements in a 3D image located on the host
  //! @param[in] data Input image
  //! @param[out] sum Computed sum
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: not relevant \n
  //!       \a ROI: working \n
  template<class T, typename U>
  inline bool computeSum(const Cuda::HostMemory<T, 3> &data, U *sum, bool force_gpu = false) {
    if (force_gpu) {
      printf("Not implemented yet!\n"); //! TODO: implement texture based sum in 3D
      //       Cuda::Array<T, 2> data_d(data);
      //       return iComputeSum(data_d, sum);
      return false;
    }
    else {
      unsigned int ofs_x = data.region_ofs[0];
      unsigned int ofs_y = data.region_ofs[1];
      unsigned int ofs_z = data.region_ofs[2];
      unsigned int width = data.region_size[0];
      unsigned int height = data.region_size[1];
      unsigned int depth = data.region_size[2];
      unsigned int pitchY = data.stride[0];
      unsigned int pitchZ = data.stride[1];
      unsigned int offset = ofs_z*pitchZ + ofs_y*pitchY + ofs_x;

      const T *buffer = &data.getBuffer()[offset];

      *sum = 0;
      for (unsigned int i = 0; i < width; i++)
      {
        U tempY = 0;
        for (unsigned int j = 0; j < height; j++)
        {
          U tempZ = 0;
          for (unsigned int k = 0; k < depth; k++)
            tempZ += buffer[k*pitchZ + j*pitchY + i];
          tempY += tempZ;
        }
        *sum += tempY;
      }
      return true;
    }
  };

  //-----------------------------------------------------------------------------
  //! Returns the minimum and maximum of an 2D image located on the device.
  //! @param[in] data Input image
  //! @param[out] minimum Computed minimum
  //! @param[out] maximum Computed maximum
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  template<class T>
  inline bool findMinMax(const Cuda::DeviceMemory<T, 2> &data, T *minimum, T *maximum) {
    return iFindMinMax(data, minimum, maximum);
  }

  //-----------------------------------------------------------------------------
  //! Returns the minimum and maximum of an 3D image located on the device.
  //! @param[in] data Input image
  //! @param[out] minimum Computed minimum
  //! @param[out] maximum Computed maximum
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  template<class T>
  inline bool findMinMax(const Cuda::DeviceMemory<T, 3> &data, T *minimum, T *maximum) {
    return iFindMinMax(data, minimum, maximum);
  }

  //-----------------------------------------------------------------------------
  //! Returns the minimum and maximum of an 2D image located on the host.
  //! @param[in] data Input image
  //! @param[out] minimum Computed minimum
  //! @param[out] maximum Computed maximum
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  template<class T>
  inline bool findMinMax(const Cuda::HostMemory<T, 2> &data, T *minimum, T *maximum, bool force_gpu = false)
  {
    if (force_gpu) {
      Cuda::DeviceMemoryPitched<T, 2> data_d(data);
      return iFindMinMax(data_d, minimum, maximum);
    }
    else {
      unsigned int ofs_x = data.region_ofs[0];
      unsigned int ofs_y = data.region_ofs[1];
      unsigned int width = data.region_size[0];
      unsigned int height = data.region_size[1];
      unsigned int pitch = data.stride[0];
      unsigned int offset = ofs_y*pitch+ofs_x;

      const T *buffer = &data.getBuffer()[offset];
      *minimum = buffer[0];
      *maximum = buffer[0];

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int y = 0; y < height; y++)
        {
          unsigned int center = y*pitch+x;
          if (buffer[center] < *minimum)
            *minimum = buffer[center];
          if (buffer[center] > *maximum)
            *maximum = buffer[center];
        }
      }
      return true;
    }
  };

  //-----------------------------------------------------------------------------
  //! Returns the minimum and maximum of an 3D image located on the host.
  //! @param[in] data Input image
  //! @param[out] minimum Computed minimum
  //! @param[out] maximum Computed maximum
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  template<class T>
  inline bool findMinMax(const Cuda::HostMemory<T, 3> &data, T *minimum, T *maximum,
    bool force_gpu = false)
  {
    if (force_gpu)
    {
      Cuda::DeviceMemoryPitched<T, 3> data_d(data);
      return iFindMinMax(data_d, minimum, maximum);
    }
    else
    {
      unsigned int ofs_x = data.region_ofs[0];
      unsigned int ofs_y = data.region_ofs[1];
      unsigned int ofs_z = data.region_ofs[2];
      unsigned int width = data.region_size[0];
      unsigned int height = data.region_size[1];
      unsigned int depth = data.region_size[2];
      unsigned int pitch = data.stride[0];
      unsigned int pitchPlane = data.stride[1];
      unsigned int offset = ofs_z*pitchPlane + ofs_y*pitch + ofs_x;

      const T *buffer = &data.getBuffer()[offset];
      *minimum = buffer[0];
      *maximum = buffer[0];

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int y = 0; y < height; y++)
        {
          for (unsigned int z = 0; z < depth; z++)
          {
            unsigned int center = z*pitchPlane+y*pitch+x;
            if (buffer[center] < *minimum)
              *minimum = buffer[center];
            if (buffer[center] > *maximum)
              *maximum = buffer[center];
          }
        }
      }
      return true;
    }
  };


  ///////////////////////////////////////////////////////////////////////////////
  // ARITHMETIC FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Computes the absolute difference of two device images.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise absolute difference between input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool absoluteDifference(const Cuda::DeviceMemory<T, 2> *data_a,
    const Cuda::DeviceMemory<T, 2> *data_b,
    Cuda::DeviceMemory<T, 2> *result){
      return iAbsoluteDifference(data_a, data_b, result);
  }
  //-----------------------------------------------------------------------------
  //! Computes the absolute difference of two host images.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise absolute difference between input images
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool absoluteDifference(const Cuda::HostMemory<T, 2> *data_a,
    const Cuda::HostMemory<T, 2> *data_b,
    Cuda::HostMemoryReference<T, 2> *result,
    bool force_gpu = false){
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<T, 2> data_a_d(*data_a);
        Cuda::DeviceMemoryPitched<T, 2> data_b_d(*data_b);
        Cuda::DeviceMemoryPitched<T, 2> result_d(*result);
        if (!iAbsoluteDifference(&data_a_d, &data_b_d, &result_d))
          return false;
        Cuda::copy(*result, result_d);
      }
      else {
        const T* buffer_a = data_a->getBuffer();
        const T* buffer_b = data_b->getBuffer();
        T* buffer_result = result->getBuffer();
        for (unsigned i = 0; i < data_a->size[0] * data_a->size[1]; i++)
          buffer_result[i] = (T)fabs((double)buffer_a[i] - buffer_b[i]);
      }
      return true;
  }

  //-----------------------------------------------------------------------------
  //! Computes the difference of two device images (result = data_a - data_b).
  //! @note Calculations are done on ROI defined from result.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise  difference between input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool difference(const Cuda::DeviceMemory<T, 2> *data_a,
    const Cuda::DeviceMemory<T, 2> *data_b,
    Cuda::DeviceMemory<T, 2> *result){
      return iDifference(data_a, data_b, result);
  }
  //-----------------------------------------------------------------------------
  //! Computes the  difference of two host images.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise  difference between input images
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool difference(const Cuda::HostMemoryReference<T, 2> &data_a,
    const Cuda::HostMemoryReference<T, 2> &data_b,
    Cuda::HostMemory<T, 2> *result, bool force_gpu = false){
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<T, 2> data_a_d(data_a);
        Cuda::DeviceMemoryPitched<T, 2> data_b_d(data_b);
        Cuda::DeviceMemoryPitched<T, 2> result_d(*result);
        if (!iDifference(&data_a_d, &data_b_d, &result_d))
          return false;
        Cuda::copy(*result, result_d);
      }
      else {
        const T* buffer_a = data_a.getBuffer();
        const T* buffer_b = data_b.getBuffer();
        T* buffer_result = result->getBuffer();
        unsigned offset = result->region_ofs[1]*result->stride[0] + result->region_ofs[1];
        for (unsigned i = offset; i < result->region_size[0] * result->region_size[1]; i++)
          buffer_result[i] = (T)fabs((double)buffer_a[i] - buffer_b[i]);
      }
      return true;
  }


  //-----------------------------------------------------------------------------
  //! Adds two images together.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise sum of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool add(Cuda::DeviceMemory<T, 2> *data_a,
    Cuda::DeviceMemory<T, 2> *data_b,
    Cuda::DeviceMemory<T, 2> *result)
  {
    return iAdd(data_a, data_b, result);
  }

  //-----------------------------------------------------------------------------
  //! Adds two images together.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise sum of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool add( const Cuda::HostMemory<T, 2> *data_a,
    const Cuda::HostMemory<T, 2> *data_b,
    Cuda::HostMemory<T, 2> *result, bool force_gpu = false)
  {
    if (force_gpu)
    {
      Cuda::DeviceMemoryPitched<T, 2> data_a_d(*data_a);
      Cuda::DeviceMemoryPitched<T, 2> data_b_d(*data_b);
      Cuda::DeviceMemoryPitched<T, 2> result_d(*result);
      return iAdd(&data_a_d, &data_b_d, &result_d);
    }
    else
    {
      int ofs_x = max(max(data_a->region_ofs[0], data_b->region_ofs[0]), result->region_ofs[0]);
      int ofs_y = max(max(data_a->region_ofs[1], data_b->region_ofs[1]), result->region_ofs[1]);
      int end_x = min( min( data_a->region_ofs[0]+data_a->region_size[0],
        data_b->region_ofs[0]+data_b->region_size[0] ),
        result->region_ofs[0]+result->region_size[0]);
      int end_y = min( min( data_a->region_ofs[1]+data_a->region_size[1],
        data_b->region_ofs[1]+data_b->region_size[1]),
        result->region_ofs[1]+result->region_size[1]);
      int width = end_x-ofs_x;
      int height = end_y-ofs_y;
      int pitch = data_a->stride[0];
      int offset = ofs_y*pitch+ofs_x;

      const T *a_buffer = &data_a->getBuffer()[offset];
      const T *b_buffer = &data_b->getBuffer()[offset];
      T *result_buffer = &result->getBuffer()[offset];

      for (int i = 0; i < width; i++)
      {
        for (int j = 0; j < height; j++)
        {
          int center = i + j*pitch;
          result_buffer[center] = a_buffer[center] + b_buffer[center];
        }
      }

      return true;
    }
  };

  //-----------------------------------------------------------------------------
  //! Adds two images together using weighting coefficients.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[in] weight_a weight for first image
  //! @param[in] weight_b weight for second image
  //! @param[out] result Pixelwise sum of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool addWeighted( Cuda::DeviceMemory<T, 2> *data_a,
    Cuda::DeviceMemory<T, 2> *data_b,
    float weight_a, float weight_b,
    Cuda::DeviceMemory<T, 2> *result)
  {
    return iAddWeighted(data_a, data_b, weight_a, weight_b, result);
  }

  //-----------------------------------------------------------------------------
  //! Adds two images together using weighting coefficients.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[in] weight_a weight for first image
  //! @param[in] weight_b weight for second image
  //! @param[out] result Pixelwise sum of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool addWeighted( const Cuda::HostMemory<T, 2> *data_a,
    const Cuda::HostMemory<T, 2> *data_b,
    float weight_a, float weight_b,
    Cuda::HostMemory<T, 2> *result,
    bool force_gpu = false)
  {
    if (force_gpu)
    {
      Cuda::DeviceMemoryPitched<T, 2> data_a_d(*data_a);
      Cuda::DeviceMemoryPitched<T, 2> data_b_d(*data_b);
      Cuda::DeviceMemoryPitched<T, 2> result_d(*result);
      return iAddWeighted(&data_a_d, &data_b_d, weight_a, weight_b, &result_d);
    }
    else
    {
      int ofs_x = max(max(data_a->region_ofs[0], data_b->region_ofs[0]), result->region_ofs[0]);
      int ofs_y = max(max(data_a->region_ofs[1], data_b->region_ofs[1]), result->region_ofs[1]);
      int end_x = min( min( data_a->region_ofs[0]+data_a->region_size[0],
        data_b->region_ofs[0]+data_b->region_size[0] ),
        result->region_ofs[0]+result->region_size[0]);
      int end_y = min( min( data_a->region_ofs[1]+data_a->region_size[1],
        data_b->region_ofs[1]+data_b->region_size[1]),
        result->region_ofs[1]+result->region_size[1]);
      int width = end_x-ofs_x;
      int height = end_y-ofs_y;
      int pitch = data_a->stride[0];
      int offset = ofs_y*pitch+ofs_x;

      const T *a_buffer = &data_a->getBuffer()[offset];
      const T *b_buffer = &data_b->getBuffer()[offset];
      T *result_buffer = &result->getBuffer()[offset];

      for (int i = 0; i < width; i++)
      {
        for (int j = 0; j < height; j++)
        {
          int center = i + j*pitch;
          result_buffer[center] = (T)(weight_a*(float)a_buffer[center] + weight_b*(float)b_buffer[center]);
        }
      }

      return true;
    }
  };

  //-----------------------------------------------------------------------------
  //! Adds two images together using weighting coefficients.
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[in] weight_a weight for first image
  //! @param[in] weight_b weight for second image
  //! @param[out] result Pixelwise sum of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool addWeighted( Cuda::DeviceMemory<T, 3> *data_a,
    Cuda::DeviceMemory<T, 3> *data_b,
    float weight_a, float weight_b,
    Cuda::DeviceMemory<T, 3> *result)
  {
    return iAddWeighted(data_a, data_b, weight_a, weight_b, result);
  }

  //-----------------------------------------------------------------------------
  //! Multiplies every element of a device image by a constant value.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] factor Multiplication factor
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool multiplyConstant(Cuda::DeviceMemoryPitched<T, 2> *data, float factor) {
    return iMultiplyConstant(data, factor);
  }
  //-----------------------------------------------------------------------------
  //! Multiplies every element of a host image by a constant value.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] factor Multiplication factor
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<class T>
  inline bool multiplyConstant(Cuda::HostMemoryReference<T, 2> *data, float factor, bool force_gpu = false) {
    if (force_gpu){
      Cuda::DeviceMemoryPitched<T, 2> data_d(*data);
      if (!iMultiplyConstant(&data_d, factor))
        return false;
      Cuda::copy(*data, data_d);
    }
    else {
      T *buffer = data->getBuffer();
      for (unsigned int i = 0; i < data->size[0] * data->size[1]; i++)
        buffer[i] = (T) (factor * buffer[i]);
    }
    return true;
  }

  //-----------------------------------------------------------------------------
  //! Multiplies pixel values of two images
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise multiplication of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool multiply( Cuda::DeviceMemory<T, 2> *data_a,
    Cuda::DeviceMemory<T, 2> *data_b,
    Cuda::DeviceMemory<T, 2> *result)
  {
    return iMultiply(data_a, data_b, result);
  }

  //-----------------------------------------------------------------------------
  //! Multiplies pixel values of two images in 3D
  //! @param[in] data_a First input image
  //! @param[in] data_b Second input image
  //! @param[out] result Pixelwise multiplication of input images
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool multiply( Cuda::DeviceMemory<T, 3> *data_a,
    Cuda::DeviceMemory<T, 3> *data_b,
    Cuda::DeviceMemory<T, 3> *result)
  {
    return iMultiply(data_a, data_b, result);
  }


  //-----------------------------------------------------------------------------
  //! Thresholds a device image. Output is 1 where value is greater than threshold.
  //! @param[out] input Input image
  //! @param[out] output Thresholded output image
  //! @param[in] alpha Threshold value
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T, class U>
  inline bool thresholdGreater( const Cuda::DeviceMemory<T, 2> *input,
    Cuda::DeviceMemory<U, 2> *output, float threshold)
  {
    return iThresholdGreater(input, output, threshold);
  }


  //-----------------------------------------------------------------------------
  //! Thresholds a 3D device image. Output is 1 where value is greater than threshold.
  //! @param[out] input Input image
  //! @param[out] output Thresholded output image
  //! @param[in] alpha Threshold value
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T, class U>
  inline bool thresholdGreater( const Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<U, 3> *output, float threshold)
  {
    return iThresholdGreater(input, output, threshold);
  }

  //-----------------------------------------------------------------------------
  //! Normalizes a 2D float image on the device between 0 and 1.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  inline bool normalize(Cuda::DeviceMemory<float, 2> *data)
  {
    return iNormalize(data);
  }
  //-----------------------------------------------------------------------------
  //! Normalizes a 3D float image on the device between 0 and 1.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  inline bool normalize(Cuda::DeviceMemory<float, 3> *data)
  {
    return iNormalize(data);
  }

  //-----------------------------------------------------------------------------
  //! Normalizes a 2D float image on the host between 0 and 1.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  inline bool normalize(Cuda::HostMemory<float, 2> *data, bool force_gpu = false)
  {
    if (force_gpu)
    {
      Cuda::DeviceMemoryPitched<float, 2> data_d(*data);
      if (!iNormalize(&data_d))
        return false;
      Cuda::copy(data_d, *data);
    }
    else
    {
      float minimum, maximum;
      findMinMax(*data, &minimum, &maximum, false);

      unsigned int ofs_x = data->region_ofs[0];
      unsigned int ofs_y = data->region_ofs[1];
      unsigned int width = data->region_size[0];
      unsigned int height = data->region_size[1];
      unsigned int pitch = data->stride[0];
      unsigned int offset = ofs_y*pitch+ofs_x;
      float *buffer = &data->getBuffer()[offset];

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int y = 0; y < height; y++)
        {
          unsigned int center = y*pitch + x;
          buffer[center] = (float)((buffer[center]-minimum)/(maximum-minimum));
        }
      }
    }
    return true;
  }

  //-----------------------------------------------------------------------------
  //! Normalizes a 3D float image on the host between 0 and 1.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: working \n
  inline bool normalize(Cuda::HostMemory<float, 3> *data, bool force_gpu = false)
  {
    if (force_gpu)
    {
      Cuda::DeviceMemoryPitched<float, 3> data_d(*data);
      if (!iNormalize(&data_d))
        return false;
      Cuda::copy(data_d, *data);
    }
    else
    {
      float minimum, maximum;
      findMinMax(*data, &minimum, &maximum, false);

      unsigned int ofs_x = data->region_ofs[0];
      unsigned int ofs_y = data->region_ofs[1];
      unsigned int ofs_z = data->region_ofs[2];
      unsigned int width = data->region_size[0];
      unsigned int height = data->region_size[1];
      unsigned int depth = data->region_size[2];
      unsigned int pitch = data->stride[0];
      unsigned int pitchPlane = data->stride[1];
      unsigned int offset = ofs_z*pitchPlane + ofs_y*pitch + ofs_x;
      float *buffer = &data->getBuffer()[offset];

      for (unsigned int x = 0; x < width; x++)
      {
        for (unsigned int y = 0; y < height; y++)
        {
          for (unsigned int z = 0; z < depth; z++)
          {
            unsigned int center = z*pitchPlane + y*pitch + x;
            buffer[center] = (float)((buffer[center]-minimum)/(maximum-minimum));
          }
        }
      }
    }
    return true;
  }

  //-----------------------------------------------------------------------------
  //! Calculate euclidean distance transformation on input image
  //! 0 is assumed background, everything greater foreground
  //! @param[in] input Input image
  //! @param[out] distance Euclidean distance transformation
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool computeEDT( const Cuda::DeviceMemory<T, 2> *input,
    Cuda::DeviceMemory<float, 2> *distance)
  {
    return iComputeEDT(input, distance);
  }

  //-----------------------------------------------------------------------------
  //! Calculate signed euclidean distance transformation on input image
  //! 0 is assumed background, everything greater foreground
  //! will be negative in foreground regions
  //! @param[in] input Input image
  //! @param[out] distance Euclidean distance transformation
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool computeSEDT( const Cuda::DeviceMemory<T, 2> *input,
    Cuda::DeviceMemory<float, 2> *distance)
  {
    return iComputeSEDT(input, distance);
  }

  //-----------------------------------------------------------------------------
  //! Calculate euclidean distance transformation on 3D image
  //! 0 is assumed background, everything greater foreground
  //! @param[in] input Input image
  //! @param[out] distance Euclidean distance transformation
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool computeEDT( const Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<float, 3> *distance)
  {
    return iComputeEDT(input, distance);
  }

  //-----------------------------------------------------------------------------
  //! Calculate signed euclidean distance transformation on 3D image
  //! 0 is assumed background, everything greater foreground
  //! will be negative in foreground regions
  //! @param[in] input Input image
  //! @param[out] distance Euclidean distance transformation
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool computeSEDT( const Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<float, 3> *distance)
  {
    return iComputeSEDT(input, distance);
  }

  ///////////////////////////////////////////////////////////////////////////////
  // FILTERING FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Convolves a floating point device image with any kernel using constant padding.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] kernel Convolution kernel
  //! @return True if call was successful
  inline bool convolution(Cuda::DeviceMemoryPitched<float, 2> *data,
    const Cuda::DeviceMemoryPitched<float, 2> &kernel) {
      return iConvolution(data, kernel);
  }
  //-----------------------------------------------------------------------------
  //! Convolves a floating point host image with any kernel using constant padding.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] kernel Convolution kernel
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  inline bool convolution(Cuda::HostMemoryReference<float, 2> *data,
    const Cuda::HostMemoryReference<float, 2> &kernel,
    bool force_gpu = false) {
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<float, 2> data_d(*data);
        Cuda::DeviceMemoryPitched<float, 2> kernel_d(kernel);
        return iConvolution(&data_d, kernel_d);
      }
      else{
        // check dimensions
        if (kernel.size[0]%2 != 1 || kernel.size[1]%2 != 1)
          COMMONLIB_THROW_ERROR("Kernel dimensions must be odd");

        // copy input
        float *input = new float[data->size[0] * data->size[1]];
        memcpy(input, data->getBuffer(), data->size[0] * data->size[1] * sizeof(float));

        int half_width = (kernel.size[0] - 1) / 2;
        int half_height =(kernel.size[1] - 1) / 2;

        float sum;
        for (unsigned int y = 0; y < data->size[1]; y++) {
          for (unsigned int x = 0; x < data->size[0]; x++) {
            // for every output pixel
            sum = 0;
            for (unsigned int j = 0; j < kernel.size[1]; j++) {
              for (unsigned int i = 0; i < kernel.size[0]; i++) {
                unsigned int kernel_index = j * kernel.size[0] + i;
                unsigned int input_x = clamp((int)x - half_width + (int)i, 0, (int)data->size[0]-1);
                unsigned int input_y = clamp((int)y - half_height + (int)j, 0, (int)data->size[1]-1);
                unsigned int input_index = input_y * data->size[0] + input_x;
                sum += input[input_index] * kernel.getBuffer()[kernel_index];
              }
            }
            data->getBuffer()[y * data->size[0] + x] = sum;
          }
        }
        return true;
      }
  }

  //-----------------------------------------------------------------------------
  //! Performs gaussian filtering of a floating point 2D device image.
  //!
  //! Attention: The GPU variant of this function computes the gaussian filter
  //!            kernel incrementally resulting in an error of ~0.25% of the
  //!            kernel magnitude.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Dataset to be smoothed
  //! @param[in] sigma Sigma of the convolution kernel
  //! @param[in] kernelsize The size of the convolution kernel. If omitted, a 6-sigma environment is used.
  //! @return True if call was successful
  inline bool gaussianConvolution(Cuda::DeviceMemoryPitched<float, 2> *data,
    float sigma, unsigned int kernelsize = 0) {
      return iGaussianConvolution(data, sigma, kernelsize);
  }

  //-----------------------------------------------------------------------------
  //! Performs gaussian filtering of a floating point 2D device color image.
  //!
  //! Attention: The GPU variant of this function computes the gaussian filter
  //!            kernel incrementally resulting in an error of ~0.25% of the
  //!            kernel magnitude.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Dataset to be smoothed
  //! @param[in] sigma Sigma of the convolution kernel
  //! @param[in] kernelsize The size of the convolution kernel. If omitted, a 6-sigma environment is used.
  //! @return True if call was successful
  inline bool gaussianConvolution(Cuda::DeviceMemoryPitched<float, 3> *data,
    float sigma, unsigned int kernelsize = 0) {
      return iGaussianConvolution(data, sigma, kernelsize);
  }

  //-----------------------------------------------------------------------------
  //! Performs gaussian filtering of a floating point host image.
  //!
  //! Attention: The GPU variant of this function computes the gaussian filter
  //!            kernel incrementally resulting in an error of ~0.25% of the
  //!            kernel magnitude.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Dataset to be smoothed
  //! @param[in] sigma Sigma of the convolution kernel
  //! @param[in] kernelsize The size of the convolution kernel. If omitted, a 6-sigma environment is used.
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  inline bool gaussianConvolution(Cuda::HostMemoryReference<float, 2> *data,
    float sigma, unsigned int kernelsize = 0, bool force_gpu = false) {
      if (force_gpu) {
        Cuda::DeviceMemoryPitched<float, 2> data_d(*data);
        return iGaussianConvolution(&data_d, sigma, kernelsize);
      }
      else {
        if (kernelsize == 0)
          kernelsize = (unsigned int)ceil(sigma * 3) * 2 + 1;
        if (kernelsize%2 != 1)
          COMMONLIB_THROW_ERROR("Kernel dimensions must be odd");

        // compute kernel
        float hxs = kernelsize/2.0f-0.5f;
        float ss2 = sigma*sigma*2.0f;
        float denom = ss2;
        float sum = 0;

        float *kernel = new float[kernelsize];

        for(unsigned int x = 0; x < kernelsize; x++) {
          kernel[x] = (exp(-((x-hxs)*(x-hxs))/ss2))/denom;
          sum += kernel[x];
        }
        for(unsigned int x = 0; x < kernelsize; x++) {
          kernel[x] /= sum;
        }
        // convolve horizontally
        Cuda::HostMemoryReference<float, 2> kernel_x(Cuda::Size<2>(kernelsize, 1), kernel);
        convolution(data, kernel_x);

        // convolve vertically
        Cuda::HostMemoryReference<float, 2> kernel_y(Cuda::Size<2>(1,kernelsize), kernel);
        convolution(data, kernel_y);
      }
      return true;
  }

  //-----------------------------------------------------------------------------
  //! Applies a 3x3 median filter to the input image that is located on the device.
  //!
  //! Attention: The filter cannot be applied inplace.
  //! @param[in] input   Input image on the device
  //! @param[out] output Filtered image on the device
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool medianFilter3x3(const Cuda::DeviceMemoryPitched<T, 2>& input,
    Cuda::DeviceMemoryPitched<T, 2>& output) {
      return iMedianFilter3x3(input, output);
  }
  //-----------------------------------------------------------------------------
  //! Applies a 3x3 median filter to the input image that is located on the host.
  //!
  //! Attention: The filter cannot be applied inplace.
  //! @todo Median on CPU not implemented yet, there's no test for this function!
  //! @param[in] input   Input image on the host
  //! @param[out] output Filtered image on the host
  //! @param[in] force_gpu If true, calculations are done on the GPU
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  inline bool medianFilter3x3(const Cuda::HostMemoryReference<float, 2> &input,
    Cuda::HostMemoryReference<float, 2> &output, bool force_gpu) {
      if (force_gpu) {
        const Cuda::DeviceMemoryPitched<float, 2> input_d(input);
        Cuda::DeviceMemoryPitched<float, 2> output_d(output);
        bool success = iMedianFilter3x3(input_d, output_d);
        if(!success)
          return false;
        copy(output, output_d); // copy output back to host memory
      }
      else { // Median on CPU
        return false;
      }
      return true;
  }

  //-----------------------------------------------------------------------------
  //! Convolves a floating point device image with the Seewig Method.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] lcx Cut-off frequency of the filter in x direction [m]
  //! @param[in] lcy Cut-off frequency of the filter in y direction [m]
  //! @param[in] sx Sampling distance of the filter in x direction [m]
  //! @param[in] sy Sampling distance of the filter in y direction [m]
  //! @return True if call was successful
  inline bool seewigConvolution(Cuda::Array<float, 2> *data,
    const float lcx, const float lcy, const float sx, const float sy) {
      return iSeewigConvolution(data, lcx, lcy, sx, sy);
  }
  //-----------------------------------------------------------------------------
  //! Convolves a floating point device image with the Seewig Method.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] lcx Cut-off frequency of the filter in x direction [m]
  //! @param[in] lcy Cut-off frequency of the filter in y direction [m]
  //! @param[in] sx Sampling distance of the filter in x direction [m]
  //! @param[in] sy Sampling distance of the filter in y direction [m]
  //! @return True if call was successful
  inline bool seewigConvolution(Cuda::DeviceMemoryPitched<float, 2> *data,
    const float lcx, const float lcy, const float sx, const float sy) {
      Cuda::Array<float,2> data_array(*data);
      if (iSeewigConvolution(&data_array, lcx, lcy, sx, sy)) {
        Cuda::copy(*data, data_array);
        return true;
      }
      return false;
  }
  //-----------------------------------------------------------------------------
  //! Convolves a floating point host image with the Seewig Method.
  //! @todo Maybe a separate input and output image would be desireable.
  //! @param[out] data Input and output image
  //! @param[in] lcx Cut-off frequency of the filter in x direction [m]
  //! @param[in] lcy Cut-off frequency of the filter in y direction [m]
  //! @param[in] sx Sampling distance of the filter in x direction [m]
  //! @param[in] sy Sampling distance of the filter in y direction [m]
  //! @param[in] force_gpu Forces the calculation to be done on the GPU
  //! @return True if call was successful
  inline bool seewigConvolution(Cuda::HostMemoryReference<float, 2> *data,
    const float lcx, const float lcy, const float sx, const float sy,
    bool force_gpu = false) {
      if (force_gpu) {
        Cuda::Array<float, 2> data_d(*data);
        return iSeewigConvolution(&data_d, lcx, lcy, sx, sy);
      }
      else{
        // copy input
        float *input = new float[data->size[0] * data->size[1]];
        memcpy(input, data->getBuffer(), data->size[0] * data->size[1] * sizeof(float));

        const float factor = sqrt(log(2.0f))/3.141592653589793f;
        const float constx2 = (factor * lcx/sx) * (factor * lcx/sx);
        const float consty2 = (factor * lcy/sy) * (factor * lcy/sy);

        for (size_t y = 0; y < data->size[1]; ++y){
          for (size_t x = 0; x < data->size[0]; ++x){
            // compute matrices
            // init counters
            float fw = 0;     // sum of weights and input
            float fwx = 0;    // sum of weights, input and x data
            float fwy = 0;    // sum of weights, input and y data
            float w = 0;      // sum of weights
            float wx = 0;     // sum of weights and x data
            float wy = 0;     // sum of weights and y data
            float wx2 = 0;    // sum of weights and squared x data
            float wy2 = 0;    // sum of weights and squared y data
            float wxy = 0;    // sum of weights and x and y data

            // sum up counters
            for (size_t j = 0; j < data->size[1]; ++j) {
              for (size_t i = 0; i < data->size[0]; ++i) {
                int index = j * data->size[0] + i;
                float elem = input[index];

                float weight = 0;
                if (elem < 1e9) { //if pixel is valid
                  int xdat = i - x;
                  int xdat2 = xdat * xdat;
                  int ydat = j - y;
                  int ydat2 = ydat * ydat;

                  weight = exp(- (xdat2/constx2 + ydat2/consty2));

                  // update counting sums
                  float weightelem = weight * elem;
                  fw += weightelem;
                  fwx += weightelem * xdat;
                  fwy += weightelem * ydat;
                  w += weight;
                  wx += weight * xdat;
                  wy += weight * ydat;
                  wx2 += weight * xdat2;
                  wy2 += weight * ydat2;
                  wxy += weight * xdat * ydat;
                }
              }
            }

            // For manual inversion of M compute det(M) first
            float det = w*wx2*wy2 + wx*wxy*wy + wy*wx*wxy -
              wy*wx2*wy - wxy*wxy*w - wy2*wx*wx;

            // Multiply inverse matrix M by vector V (only the first element is necessary)
            data->getBuffer()[y * data->size[0] + x] =
              ((wx2*wy2-wxy*wxy) * fw -
              (wx*wy2-wxy*wy) * fwx +
              (wx*wxy-wx2*wy) * fwy) / det;
          }
        }
        return true;
      }
  }

  //-----------------------------------------------------------------------------
  //! Does a separated convolution of a specified kernel (1D).
  //! @param[in] input Input data that will be filtered.
  //! @param[in] kernel 1D kernel used for convolution
  //! @param[in] output Filtered output image. If this is set to NULL or omitted the filtering is done inplace.
  inline bool separableKernelConvolution(Cuda::DeviceMemory<float, 2>* input,
    Cuda::DeviceMemory<float, 1>* kernel,
    Cuda::DeviceMemory<float, 2>* output = NULL)
  {
    return iSeparableKernelConvolution(input,kernel,output);
  }
  inline bool separableKernelConvolution(Cuda::DeviceMemory<float, 2>* input,
    Cuda::HostMemory<float, 1>* kernel,
    Cuda::DeviceMemory<float, 2>* output = NULL)
  {
    Cuda::DeviceMemoryLinear<float,1> d_kernel(*kernel);
    return iSeparableKernelConvolution(input,&d_kernel,output);
  }


  //! @todo separableKernelConvolution for host memory input and host implementation to compare device implementation!

  //-----------------------------------------------------------------------------
  //! Calculates gradients in x and y direction, and returns the sqared sum of
  //! these gradients
  //! @param[out] input Input image
  //! @param[out] output Edge image
  //! @param[in] root if true, the root is calculated before returning the edges
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float, unsigned char \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool edgeDetection( const Cuda::DeviceMemory<T, 2> *input,
    Cuda::DeviceMemory<T, 2> *output,
    bool root = false)
  {
    return iEdgeDetection(input, output, root);
  }

  //-----------------------------------------------------------------------------
  //! Calculates gradients in x, y and z direction, and returns the sqared sum of
  //! these gradients
  //! @param[out] input Input image
  //! @param[out] output Edge image
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool edgeDetection( const Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<T, 3> *output)
  {
    return iEdgeDetection3D(input, output);
  }


  //-----------------------------------------------------------------------------
  //! Calculates gradients in x and y direction, and returns the sqared value
  //! This function works for color images. Channels are equally weighted by
  //! factor 1/3.
  //! @param[out] input Input image
  //! @param[out] output Edge image
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool colorEdgeDetection( const Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<T, 2> *output)
  {
    return iColorEdgeDetection(input, output);
  }


  //-----------------------------------------------------------------------------
  //! Evaluates the edge function of the form:
  //! output = exp(-edge_scale*pow(input,power))+epsilon
  //! @param[out] input Gradient image image
  //! @param[out] output Edge image with function evaluated (can be the same as input)
  //! @param[in] scale Edge scale factor
  //! @param[in] power Power factor the smaller the stronger weak edges get
  //! @param[in] epsilon additional value, so function does not get 0
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: intersection \n
  template<class T>
  inline bool evaluateEdgeFunction( Cuda::DeviceMemory<T, 2> *input,
    Cuda::DeviceMemory<T, 2> *output,
    float scale, float power, float epsilon )
  {
    return iEvaluateEdgeFunction(input, output, scale, power, epsilon);
  }

  //-----------------------------------------------------------------------------
  //! Evaluates the edge function of the form:
  //! output = exp(-edge_scale*pow(input,power))+epsilon
  //! @param[out] input Gradient image image
  //! @param[out] output Edge image with function evaluated (can be the same as input)
  //! @param[in] scale Edge scale factor
  //! @param[in] power Power factor the smaller the stronger weak edges get
  //! @param[in] epsilon additional value, so function does not get 0
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO testing \n
  template<class T>
  inline bool evaluateEdgeFunction( Cuda::DeviceMemory<T, 3> *input,
    Cuda::DeviceMemory<T, 3> *output,
    float scale, float power, float epsilon)
  {
    return iEvaluateEdgeFunction3D(input, output, scale, power, epsilon);
  }


  //-----------------------------------------------------------------------------
  //! Region growing algorithm for 3D
  //! Delivers a binary result based on seed regions
  //! @param[out] region Region containing the seeds and will contain the result
  //! @param[in] image Input image
  //! @param[in] threshold threshold value for input image (currently everything equal or greater
  //!                      than the threshold will be flooded)
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: TODO \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO testing \n
  template<class T, class U>
  inline bool regionGrowing( Cuda::DeviceMemory<T, 3> *region,
    Cuda::DeviceMemory<U, 3> *image, U threshold)
  {
    return iRegionGrowing(region, image, threshold);
  }



  ///////////////////////////////////////////////////////////////////////////////
  // MACHINE LEARNING
  ///////////////////////////////////////////////////////////////////////////////
  //-----------------------------------------------------------------------------
  inline bool trainForest(const Cuda::DeviceMemoryPitched<float,2>& input,
    const Cuda::DeviceMemoryLinear<int,1>& labels,
    const Cuda::DeviceMemoryLinear<float,1>& weights,
    Cuda::DeviceMemoryPitched<float,2>** forest,
    const int num_samples, const int num_classes,
    const int num_trees, const int max_depth, const int num_tree_cols,
    const int num_hypotheses, const int num_features,
    const float bag_ratio)
  {
    return iTrainForest(input, labels, weights, forest,
      num_samples, num_classes, num_trees, max_depth, num_tree_cols,
      num_hypotheses, num_features, bag_ratio);
  }

  //-----------------------------------------------------------------------------
  inline int forestGetNumTreeCols(const int num_features, const int num_classes) {
    if (num_features * 2 > num_classes)
      return 1 + num_features * 2 + 1;
    else
      return 1 + num_classes + 1;
  }

  //-----------------------------------------------------------------------------
  inline bool evaluateForest(const Cuda::Array<float,2>& forest,
    const Cuda::DeviceMemoryPitched<float,2>& input,
    Cuda::DeviceMemoryPitched<float,2>* confidences,
    const int num_samples, const int num_trees, const int max_depth, const int num_tree_cols,
    const int num_classes, const int num_features)
  {
    return iEvaluateForest(
      forest, input, confidences,
      num_samples, num_trees, max_depth, num_tree_cols,
      num_classes, num_features);
  }

  inline bool computeFeatures(const Cuda::DeviceMemoryPitched<float,3>& input_image,
    Cuda::DeviceMemoryPitched<float,3>* features,
    int num_scales, bool color, bool patch, int patch_size, bool hog)
  {
    return iComputeFeatures(input_image, features, num_scales, color, patch, patch_size, hog);
  }

  inline bool computeFeaturesGetLayout(const Cuda::DeviceMemoryPitched<float,3>& input_image,
    Cuda::Layout<float,3>* layout,
    int num_scales, bool color, bool patch, int patch_size, bool hog)
  {
    int color_num = 3;
    int patch_num = (patch_size * 2 + 1) * (patch_size * 2 + 1) * 3;
    int hog_num = 81;

    int total_num = 0;
    if (color)
      total_num += color_num;
    if (patch)
      total_num += patch_num;
    if (hog)
      total_num += hog_num;

    total_num *= num_scales;

    (*layout).region_ofs = input_image.region_ofs;
    (*layout).region_size = input_image.region_size;
    (*layout).size = input_image.size;
    (*layout).region_ofs[2] = 0;
    (*layout).region_size[2] = total_num;
    (*layout).size[2] = total_num;
    return true;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // INTERACTIVE FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Draws a line of specified size to an image
  //! @param[out] image  Input image to which will be drawn
  //! @param[in] x_start Starting coordiante x of line
  //! @param[in] y_start Starting coordiante y of line
  //! @param[in] x_end End coordiante x of line
  //! @param[in] y_end End coordiante y of line
  //! @param[in] line_width Width (squared) of line
  //! @param[in] intensity intensity with which will be drawn
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool drawLine( Cuda::DeviceMemory<T, 2> *image,
    int x_start, int y_start, int x_end, int y_end,
    int line_width, T intensity)
  {
    return iDrawLine(image, x_start, y_start, x_end, y_end, line_width,  intensity);
  }

  //-----------------------------------------------------------------------------
  //! Draws a line of specified size to a 3D image
  //! @param[out] image  Input image to which will be drawn
  //! @param[in] x_start Starting coordiante x of line
  //! @param[in] y_start Starting coordiante y of line
  //! @param[in] z_start Starting coordiante z of line
  //! @param[in] x_end End coordiante x of line
  //! @param[in] y_end End coordiante y of line
  //! @param[in] z_end End coordiante z of line
  //! @param[in] line_width Width (squared) of line
  //! @param[in] intensity intensity with which will be drawn
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool drawLine( Cuda::DeviceMemory<T, 3> *image,
    int x_start, int y_start, int z_start,
    int x_end, int y_end, int z_end,
    int line_width, T intensity)
  {
    return iDrawLine3D(image, x_start, y_start, z_start, x_end, y_end, z_end, line_width,  intensity);
  }

  //-----------------------------------------------------------------------------
  //! Draws a line of specified size to an image
  //! This function takes an additional binary mask, that restricts the drawing area
  //! @param[out] image  Input image to which will be drawn
  //! @param[in] maks  Mask image (drawing only where mask == 1)
  //! @param[in] x_start Starting coordiante x of line
  //! @param[in] y_start Starting coordiante y of line
  //! @param[in] x_end End coordiante x of line
  //! @param[in] y_end End coordiante y of line
  //! @param[in] line_width Width (squared) of line
  //! @param[in] intensity intensity with which will be drawn
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool drawLineMasked( Cuda::DeviceMemory<T, 2> *image,
    Cuda::DeviceMemory<T, 2> *mask,
    int x_start, int y_start, int x_end, int y_end,
    int line_width, T intensity)
  {
    return iDrawLineMasked(image, mask, x_start, y_start, x_end, y_end, line_width,  intensity);
  }

  //-----------------------------------------------------------------------------
  //! Draws a rectangle of specified size to an image
  //! @param[out] image  Input image to which will be drawn
  //! @param[in] x_start Starting coordiante x
  //! @param[in] y_start Starting coordiante y
  //! @param[in] x_end End coordiante x
  //! @param[in] y_end End coordiante y
  //! @param[in] line_width Width (squared) of line
  //! @param[in] intensity intensity with which will be drawn
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool drawRectangle( Cuda::DeviceMemory<T, 2> *image,
    int x_start, int y_start, int x_end, int y_end,
    int line_width, T intensity)
  {
    int ret = iDrawLine(image, x_start, y_start, x_start, y_end, line_width,  intensity);
    if (!ret) return ret;
    ret = iDrawLine(image, x_end, y_start, x_end, y_end, line_width,  intensity);
    if (!ret) return ret;
    ret = iDrawLine(image, x_start, y_start, x_end, y_start, line_width,  intensity);
    if (!ret) return ret;
    ret = iDrawLine(image, x_start, y_end, x_end, y_end, line_width,  intensity);
    return ret;
  }

  //-----------------------------------------------------------------------------
  //! Draws a cube of specified size to an image
  //! @param[out] image  Input image to which will be drawn
  //! @param[in] x_start Starting coordiante x
  //! @param[in] y_start Starting coordiante y
  //! @param[in] z_start Starting coordiante z
  //! @param[in] x_end End coordiante x
  //! @param[in] y_end End coordiante y
  //! @param[in] z_end End coordiante z
  //! @param[in] line_width Width of line (Extended to inside)
  //! @param[in] intensity intensity with which will be drawn
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: float \n
  //!       \a Compute \a Capability: TODO \n
  //!       \a ROI: TODO \n
  template<class T>
  inline bool drawCube( Cuda::DeviceMemory<T, 3> *image,
    int x_start, int y_start, int z_start,
    int x_end, int y_end, int z_end,
    int line_width, T intensity)
  {
    int ret = iDrawCube(image, x_start, y_start, z_start, x_end, y_end, z_end, line_width,  intensity);
    return ret;
  }




  ///////////////////////////////////////////////////////////////////////////////
  // IMAGE RESCALING AND WARPING FUNCTIONS
  ///////////////////////////////////////////////////////////////////////////////

  //-----------------------------------------------------------------------------
  //! Rescales the given input image to fit in the provided output image with a
  //! reasonably good filter (default Lanczos).
  //! @todo Extend this documentation
  //! @param[in] input   Input image as an array
  //! @param[out] output Filtered image as pitched device memory
  //! @param[in] boundaryCondition Boundary condition ???
  //! @param[in] filterType filter type ???
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<typename T>
  inline bool rescaleImage(const Cuda::Array<T, 2> &input,
    Cuda::DeviceMemoryPitched<T, 2> &output,
    ImageRescalingFilter filterType = DefaultFilter,
    bool clampTo01 = true, float blur = 1.0f) {
      return iRescaleImage(input, output, filterType, clampTo01, blur);
  }

  //-----------------------------------------------------------------------------
  //! @todo Documentation ...
  template<typename T>
  inline bool rescaleImage(const Cuda::HostMemoryReference<T, 2> &input,
    Cuda::HostMemoryReference<T, 2> &output,
    ImageRescalingFilter filterType = DefaultFilter,
    bool clampTo01 = true, float blur = 1.0f,
    bool force_gpu = false) {
      if (force_gpu) {
        Cuda::Array<T, 2> in(input);
        Cuda::DeviceMemoryPitched<T, 2> out_d(output);
        if(iRescaleImage(in, out_d, filterType, clampTo01, blur)) {
          Cuda::copy(output, out_d);
          return true;
        }
        else
          return false;
      }
      else {
        // TODO: CPU implementation. Call zoom code
        const T *buffer_a = input.getBuffer();
        const T *buffer_b = output.getBuffer();
      }
      return true;
  }

  //-----------------------------------------------------------------------------
  //! Warps the given input image with the given disparity fields \a dx and \a dy
  //! @todo Extend this documentation
  //! @param[in] input   Input image as an array
  //! @param[out] output Filtered image as pitched device memory
  //! @param[in] boundaryCondition Boundary condition ???
  //! @param[in] filterType filter type ???
  //! @return True if call was successful
  //!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<typename T>
  inline bool warpImage(const Cuda::Array<T, 2> &input,
    Cuda::DeviceMemory<T, 2> *dx, Cuda::DeviceMemory<T, 2> *dy,
    Cuda::DeviceMemory<T, 2> *output,
    ImageRescalingFilter filterType = DefaultFilter,
    bool clampTo01 = true, float blur = 1.0f) {
      return iWarpImage(input, dx, dy, output, filterType, clampTo01, blur);
  }

  //-----------------------------------------------------------------------------
  //! @todo Documentation ...
  template<typename T>
  inline bool warpImage(const Cuda::HostMemoryReference<T, 2> &input,
    Cuda::HostMemoryReference<T, 2> &dx,
    Cuda::HostMemoryReference<T, 2> &dy,
    Cuda::HostMemoryReference<T, 2> &output,
    ImageRescalingFilter filterType = DefaultFilter,
    bool clampTo01 = true, float blur = 1.0f,
    bool force_gpu = false) {
      if (force_gpu) {
        Cuda::Array<T, 2> in(input);
        Cuda::DeviceMemoryPitched<T, 2> dx_d(dx);
        Cuda::DeviceMemoryPitched<T, 2> dy_d(dy);
        Cuda::DeviceMemoryPitched<T, 2> out_d(output);
        if(iWarpImage(in, dx_d, dy_d, out_d, filterType, clampTo01, blur)) {
          Cuda::copy(output, out_d);
          return true;
        }
        else
          return false;
      }
      else {
        // TODO: CPU implementation.
        const T *buffer_a = input.getBuffer();
        const T *buffer_b = output.getBuffer();
        fprintf(stderr, "CPU implementation of warpImage not yet available!\n");
        return false;
      }
  }

#ifdef USING_CUDA22
/*
  The following fcns are using the ability to bind pitched device memory to
  textures. This is only possible since Cuda 2.2. Therefore the interfaces are only
  available if nvcc release >= 2.2 is installed.
*/

 //-----------------------------------------------------------------------------
  //! Rescales the given input image to fit in the provided output image with a
  //! reasonably good filter (default Lanczos).
  //! @todo Extend this documentation
  //! @param[in] input   Input image as pitched device memory (only possible in Cuda > 2.2)
  //! @param[out] output Filtered image as pitched device memory
  //! @param[in] boundaryCondition Boundary condition ???
  //! @param[in] filterType filter type ???
  //! @return True if call was successful
  //!
  //! @note for a call using HostReference use the array version!
  //! @note \a Datatypes: ? \n
  //!       \a Compute \a Capability: ? \n
  //!       \a ROI: ? \n
  template<typename T>
  inline bool rescaleImage(const Cuda::DeviceMemoryPitched<T, 2> &input,
    Cuda::DeviceMemoryPitched<T, 2> &output,
    ImageRescalingFilter filterType = DefaultFilter,
    bool clampTo01 = true, float blur = 1.0f) {
      return iRescaleImage(input, output, filterType, clampTo01, blur);
  }

#endif // USING_CUDA22

} // namespace CommonLib

#endif //COMMONLIB_H
