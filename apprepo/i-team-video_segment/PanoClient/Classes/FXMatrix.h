//
//  FXMatrix.h
//  visualFX
//
//  Created by Matthias Grundmann on 7/31/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "FXImage.h"

#include "Util.h"

@interface FXMatrix : NSObject {
  uint      width_;
  uint      height_;
  uint      width_step_;
  float*    data_;
  FXRect    roi_;
}

-(id) initWithMatrixWidth:(uint)width matrixHeight:(uint)height zeroMatrix:(bool)zero;

-(id) initWithMatrixWidth:(uint)width matrixHeight:(uint)height matrixData:(float*)data;

// Uses the whole image, sets ROI like in FXImage;
-(id) initWithFXImage:(FXImage*)img;

// Apply variable sized gauss filter to an image
// A good choice for radius is floor(1.5*sigma) -> accounts for 95% of summed probability.
-(void) gaussianFilterWithSigma:(float)sigma radius:(int)rad outputMatrix:(FXMatrix*)output;

// Same as above but uses separable filter. If tmp is zero, a temporary matrix will be
// allocated otherwise the specified one will be used and overwritten.
-(void) gaussianFilterSeparableWithSigma:(float)sigma
                                  radius:(int)rad
                               tmpMatrix:(FXMatrix*)tmp
                            outputMatrix:(FXMatrix*)output;

-(void) copyToMatrix:(FXMatrix*)dst;

-(void) copyToMatrix:(FXMatrix*)dst usingMask:(FXImage*)mask;

-(void) copyToMatrix:(FXMatrix*)dst replicateBorder:(int)border;

-(void) copyReplicateBorderPixels:(int)border;

// Same as above but copies only horizontal or vertical border part.
-(void) copyReplicateBorderPixels:(int)border mode:(BorderMode)mode;

-(void) setBorder:(int)border toConstant:(float)value;
-(void) setBorder:(int)border toConstant:(float)value mode:(BorderMode)mode;

// Convert matrix to image, scales values to lie in interval [0, 255].
-(void) convertToGrayImage:(FXImage*)img;

////////////////////////////////////////
/// Arithmetic and statistical functions

-(void) scaleBy:(float)scale;

-(void) setToConstant:(float)value;
-(void) setToConstant:(float)value usingMask:(FXImage*) mask;

-(void) minimumValue:(float*)p_min_val maximumValue:(float*)p_max_val;

//////////////////////////
/// Accessor functions
-(float) valueAtX:(uint)x andY:(uint)y;
-(float*) mutableValueAtX:(uint)x andY:(uint)y;

-(BOOL) hasBorderOfAtLeast:(uint)border;

@property (nonatomic, readonly) uint width;
@property (nonatomic, readonly) uint height;
@property (nonatomic, readonly) uint width_step;
@property (nonatomic, readonly) float* data;
@property (nonatomic, readonly) float* roi_data;
@property (nonatomic, readonly) uint roi_pad;
@property (nonatomic, assign)   FXRect roi;

@end
