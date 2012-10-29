//
//  FXImage.h
//  visualFX
//
//  Created by Matthias Grundmann on 7/10/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>

#include "Util.h"

#define kMaxTextureSize 1024

typedef unsigned char uchar;
@class FXMatrix;

@interface FXImage : NSObject {
  uint       width_;
  uint       height_;
  uint       channels_;
  uint       width_step_;
  uchar*     img_data_;
  FXRect     roi_;
  
  // Texture information.
  GLuint     texture_id_;
  uint       tex_width_;    // width of texture in memory (power of 2)
  uint       tex_height_;   // dito
  float      tex_scale_;    // overall scale factor to fit texture in 1024 x 1024
  
  float      tex_coord_x_;
  float      tex_coord_y_;
}


////////////////////////////////////
// Initialization and data exchange with Quartz.

// Creates a RGBA image from a UIImage - convience function.
-(id) initWithUIImage:(UIImage*)img borderWidth:(uint)border;
-(id) initWithUIImage:(UIImage*)img borderWidth:(uint)border alphaSupport:(bool)alpha;

// Creates a RGBA image from a CGImageRef.
-(id) initWithCGImage:(CGImageRef)img borderWidth:(uint)border;

-(id) initWithCGImage:(CGImageRef)img 
          borderWidth:(uint)border
              maxSize:(uint)sz
          orientation:(UIImageOrientation)dir;

-(id) initWithCGImage:(CGImageRef)img borderWidth:(uint)border alphaSupport:(bool)alpha;

-(id) initWithImgWidth:(uint)width 
              imgHeight:(uint)height
            imgChannels:(uint)channels 
            borderWidth:(uint)border;

-(id) initWithImgWidth:(uint)width imgHeight:(uint)height imgChannels:(uint)channels;

// Returns a BitmapContext to support Quartz drawing.
// Caller is responsible for releasing it. Only supported for 4 channel data.
-(CGContextRef) createBitmapContext;

// Creates a new CGImage from FXImage for visualization.
// Caller is responsible for releasing it.
// ATTENTION: ignores ROI.
-(CGImageRef) copyToNewCGImage;

// Copies ROI image pixels to roi in dst.
-(void) copyToImage:(FXImage*)dst;

-(void) copyToImage:(FXImage*)dst usingMask:(FXImage*)mask;

-(void) setToConstant:(uchar)value;
-(void) setToConstant:(uchar)value usingMask:(FXImage*)mask;

////////////////////////////////////
// Border operations.

// Copies ROI image pixels to roi in dst assuming a square border.
- (void) copyToImage:(FXImage*)dst replicateBorder:(int)border;

// Copies pixels to border, inplace operation. Border values are assigned value of
// closest pixel in image.
-(void) copyReplicateBorderPixels:(int)border;

// Same as above but copies only horizontal or vertical border part.
-(void) copyReplicateBorderPixels:(int)border mode:(BorderMode)mode;

-(void) setBorder:(int)border toConstant:(uchar)value;
-(void) setBorder:(int)border toConstant:(uchar)value mode:(BorderMode)mode;

////////////////////////////////////
// Color conversion.

// Color conversion routines, ignores alpha.
- (void) rgbaToGrayImage:(FXImage*) img;
// Sets alpha to 255.
- (void) grayToRgbaImage:(FXImage*) img;

////////////////////////////////////
// Filter operations

// Computes the gradient of a grayscale image in x and y direction 
// simultaneously via Sobel Filter.
// Expects image to have a border of at least one, DOES NOT call copyReplicateBorderPixels.
- (void) computeGradientX:(FXMatrix*)graX gradientY:(FXMatrix*)graY;

// Compute a Gaussian kernel of a given size and with given sigma
- (void) computeGaussianKernel:(float*)gaussian_kernel
									kernelSize:(int)size
								 kernelSigma:(float)sigma;

// Compute the Gaussian smoothing of a FXMatrix image given the sigma and the 
// size of the gaussian kernel. A call to computeGaussianKernel is made inside
// this function.
-(void) computeGaussianSmoothing:(FXMatrix*)imagemat
											kernelSize:(int)size
										 kernelSigma:(float)sigma;

// Integer based version of the method located in FXMatrix.
// Intended use is small radius, i.e. everything from 1-3.
// If larger radius is need implement separable version.
// Supports grayscale and rgba images.
-(void) gaussianFilterWithSigma:(float)sigma radius:(int)rad outputImage:(FXImage*)output;

-(void) binaryThreshold:(uchar)value;

//////////////////////////
/// Accessor Functions

-(uchar) valueAtX:(uint)x andY:(uint)y;
-(uchar*) mutableValueAtX:(uint)x andY:(uint)y;
-(bool) hasBorderOfAtLeast:(uint)border;

////////////////////////////////////
// OpenGL support functions.
// These function should only be called within a valid context,
// otherwise the application is most likely to crash.

// Loads image to texture and returns the texture_id resp. texture_name.
// Only works on 4 channel images!
-(GLuint) loadTo32bitTexture;

// Same as above. Copies image it to temporal buffer for conversion.
-(GLuint) loadTo16bitTexture;

// For 1 channel images. Does not support resizing images larger than 1024*1024.
// Specify GL_ALPHA or GL_LUMINANCE 
-(GLuint) loadTo8bitTextureMode:(GLenum)target;

// Unloads the texture from graphics memory.
- (void) unloadTexture;

- (BOOL) isTextureLoaded;

@property (nonatomic, readonly) uint width;
@property (nonatomic, readonly) uint height;
@property (nonatomic, readonly) uint channels;
@property (nonatomic, readonly) uint width_step;
@property (nonatomic, readonly) uchar* img_data;
@property (nonatomic, readonly) uchar* roi_data;
@property (nonatomic, readonly) uint roi_pad;
@property (nonatomic, readonly) GLuint texture_id;
@property (nonatomic, readonly) float  tex_coord_x;
@property (nonatomic, readonly) float  tex_coord_y;
@property (nonatomic, readonly) float  tex_scale;
@property (nonatomic) FXRect roi;

@end
