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
 * Module      : VM Lib
 * Class       : $RCSfile$
 * Language    : C++/CUDA
 * Description : Implementation of image filters based on variational methods
 *
 * Author     :
 * EMail      : unger@icg.tugraz.at
 *
 */

#ifndef VMLIB_H
#define VMLIB_H

///////////////////////////////////////////////////////////////////////////////
//! Make sure that all functions are called from the same thread !!
//! See VM_defs.h for additional information (defines and typedefs)
////////////////////////////////////////////////////////////////////////////////

// include deffinitions
#include "VM_defs.h"
#include "VMLibSharedLibMacros.h"

// System includes
#include <float.h>
#include <math.h>
#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <iomanip>
using namespace std;

 class VMLIB_DLLAPI VMLib
{
  public:
    ////////////////////////////////////////////////////////////////////////////////
    //! Constructor of the VM library.
    //! Does some initializations, creates abbrevations, and list of available
    //! algorithms, models and modes.
    ////////////////////////////////////////////////////////////////////////////////
    VMLib();

    ////////////////////////////////////////////////////////////////////////////////
    //! Destructor of the VM library
    ////////////////////////////////////////////////////////////////////////////////
    ~VMLib();

    ////////////////////////////////////////////////////////////////////////////////
    //! Prints list with available algorithms.
    //! All algorithms that can be used with VLib are listed with a short description.
    ////////////////////////////////////////////////////////////////////////////////
    void printAvailableAlgorithms();

    ////////////////////////////////////////////////////////////////////////////////
    //! Determines if a certain combination is available for the current input image.
    //! @param model    The desired model selection
    //! @param alg      The desired algorithm selection
    //! @return         Is true if algorithm is implemented for selected model
    ////////////////////////////////////////////////////////////////////////////////
    bool isAvailable(int model, int alg);

    ////////////////////////////////////////////////////////////////////////////////
    //! Determines if a certain combination is available.
    //! @param model      The desired model selection
    //! @param alg        The desired algorithm selection
    //! @param mode       The desired image type
    //! @return           Is true if selected combination is available
    ////////////////////////////////////////////////////////////////////////////////
    bool isAvailable(int model, int alg, int mode);

    ////////////////////////////////////////////////////////////////////////////////
    //! Determines the number of available models.
    //! @return        Number of available models
    ////////////////////////////////////////////////////////////////////////////////
    int getNumModels();

    ////////////////////////////////////////////////////////////////////////////////
    //! Retruns a string with abbreviation for specified model.
    //! @param num     The desired model selection
    //! @return        String containing abbreviation
    ////////////////////////////////////////////////////////////////////////////////
    string getModelAbbreviation(int num);

    ////////////////////////////////////////////////////////////////////////////////
    //! Retruns string with description for specified model.
    //! @param num     The desired model selection
    //! @return        String containing description
    ////////////////////////////////////////////////////////////////////////////////
    string getModelDescription(int num);

    ////////////////////////////////////////////////////////////////////////////////
    //! Determines the number of available algorithms.
    //! @return        Number of available algorithms
    ////////////////////////////////////////////////////////////////////////////////
    int getNumAlgorithms();

    ////////////////////////////////////////////////////////////////////////////////
    //! Retruns string with abbreviation for specified algorithm.
    //! @param num     The desired algorithm selection
    //! @return        String containing abbreviation
    ////////////////////////////////////////////////////////////////////////////////
    string getAlgorithmAbbreviation(int num);

    ////////////////////////////////////////////////////////////////////////////////
    //! Retruns string with description for specified algorithm.
    //! @param num     The desired algorithm selection
    //! @return        String containing description
    ////////////////////////////////////////////////////////////////////////////////
    string getAlgorithmDescription(int num);

    ////////////////////////////////////////////////////////////////////////////////
    //! Returns a list containing the progress of the primal energy in the last call
    //! If no algorithm was called to be iterated until convergence, the list is
    //! empty
    //! @return        List of primal energies
    ////////////////////////////////////////////////////////////////////////////////
    list<float> getPrimalEnergyList() { return primal_en_; };

    ////////////////////////////////////////////////////////////////////////////////
    //! Returns a list containing the progress of the dual energy in the last call
    //! If no algorithm was called to be iterated until convergence, the list is
    //! empty. Not all algorithms log a dual energy
    //! @return        List of dual energies
    ////////////////////////////////////////////////////////////////////////////////
    list<float> getDualEnergyList() { return dual_en_; };

    ////////////////////////////////////////////////////////////////////////////////
    //! Returns a list containing the progress of the iterations in the last call
    //! If no algorithm was called to be iterated until convergence, the list is
    //! empty
    //! @return        List of iterations
    ////////////////////////////////////////////////////////////////////////////////
    list<float> getIterationsEnergyList() { return iterations_; };

    ////////////////////////////////////////////////////////////////////////////////
    //! Returns a list containing the progress of the time in the last call
    //! If no algorithm was called to be iterated until convergence, the list is
    //! empty
    //! @return        List of time
    ////////////////////////////////////////////////////////////////////////////////
    list<float> getTimeEnergyList() { return time_; };

    ////////////////////////////////////////////////////////////////////////////////
    //! Selects a model that will be minimized.
    //! @param value    The desired model selection
    ////////////////////////////////////////////////////////////////////////////////
    void setModel(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Selects an algorithm used for minimization.
    //! @param value    The desired algorithm selection
    ////////////////////////////////////////////////////////////////////////////////
    void setAlgorithm(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the parameter lambda.
    //! See documentation for influence of parameter with the according model.
    //! If value is set FLT_MAX, the algorithm (maybe) will choose a default parameter.
    //! @param value   New value for lambda
    ////////////////////////////////////////////////////////////////////////////////
    void setLambda(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the parameter alpha.
    //! See documentation for influence of parameter with the according model.
    //! If value is set FLT_MAX, the algorithm (maybe) will choose a default parameter.
    //! @param value   New value for alpha
    ////////////////////////////////////////////////////////////////////////////////
    void setAlpha(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the parameter beta.
    //! See documentation for influence of parameter with the according model.
    //! If value is set FLT_MAX, the algorithm (maybe) will choose a default parameter.
    //! @param value   New value for beta
    ////////////////////////////////////////////////////////////////////////////////
    void setBeta(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the parameter delta.
    //! See documentation for influence of parameter with the according model.
    //! If value is set FLT_MAX, the algorithm (maybe) will choose a default parameter.
    //! @param value   New value for delta
    ////////////////////////////////////////////////////////////////////////////////
    void setDelta(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Change verbose mode. If enabled, additional information is displayed.
    //! @param value    Enables or disables verbose mode
    ////////////////////////////////////////////////////////////////////////////////
    void setVerbose(bool value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Change matlab mode. If enabled, additional information (e.g. energies,
    //! timesteps ... over time) is written to a Matlab file.
    //! @param value    Enables or disables matlab mode
    ////////////////////////////////////////////////////////////////////////////////
    void setMatlab(bool value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the Matlab file name. Only used if Matlab mode is enabled.
    //! Default name is set to 'VMLib_Log.m'.
    //! @param file_name  New filename for Matlab output
    ////////////////////////////////////////////////////////////////////////////////
    void setMatlabFile(string file_name);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the maximum number of iterations.
    //! Only taken into account when running until convergence.
    //! The default value is set to INT_MAX.
    //! @param value    Maximum number of iterations
    ////////////////////////////////////////////////////////////////////////////////
    void setMaxIterations(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the time allowed for computation (in ms).
    //! Only taken into account when running until convergence.
    //! The default value is set to FLT_MAX.
    //! @param value    Maximum time in ms
    ////////////////////////////////////////////////////////////////////////////////
    void setMaxTime(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Sets the convergence criterion.
    //! If set to FLT_MAX this parameter will be choosen internally (recomended).
    //! @param value    New convergence criterion
    ////////////////////////////////////////////////////////////////////////////////
    void setConvergenceCriterion(float value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Calculates the optimal memory size for 3D images.
    //! This is necassary to speed up the algorithms by using an optimal memory layout.
    //! Make sure to use this size to create the IO memory! But do not forget to set
    //! a region on which the algorithm should be evaluated (that is usually the size
    //! of the original image).
    //! @param size    Size of input image
    //! @return        Optimal size for memory alignment
    ////////////////////////////////////////////////////////////////////////////////
    Cuda::Size<3> getOptimalMemSize(Cuda::Size<3> size);

    ////////////////////////////////////////////////////////////////////////////////
    //! Calculates the optimal memory size for 2D color images.
    //! This is necassary to speed up the algorithms by using an optimal memory layout.
    //! Make sure to use this size to create the IO memory! But do not forget to set
    //! a region on which the algorithm should be evaluated (that is usually the size
    //! of the original image).
    //! @param size    Size of input image
    //! @return        Optimal size for memory alignment
    ////////////////////////////////////////////////////////////////////////////////
    Cuda::Size<3> getOptimalMemSizeColor(Cuda::Size<3> size);

    ////////////////////////////////////////////////////////////////////////////////
    //! Calculates the optimal memory size for 2D images.
    //! This is necassary to speed up the algorithms by using an optimal memory layout.
    //! Make sure to use this size to create the IO memory! But do not forget to set
    //! a region on which the algorithm should be evaluated (that is usually the size
    //! of the original image).
    //! @param size    Size of input image
    //! @return        Optimal size for memory alignment
    ////////////////////////////////////////////////////////////////////////////////
    Cuda::Size<2> getOptimalMemSize(Cuda::Size<2> size);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO image u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setU(Cuda::DeviceMemory<float, 2>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO color image u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setUColor(Cuda::DeviceMemory<float, 3>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO image u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setU(Cuda::DeviceMemory<float, 3>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO image f. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setF(Cuda::DeviceMemory<float, 2>* f);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO color image f. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setFColor(Cuda::DeviceMemory<float, 3>* f);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO image f. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setF(Cuda::DeviceMemory<float, 3>* f);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO image g. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param g       The image that will be used for g
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setG(Cuda::DeviceMemory<float, 2>* g);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO image g. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param g       The image that will be used for g
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setG(Cuda::DeviceMemory<float, 3>* g);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO image gz. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param gz       The image that will be used for gz
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int setGz(Cuda::DeviceMemory<float, 3>* gz);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO images f and u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFU(Cuda::DeviceMemory<float, 2>* f, Cuda::DeviceMemory<float, 2>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set color IO images f and u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFUColor(Cuda::DeviceMemory<float, 3>* f, Cuda::DeviceMemory<float, 3>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO images f and u. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFU(Cuda::DeviceMemory<float, 3>* f, Cuda::DeviceMemory<float, 3>* u);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set IO images f, u and g. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @param g       The image that will be used for g
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFUG( Cuda::DeviceMemory<float, 2>* f, Cuda::DeviceMemory<float, 2>* u,
                 Cuda::DeviceMemory<float, 2>* g);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO images f, u and g. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @param g       The image that will be used for g
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFUG( Cuda::DeviceMemory<float, 3>* f, Cuda::DeviceMemory<float, 3>* u,
                 Cuda::DeviceMemory<float, 3>* g);

    ////////////////////////////////////////////////////////////////////////////////
    //! Set 3D IO images f, u, g and gz. See documentation of models for meaning of images.
    //! All images have to be created and maintained externally!
    //! This ensures that the user always has the full control over the input and
    //! output data. Make sure memory is allocated correctly according to
    //! getOptimalMemSize(), and that all images have the same memory layout.
    //! Use setRegion to set the area in the image on which the calculation
    //! will be done. The algorithm will be eveluated on the intersection of all
    //! necassary regions.
    //! @param f       The image that will be used for f
    //! @param u       The image that will be used for u
    //! @param g       The image that will be used for g
    //! @param gz      The image that will be used for gz
    //! @return        Error handling
    ////////////////////////////////////////////////////////////////////////////////
    bool setFUGGz( Cuda::DeviceMemory<float, 3>* f, Cuda::DeviceMemory<float, 3>* u,
                   Cuda::DeviceMemory<float, 3>* g, Cuda::DeviceMemory<float, 3>* gz);

    ////////////////////////////////////////////////////////////////////////////////
    //! Runs a specified number of iterations.
    //! If the number of iterations is set to 0, the algorithm will run until
    //! convergence. If greater than 0, no convergence criterions are evaluated,
    //! and the algorithm automatically returns after the specified number of iterations.
    //! @param iterations Number of iterations to calculate
    //! @return           Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int runAlgorithm(int iterations=0);

    ////////////////////////////////////////////////////////////////////////////////
    //! Reinitializes the current algorithm.
    //! @return           Error handling
    ////////////////////////////////////////////////////////////////////////////////
    int reset();

    ////////////////////////////////////////////////////////////////////////////////
    //! Calculates the energy of the original energy functional
    //! @param scale      Currently not used
    //! @return           Remaining energy
    ////////////////////////////////////////////////////////////////////////////////
    float getEnergy(float scale = 1.0f);

  private:
    ////////////////////////////////////////////////////////////////////////////////
    //! Updates internal structures
    ////////////////////////////////////////////////////////////////////////////////
    int updateStructs();

    ////////////////////////////////////////////////////////////////////////////////
    //! Check if IO images are set correctly
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    bool checkIO(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Deletes an image structure
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    void deleteStruct(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Allocates an image structure
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    void allocateStruct(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Resets the memory in an image structure
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    void resetMem(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Updates parameters in an image structure
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    void updateParameters(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Resets internal parameters
    //! @param value   selected structure
    ////////////////////////////////////////////////////////////////////////////////
    void resetInternalParameters(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Updates the image mode
    //! Basically sets all pointers not belonging to the current image mode to 0
    //! @param value   new image mode
    ////////////////////////////////////////////////////////////////////////////////
    void setMode(int value);

    ////////////////////////////////////////////////////////////////////////////////
    //! Helper functions to make running of algorithms more clear
    ////////////////////////////////////////////////////////////////////////////////
    int runTV_L2();
    int runTV_L2(int iterations);
    int runTV_L1();
    int runTV_L1(int iterations);
    int runTV_L1_MIN();
    int runTV_L1_MIN(int iterations);
    int rungTV_L1();
    int rungTV_L1(int iterations);
    int rungTV_L1_MIN();
    int rungTV_L1_MIN(int iterations);
    int rungTV_L1_MAX();
    int rungTV_L1_MAX(int iterations);
    int rungTV_L1_MAX_INP();
    int rungTV_L1_MAX_INP(int iterations);
    int runwTV_MS();
    int runwTV_MS(int iterations);
    int runaniWTV_MS();
    int runaniWTV_MS(int iterations);
    int runSO_L2();
    int runSO_L2(int iterations);
    int runSO_L2_MIN();
    int runSO_L2_MIN(int iterations);
    int runSO_L2_MAX();
    int runSO_L2_MAX(int iterations);
    int runSO_Inp();
    int runSO_Inp(int iterations);
    int runwTV_L2();
    int runwTV_L2(int iterations);
    int runTV_Inp();
    int runTV_Inp(int iterations);
    int rungTV_Inp();
    int rungTV_Inp(int iterations);
    int runD_Inp();
    int runD_Inp(int iterations);
    int runADTV_L2();
    int runADTV_L2(int iterations);
    int rungTV_L1_INP();
    int rungTV_L1_INP(int iterations);

    ////////////////////////////////////////////////////////////////////////////////
    //! Images used in the library
    Cuda::DeviceMemory<float, 2>* u_2d;
    Cuda::DeviceMemory<float, 2>* f_2d;
    Cuda::DeviceMemory<float, 2>* g_2d;

    Cuda::DeviceMemory<float, 3>* u_color;
    Cuda::DeviceMemory<float, 3>* f_color;

    Cuda::DeviceMemory<float, 3>* u_3d;
    Cuda::DeviceMemory<float, 3>* f_3d;
    Cuda::DeviceMemory<float, 3>* g_3d;
    Cuda::DeviceMemory<float, 3>* gz_3d;

    ////////////////////////////////////////////////////////////////////////////////
    //! Model and algorithm selection
    vector<string> model_abrv;
    vector<string> model_desc;
    vector<string> alg_abrv;
    vector<string> alg_desc;
    vector<string> modes_abrv;

    ////////////////////////////////////////////////////////////////////////////////
    //! General parameters
    Algorithm algorithm;
    int old_struct;
    map<Algorithm, int, std::less<Algorithm> > mapping;

    bool verbose;
    bool matlab_output;
    string matlab_file;

    bool realloc_struct;

    unsigned int max_iterations;
    float max_time;
    float convergence_criterion;

    ////////////////////////////////////////////////////////////////////////////////
    //! Algorithm specific parameters
    float lambda;
    float alpha;
    float beta;
    float delta;

    ////////////////////////////////////////////////////////////////////////////////
    //! Logging
    list<float> primal_en_;
    list<float> dual_en_;
    list<float> iterations_;
    list<float> time_;

    ////////////////////////////////////////////////////////////////////////////////
    //! Structs
    TV_L2_2D_Primal    tv_l2_2d_primal;
    TV_L2_2D_Dual      tv_l2_2d_dual;
    TV_L2_3D_Dual      tv_l2_3d_dual;
    TV_L2_2D_Aug       tv_l2_2d_aug;
    TV_L1_2D_Primal    tv_l1_2d_primal;
    TV_L1_2D_Dual      tv_l1_2d_dual;
    TV_L1_2D_Aug       tv_l1_2d_aug;
    wTV_MS_2D_Dual     wtv_ms_2d_dual;
    wTV_MS_3D_Dual     wtv_ms_3d_dual;
    SO_L2_2D_Dual      so_l2_2d_dual;
    SO_INP_2D_Dual     so_inp_2d_dual;
    SO_INP_2D_Alt      so_inp_2d_alt;
    wTV_L2_2D_Dual     wtv_l2_2d_dual;
    TV_INP_2D_Dual     tv_inp_2d_dual;
    INP_2D_Primal      inp_2d_primal;
    ADTV_L2_2D_Qp      adtv_l2_2d_qp;
    TV_L1_INP_2D_Dual  tv_l1_inp_2d_dual;
    TV_L2_2DCol_Dual   tv_l2_2dcol_dual;
    aniWTV_MS_3D_Dual  aniwtv_ms_3d_dual;
};


#endif //VMLIB_H
