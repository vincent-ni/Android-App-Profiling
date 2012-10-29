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

#ifndef INTERFACE_CUH_
#define INTERFACE_CUH_

#include "CommonLibDefs.h"

///////////////////////////////////////////////////////////////////////////////
// DEVICE SUPPORT FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
bool iDeviceQuery(int *number, bool verbose);
bool iSelectDevice(unsigned int device_id);
bool iSelectDeviceDialog();
bool iGetCurrentComputeCapability(int *major, int *minor);
bool iIsSupportedSM13();

///////////////////////////////////////////////////////////////////////////////
// IMAGE STATISTICS FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
bool iComputeRMSE( const Cuda::Array<unsigned char, 2> &data_a,
                   const Cuda::Array<unsigned char, 2> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::Array<unsigned char, 2> &data_a,
                   const Cuda::Array<unsigned char, 2> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::Array<float, 2> &data_a,
                   const Cuda::Array<float, 2> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::Array<float, 2> &data_a,
                   const Cuda::Array<float, 2> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<unsigned char, 2> &data_a,
                   const Cuda::DeviceMemory<unsigned char, 2> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<unsigned char, 2> &data_a,
                   const Cuda::DeviceMemory<unsigned char, 2> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<float, 2> &data_a,
                   const Cuda::DeviceMemory<float, 2> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<float, 2> &data_a,
                   const Cuda::DeviceMemory<float, 2> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::Array<unsigned char, 3> &data_a,
                   const Cuda::Array<unsigned char, 3> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::Array<unsigned char, 3> &data_a,
                   const Cuda::Array<unsigned char, 3> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::Array<float, 3> &data_a,
                   const Cuda::Array<float, 3> &data_b, float *rmse);
bool iComputeRMSE (const Cuda::Array<float, 3> &data_a,
                   const Cuda::Array<float, 3> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<unsigned char, 3> &data_a,
                   const Cuda::DeviceMemory<unsigned char, 3> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<unsigned char, 3> &data_a,
                   const Cuda::DeviceMemory<unsigned char, 3> &data_b, double *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<float, 3> &data_a,
                   const Cuda::DeviceMemory<float, 3> &data_b, float *rmse);
bool iComputeRMSE( const Cuda::DeviceMemory<float, 3> &data_a,
                   const Cuda::DeviceMemory<float, 3> &data_b, double *rmse);

bool iRandomNumbers(Cuda::DeviceMemory<float,2> *data, const float a, const float b);
bool iRandomNumbers(Cuda::DeviceMemory<int,2> *data, const int a, const int b);

bool iComputeSum(const Cuda::DeviceMemory<unsigned char, 2> &data, float *sum);
bool iComputeSum(const Cuda::DeviceMemory<unsigned char, 2> &data, double *sum);
bool iComputeSum(const Cuda::DeviceMemory<float, 2> &data, float *sum);
bool iComputeSum(const Cuda::DeviceMemory<float, 3> &data, float *sum);
bool iComputeSum(const Cuda::DeviceMemory<float, 2> &data, double *sum);
bool iComputeSum(const Cuda::Array<unsigned char, 2> &data, float *sum);
bool iComputeSum(const Cuda::Array<unsigned char, 2> &data, double *sum);
bool iComputeSum(const Cuda::Array<float, 2> &data, float *sum);
bool iComputeSum(const Cuda::Array<float, 2> &data, double *sum);

bool iFindMinMax( const Cuda::DeviceMemory<unsigned char, 2> &data,
                  unsigned char *minimum, unsigned char *maximum);
bool iFindMinMax( const Cuda::DeviceMemory<int, 2> &data,
                  int *minimum, int *maximum);
bool iFindMinMax( const Cuda::DeviceMemory<float, 2> &data,
                  float *minimum, float *maximum);
bool iFindMinMax( const Cuda::DeviceMemory<unsigned char, 3> &data,
                  unsigned char *minimum, unsigned char *maximum);
bool iFindMinMax( const Cuda::DeviceMemory<int, 3> &data,
                  int *minimum, int *maximum);
bool iFindMinMax( const Cuda::DeviceMemory<float, 3> &data,
                  float *minimum, float *maximum);

///////////////////////////////////////////////////////////////////////////////
// ARITHMETIC FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
bool iAbsoluteDifference(const Cuda::DeviceMemory<unsigned char, 2> *data_a,
                         const Cuda::DeviceMemory<unsigned char, 2> *data_b,
                         Cuda::DeviceMemory<unsigned char, 2> *result);
bool iAbsoluteDifference(const Cuda::DeviceMemory<float, 2> *data_a,
                         const Cuda::DeviceMemory<float, 2> *data_b,
                         Cuda::DeviceMemory<float, 2> *result);

bool iDifference(const Cuda::DeviceMemory<unsigned char, 2> *data_a,
                 const Cuda::DeviceMemory<unsigned char, 2> *data_b,
                 Cuda::DeviceMemory<unsigned char, 2> *result);
bool iDifference(const Cuda::DeviceMemory<float, 2> *data_a,
                 const Cuda::DeviceMemory<float, 2> *data_b,
                 Cuda::DeviceMemory<float, 2> *result);

bool iAdd( Cuda::DeviceMemory<unsigned char, 2> *data_a,
           Cuda::DeviceMemory<unsigned char, 2> *data_b,
           Cuda::DeviceMemory<unsigned char, 2> *result);
bool iAdd(Cuda::DeviceMemory<float, 2> *data_a,
          Cuda::DeviceMemory<float, 2> *data_b,
          Cuda::DeviceMemory<float, 2> *result);

bool iAddWeighted( Cuda::DeviceMemory<unsigned char, 2> *data_a,
                   Cuda::DeviceMemory<unsigned char, 2> *data_b,
                   float weight_a, float weight_b,
                   Cuda::DeviceMemory<unsigned char, 2> *result);
bool iAddWeighted( Cuda::DeviceMemory<float, 2> *data_a,
                   Cuda::DeviceMemory<float, 2> *data_b,
                   float weight_a, float weight_b,
                   Cuda::DeviceMemory<float, 2> *result);

bool iAddWeighted( Cuda::DeviceMemory<float, 3> *data_a,
                   Cuda::DeviceMemory<float, 3> *data_b,
                   float weight_a, float weight_b,
                   Cuda::DeviceMemory<float, 3> *result);

bool iMultiplyConstant(Cuda::DeviceMemoryPitched<unsigned char, 2> *data, float factor);
bool iMultiplyConstant(Cuda::DeviceMemoryPitched<float, 2> *data, float factor);

bool iMultiply( Cuda::DeviceMemory<unsigned char, 2> *data_a,
                Cuda::DeviceMemory<unsigned char, 2> *data_b,
                Cuda::DeviceMemory<unsigned char, 2> *result);
bool iMultiply( Cuda::DeviceMemory<float, 2> *data_a,
                Cuda::DeviceMemory<float, 2> *data_b,
                Cuda::DeviceMemory<float, 2> *result);
bool iMultiply( Cuda::DeviceMemory<unsigned char, 3> *data_a,
                Cuda::DeviceMemory<unsigned char, 3> *data_b,
                Cuda::DeviceMemory<unsigned char, 3> *result);
bool iMultiply( Cuda::DeviceMemory<float, 3> *data_a,
                Cuda::DeviceMemory<float, 3> *data_b,
                Cuda::DeviceMemory<float, 3> *result);

bool iThresholdGreater( const Cuda::DeviceMemory<unsigned char, 2> *input,
                        Cuda::DeviceMemory<unsigned char, 2> *output,
                        float threshold);
bool iThresholdGreater( const Cuda::DeviceMemory<float, 2> *input,
                        Cuda::DeviceMemory<unsigned char, 2> *output,
                        float threshold);
bool iThresholdGreater( const Cuda::DeviceMemory<float, 2> *input,
                        Cuda::DeviceMemory<float, 2> *output,
                        float threshold);

bool iThresholdGreater( const Cuda::DeviceMemory<unsigned char, 3> *input,
                        Cuda::DeviceMemory<unsigned char, 3> *output,
                        float threshold);
bool iThresholdGreater( const Cuda::DeviceMemory<float, 3> *input,
                        Cuda::DeviceMemory<unsigned char, 3> *output,
                        float threshold);
bool iThresholdGreater( const Cuda::DeviceMemory<float, 3> *input,
                        Cuda::DeviceMemory<float, 3> *output,
                        float threshold);

bool iNormalize(Cuda::DeviceMemory<float, 2> *data);
bool iNormalize(Cuda::DeviceMemory<float, 3> *data);

bool iComputeEDT( const Cuda::DeviceMemory<unsigned char, 2> *input,
                  Cuda::DeviceMemory<float, 2>* distance);
bool iComputeEDT( const Cuda::DeviceMemory<float, 2> *input,
                  Cuda::DeviceMemory<float, 2>* distance);

bool iComputeEDT( const Cuda::DeviceMemory<unsigned char, 3> *input,
                  Cuda::DeviceMemory<float, 3>* distance);
bool iComputeEDT( const Cuda::DeviceMemory<float, 3> *input,
                  Cuda::DeviceMemory<float, 3>* distance);

bool iComputeSEDT( const Cuda::DeviceMemory<unsigned char, 2> *input,
                   Cuda::DeviceMemory<float, 2>* distance);
bool iComputeSEDT( const Cuda::DeviceMemory<float, 2> *input,
                   Cuda::DeviceMemory<float, 2>* distance);

bool iComputeSEDT( const Cuda::DeviceMemory<unsigned char, 3> *input,
                   Cuda::DeviceMemory<float, 3>* distance);
bool iComputeSEDT( const Cuda::DeviceMemory<float, 3> *input,
                   Cuda::DeviceMemory<float, 3>* distance);

///////////////////////////////////////////////////////////////////////////////
// FILTERING FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
bool iConvolution(Cuda::DeviceMemoryPitched<float, 2> *data, const Cuda::DeviceMemoryPitched<float, 2> &kernel);

bool iGaussianConvolution(Cuda::DeviceMemoryPitched<float, 2> *data, float sigma, unsigned int kernelsize);

bool iGaussianConvolution(Cuda::DeviceMemoryPitched<float, 3> *data, float sigma, unsigned int kernelsize);

bool iMedianFilter3x3(const Cuda::DeviceMemoryPitched<float, 2> &input, Cuda::DeviceMemoryPitched<float, 2> &output);

bool iSeewigConvolution(Cuda::Array<float, 2> *data, const float lcx, const float lcy, const float sx, const float sy);

bool iSeparableKernelConvolution(Cuda::DeviceMemory<float, 2>* input,
                                 Cuda::DeviceMemory<float, 1>* kernel,
                                 Cuda::DeviceMemory<float, 2>* output = NULL);

bool iEdgeDetection( const Cuda::DeviceMemory<float, 2> *input,
                     Cuda::DeviceMemory<float, 2> *output,
                     bool root );
bool iEdgeDetection( const Cuda::DeviceMemory<unsigned char, 2> *input,
                     Cuda::DeviceMemory<unsigned char, 2> *output,
                     bool root );

bool iEdgeDetection3D(const Cuda::DeviceMemory<float, 3> *input,
                    Cuda::DeviceMemory<float, 3> *output);

bool iColorEdgeDetection(const Cuda::DeviceMemory<float, 3> *input,
                         Cuda::DeviceMemory<float, 2> *output);

bool iEvaluateEdgeFunction(Cuda::DeviceMemory<float, 2> *input,
                           Cuda::DeviceMemory<float, 2> *output,
                           float scale, float power, float epsilon);

bool iEvaluateEdgeFunction3D(Cuda::DeviceMemory<float, 3> *input,
                           Cuda::DeviceMemory<float, 3> *output,
                           float scale, float power, float epsilon);

bool iRegionGrowing( Cuda::DeviceMemory<unsigned char, 3> *region,
                     Cuda::DeviceMemory<unsigned char, 3> *image,
                     unsigned char threshold);

bool iRegionGrowing( Cuda::DeviceMemory<unsigned char, 3> *region,
                     Cuda::DeviceMemory<float, 3> *image,
                     float threshold);

bool iRegionGrowing( Cuda::DeviceMemory<float, 3> *region,
                     Cuda::DeviceMemory<float, 3> *image,
                     float threshold);

///////////////////////////////////////////////////////////////////////////////
// MACHINE LEARNING
///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
bool iTrainForest(
	const Cuda::DeviceMemoryPitched<float,2>& input,
	const Cuda::DeviceMemoryLinear<int,1>& labels,
	const Cuda::DeviceMemoryLinear<float,1>& weights,
	Cuda::DeviceMemoryPitched<float,2>** forest,
	const int num_samples, const int num_classes, 
	const int num_trees, const int max_depth, const int num_tree_cols,  
	const int num_hypotheses, const int num_features,
	const float bag_ratio
);

//-----------------------------------------------------------------------------
bool iEvaluateForest(
	const Cuda::Array<float,2>& forest, 
	const Cuda::DeviceMemoryPitched<float,2>& input,
	Cuda::DeviceMemoryPitched<float,2>* confidences,
	const int num_samples, const int num_trees, const int max_depth, const int num_tree_cols, 
    const int num_classes, const int num_features
);

//-----------------------------------------------------------------------------
bool iComputeFeatures(const Cuda::DeviceMemoryPitched<float,3>& input_image,
	Cuda::DeviceMemoryPitched<float,3>* features,
    int num_scales, bool color, bool patch, int patch_size, bool hog);



///////////////////////////////////////////////////////////////////////////////
// INTERACTIVE FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
bool iDrawLine(Cuda::DeviceMemory<unsigned char, 2> *image, int x_start, int y_start,
                int x_end, int y_end, int line_width, unsigned char intensity);
bool iDrawLine(Cuda::DeviceMemory<float, 2> *image, int x_start, int y_start,
               int x_end, int y_end, int line_width, float intensity);
bool iDrawLine3D(Cuda::DeviceMemory<float, 3> *image, int x_start, int y_start, int z_start,
               int x_end, int y_end, int z_end, int line_width, float intensity);
bool iDrawLine3D(Cuda::DeviceMemory<unsigned char, 3> *image, int x_start, int y_start, int z_start,
               int x_end, int y_end, int z_end, int line_width, unsigned char intensity);
bool iDrawLineMasked(Cuda::DeviceMemory<float, 2> *image,
                     Cuda::DeviceMemory<float, 2> *mask,
                     int x_start, int y_start, int x_end, int y_end,
                     int line_width, float intensity);
bool iDrawCube(Cuda::DeviceMemory<float, 3> *image, int x_start, int y_start, int z_start,
                int x_end, int y_end, int z_end, int line_width, float intensity);

///////////////////////////////////////////////////////////////////////////////
// IMAGE RESCALING FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

/**
 * This is disabled and fixed to Neumann conditions for now. Dirichlet
 * doesn't really make sense, does it? The image would get darker along
 * the boundary...
enum ImageRescalingBoundaryCondition {
    DirichletBoundaryCondition,
    NeumannBoundaryCondition
};
*/

enum ImageRescalingFilter {
    DefaultFilter,            // LanczosFilter for downscaling, Mitchell for upscaling
    TriangleFilter,
    QuadraticFilter,
    CubicFilter,
    MitchellFilter,
    LanczosFilter,            // Sinc, 3 lobes, blackman
    Sinc4BlackmanFilter,      // Sinc, 4 lobes, blackman
    Sinc4HammingFilter,       // Sinc, 4 lobes, hamming
    Sinc4HanningFilter,       // Sinc, 4 lobes, hanning
    Bessel3Filter,            // Jinc, 3 lobes, blackman
    Bessel5Filter,            // Jinc, 5 lobes, blackman
};

bool iRescaleImage(const Cuda::Array<float, 2> &input, Cuda::DeviceMemoryPitched<float, 2> &output,
                   ImageRescalingFilter filterType, bool clampTo01, float blur);

bool iWarpImage(const Cuda::Array<float, 2> &input, Cuda::DeviceMemory<float, 2> *dx,
                Cuda::DeviceMemory<float, 2> *dy, Cuda::DeviceMemory<float, 2> *output,
                ImageRescalingFilter filterType, bool clampTo01, float blur);

#ifdef USING_CUDA22

bool iRescaleImage(const Cuda::DeviceMemoryPitched<float, 2> &input, Cuda::DeviceMemoryPitched<float, 2> &output,
                   ImageRescalingFilter filterType, bool clampTo01, float blur);

// TODO
// bool iWarpImage(const Cuda::Array<float, 2> &input, Cuda::DeviceMemory<float, 2> *dx,
//                 Cuda::DeviceMemory<float, 2> *dy, Cuda::DeviceMemory<float, 2> *output,
//                 ImageRescalingFilter filterType, bool clampTo01, float blur);

#endif // USING_CUDA22

#endif //INTERFACE_CUH_
