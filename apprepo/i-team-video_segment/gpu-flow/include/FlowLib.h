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
 * Class       : $RCSfile$
 * Language    : C++/CUDA
 * Description : Definition of FlowLib
 *
 * Author     : Manuel Werlberger
 * EMail      : werlberger@icg.tugraz.at
 *
 */

#ifndef FLOWLIB_H
#define FLOWLIB_H

// include definitions
#include <vector>
#include <map>
#include "FlowLibDefs.h"

// forward declarations
class VMLib;
class FlowField;
namespace CudaFlowLib {
  class BaseData;
}



class FLOWLIB_DLLAPI FlowLib
{
public:
  /** Constructor of the Flow library. 
   * @param verbose Defines level of additional information displayed.
   */
  FlowLib(const int verbose = 0);

  /** Destructor of the Flow library. */
  ~FlowLib();

  /** Resets the Flow library to an 'initial' stage. */
  void reset();

  /** Sets level of additional information. */
  void setVerbose(const int verbose = 0){verbose_ = verbose;}

  /** Initializes rainbow color images to visualize flow-field output. */
  void initColorGradient();

  /** Returns if FlowLib is ready or not. */
  inline bool ready(){return ready_;};

  /** Returns the number of images needed with the current algorithm. */
  unsigned int numImagesNeeded();

  /** Calculates the optimal memory size.
   * This is important for the efficiency of the algorithm! Make sure to use this
   * size to create the input device memory as else the memory has to be copied
   * @param size    size of input image
   * @return        optimal size for memory
   */
  Cuda::Size<2> getOptimalMemSize(Cuda::Size<2> size);

  /** Returns the effective image size (region_size). Therefore the flow field is used!
   * @param[in] scale Desired scale.
   */
  Cuda::Size<2> getRegionSize(const unsigned int scale);

  /** Sets two images as initial input to calculate 2-frame-flow
   * @param first First input image.
   * @param second Second input image.
   * @return TRUE if enough images are set an flow can be calculated and FALSE if not.
   */
  bool setInput(Cuda::DeviceMemory<float,2>* first, 
                Cuda::DeviceMemory<float,2>* second);

  /** Sets a new input image to calculate flow on this consecutive frame(s). If no image
   * was set previously the Library does nothing, returns FALSE and waits for another image. If enough
   * images are available to start the algorithm the function returns TRUE.
   * @param image Input image.
   * @param do_not_copy_images Flag that forces just a pointer switch and no copy operation for input images. This flag has to be set only once!
   * @return TRUE if enough images are set an flow can be calculated and FALSE if not.
   */
  bool setInput(Cuda::DeviceMemory<float,2>* image);

  /** Alternative input method.
   * @param buffer Float buffer containing image data.
   * @param width Image width.
   * @param height Image height.
   * @param stride Image pitch. (Number of _bytes_ in a row)
   * @param on_device Flag if buffer resides on the device (TRUE) or on the host (FALSE).
   */
  bool setInput(float* buffer, int width, int height, size_t pitch, bool on_device = true);

  /** Returns the specified input image. 
   * @param[in] num Image number.
   * @param[in] scale Desired scale.
   */
  Cuda::DeviceMemoryPitched<float,2>* getImage(int num, int scale);

  /** Returns the first input image
   * @param[in] scale Desired scale.
   */
  inline Cuda::DeviceMemoryPitched<float,2>* getImageOne(int scale){return this->getImage(1,scale);};

  /** Returns the second input image.
   * @param[in] scale Desired scale.
   */
  inline Cuda::DeviceMemoryPitched<float,2>* getImageTwo(int scale){return this->getImage(2,scale);};
  
  /** Returns the original input image number \a num of the given scale \a scale. */
  Cuda::DeviceMemoryPitched<float,2>* getOriginalImage(unsigned int num, unsigned int scale);

  
  /** Getter for the corresponding original image pyramid.
   * @param[in] index Index of the returned original image pyramid.
   * @returns "Vector" with the corresponding image pyramid.
   * @throw std::runtime_error If something went wrong (e.g. no initialization done yet).
   */
  Cuda::HostMemoryHeap<Cuda::DeviceMemoryPitched<float,2>*,1>* getOriginalImagePyramid(unsigned int index);

  /** Getter for the corresponding filtered image pyramid.
   * @param[in] index Index of the returned image pyramid.
   * @returns "Vector" with the corresponding image pyramid.
   * @throw std::runtime_error If something went wrong (e.g. no initialization done yet).
   */
  Cuda::HostMemoryHeap<Cuda::DeviceMemoryPitched<float,2>*,1>* getFilteredImagePyramid(unsigned int index);

  /** Getter for the corresponding image pyramid.
   * @param[in] index Index of the returned image pyramid.
   * @returns "Vector" with the corresponding image pyramid.
   * @throw std::runtime_error If something went wrong (e.g. no initialization done yet).
   */
  Cuda::HostMemoryHeap<Cuda::DeviceMemoryPitched<float,2>*,1>* getImagePyramid(unsigned int index);

  /** Returns u (the disparities in x-direction) of the given scale. */
  Cuda::DeviceMemoryPitched<float,2>* getFlowU(const unsigned int scale = 0);

  /** Returns v (the disparities in x-direction) of the given scale. */
  Cuda::DeviceMemoryPitched<float,2>* getFlowV(const unsigned int scale = 0);

  /** Returns a u for display purpose (128+u) (the disparities in x-direction) of the given scale. */
  Cuda::DeviceMemoryPitched<unsigned char,2>* getDisplayableFlowU(const unsigned int scale = 0);

  /** Returns a v for display purpose (128+v) (the disparities in y-direction) of the given scale. */
  Cuda::DeviceMemoryPitched<unsigned char,2>* getDisplayableFlowV(const unsigned int scale = 0);

  /** Returns a planar color coded flow field of the given scale. */
  Cuda::DeviceMemoryPitched<float,3>* getColorFlowField(const unsigned int scale);

  /** Copies color coded flow field of the given scale to the given variable \a c_flow. */
  void getColorFlowField(Cuda::DeviceMemory<float,3>* c_flow, const unsigned int scale);

  /** Returns the flow field pyramid. */
  //inline Cuda::HostMemoryHeap1D<FlowField*>* getFlowFieldPyramid(){return data_->getFlowFieldPyramid();};

  /** Writes the current flow field to a .flo file. */
  bool writeFlowField(std::string filename, const unsigned int scale = 0) const;

  /** Warps the given image according the given flow-field.
   * @param[in] image Image that should be warped.
   * @param[out] warped_image Warped output image.
   * @param[in] flow Flow field that is used for the warping process.
   */ 
  bool getWarpedImage(Cuda::DeviceMemoryPitched<float,2> *image, 
                      Cuda::DeviceMemoryPitched<float,2> *warped_image, 
                      Cuda::DeviceMemoryPitched<float,2> *flow_u,
                      Cuda::DeviceMemoryPitched<float,2> *flow_v);

  /** Returns warped image. Currently second image is warped back to the second one. The
   * memory allocation is done internally but no memory management is done
   * afterwards. The ownership of the memory is passed to the caller of this fcn.
   * @todo: define warp direction
   * @param[out] w_image warped image
   */ 
  bool getWarpedImage(Cuda::DeviceMemoryPitched<float,2>** w_image);

  /** Returns warped image (currently first image is warped to second one. 
   * @todo: define warp direction
   * @param[out] w_image warped image
   */ 
  bool getWarpedImage(Cuda::DeviceMemory<float,2>* w_image);

  /** Determines the number of available models
   * @return        number of available models
   */
  int getNumModels(){ return model_abrv_map_.size(); };

  /** Retruns string with description for model num
  * @param model     selected model
  * @return        string containing description
  */
  const std::string getModelDescription(const CudaFlowLib::Model& model);

  /** Retruns string with abbreviation for model num
  * @param num     selected model
  * @return        string containing abbreviation
  */
  const std::string getModelAbbreviation(const CudaFlowLib::Model& mod);

  /** Retruns  map of strings with abbreviations for all available models.  */
  const std::map<CudaFlowLib::Model, std::string>& getModelAbbreviationsMap(){return model_abrv_map_; };

  /** Retruns  vector of strings with abbreviations for all available models.  */
  const std::vector<std::string>& getModelAbbreviations();

  /** Sets Parameter structure for upcoming calculations. */
  void setParameters(const FlowParameters& parameters);

  /** Selects a model that will be minimized.
   * @todo See CUDA_defs.h for typedefs !!
   * @param mod      selected model
   */
  void setModel(const CudaFlowLib::Model& model);
  
  /** Selects a model that will be minimized.
   * @todo See CUDA_defs.h for typedefs !!
   * @param mod      selected model as string -- will be mapped to a Model internally
   */
  void setModel(const std::string& model);

  /** Returns number of images. */
  size_t getNumImages();

  /** Sets number of images used for flow calculations. */
  void setNumImages(const size_t& num_images);

  /** Sets new smoothing parameter lambda. */
  void setLambda(const float& lambda);

  /** Sets parameter for gauss TV. */
  void setGaussTVEpsilon(const float& value);

  /** Sets new iteration amount (external). */
  void setNumIter(const unsigned int& num_iter);
  
  /** Sets new amount of iterations between a warping step. */
  void setNumWarps(const unsigned int& num_warps);

  /** (De-)Activates Median filter while upscaling flow fields. */
  void useMedianFilter(const bool& filter);

  /** (De-)Activates diffusion tensor. */
  void useDiffusionTensor(const bool& flag);

  /** Set smoothing amount for calculating diffusion tensor. */
  void setDiffusionSigma(const float& value);

  /** Set parameter alpha for calculating diffusion tensor. */
  void setDiffusionAlpha(const float& value);

  /** Set parameter q for calculating diffusion tensor. */
  void setDiffusionQ(const float& value);

  /**< (De-)Activates usage textures to rescale buffers. */
  void useTextureRescale(const bool& flag);

  /** Sets number of used scales to build the image pyramid.
   * @param scales Number of scales.
   * @param returns number of scales really used. Might change to a smaller number if given number of scales does not fit the current layout.
   */
  unsigned int setNumScales(const unsigned int& scales);

  /** Sets scale factor for building the image pyramids.
   * @param factor Scale factor used to scale one pyramidal level to the next one.
   * @todo: Posibility to change scale factor on the fly
   */
  void setScaleFactor(const float& factor);

  /** Sets max flow value -- mainly used for normalization while displaying the color flow field. */
  void setMaxFlow(const float& f);

  /** Generic set method for Structure-Texture Decomposition.
   * @param[in] filter Filter method that is used for str.-tex. decomp. (please use all uppercase letters!!)
   * @param[in] weight Defines weighting between structure and texture component.
   * @param[in] smoothing amount of smoothing for str.-tex. decomposition.
   * @return TRUE if state was changed. Means if activate switches the filters state. 
   */
  void setStructureTextureDecomposition(const std::string& filter,
                                        const float& weight, const float& smoothing);

  /** Generic set method for Structure-Texture Decomposition.
   * @param[in] filter Filter method that is used for str.-tex. decomp. (please use all uppercase letters!!)
   * @param[in] weight Defines weighting between structure and texture component.
   * @param[in] smoothing amount of smoothing for str.-tex. decomposition.
   * @return TRUE if state was changed. Means if activate switches the filters state. 
   */
  void setStructureTextureDecomposition(CudaFlowLib::FilterMethod_t filter,
                                        const float& weight, const float& smoothing);

  /** Deactivates Structure-Texture Decomposition */
  void deactivateStructureTextureDecomposition();

  /** Returns filter method for structure-texture decomposition. */
  CudaFlowLib::FilterMethod_t getStrTexFilterMethod();

  /** Returns proportion of structure-texture decomposition. */
  float getStrTexWeight();

  /** Returns smoothing amount of denoising for structure-texture decomposition. */
  float getStrTexSmoothing();

  /** Wrapper for a string to set the warping direction. */
  void setWarpDirection(const std::string &direction);

  /** Sets the warping direction and the flow direction in this sense. */
  void setWarpDirection(CudaFlowLib::WarpDirection_t direction);

  /** Returns the current warp direction. */
  CudaFlowLib::WarpDirection_t getWarpDirection();

  /** Returns number of scales. */
  size_t getNumScales();

  /* @todo: input images not copied but only references switched. */
  // /** Sets flag if images are copied or only the pointers are switched to the new images. 
  //  * @param copy If FALSE the images are copied (default behaviour) and if not the pointers are switched.
  //  */
  // void copyInputImages(const bool &copy = false);
      
  /** Runs optical flow calculations with the specified amount of iterations.
   * If the number of iterations is set to 0, the algorithm will run for the last selected number of iterations.
   * @param iterations number of iterations to calculate
   * @return TRUE if everything was ok and FALSE if not.
   */
  bool runAlgorithm(const int iterations=0);

protected:
  /** Builds corresponding data structures for the used pyramidal representations.
   * @param first First input image.
   * @param second Second input image.
   * @param third Third input image. This one is ignored if a 2-frame model is selected.
   * @return TRUE if everything was ok and FALSE if not.
   */
  bool initData(Cuda::DeviceMemory<float,2>* first=0, 
                Cuda::DeviceMemory<float,2>* second=0,
                Cuda::DeviceMemory<float,2>* third=0);

  private:
  /** Helper functions to run the correct calculations for the selected model. */
  bool run();

  int verbose_; /**< Defines level of additional information. */
  bool ready_; /**< Ready to process data. */

  Cuda::DeviceMemoryLinear<float,1>* gradient_; /** Color gradient for creating colored flow field. */

  FlowParameters flow_params_; /**< Parameters to initialize flow calculation. This is not used if a data structure is available! Then the parameters are set directly. */

  CudaFlowLib::Model model_; /**< Currently selected model. */
  CudaFlowLib::BaseData* data_; /**< Current data structure. */
  std::map<CudaFlowLib::Model, std::string> model_abrv_map_; /**< Model abbrevations list. */
  std::vector<std::string> model_abrvs_; /**< Model abbrevations list. */
  std::map<CudaFlowLib::Model, std::string> model_desc_map_; /**< Model descriptions list. */
  std::map<std::string, CudaFlowLib::Model> abrv_model_map_;  /**< Maps model abbrevations from a string to the Model enum. */
};


#endif //FLOWLIB_H
