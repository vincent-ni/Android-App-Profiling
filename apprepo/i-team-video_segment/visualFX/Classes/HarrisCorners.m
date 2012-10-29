//
//  HarrisCorners.m
//  visualFX
//
//  Created by Ramkrishan kumar on 8/30/08.
//  Copyright 2008 Ramkrishan kumar. All rights reserved.
//


#import "HarrisCorners.h"
#import "FXMatrix.h"
#include "Util.h"

@implementation HarrisCorners

-(id) init {
	if (self = [super init]) {
		;
	}
	return self;
}


-(void) elementwiseMultiplication:(FXMatrix*)src1
												secondMat:(FXMatrix*)src2
														width:(uint)imagewidth
													 height:(uint)imageheight
												resultMat:(FXMatrix*)dest {
	
	float* src1_data = src1.roi_data;
	float* src2_data = src2.roi_data;
	float* dest_data = dest.roi_data;
	
	const uint src1_pad = src1.roi_pad;
	const uint src2_pad = src2.roi_pad;
	const uint dest_pad = dest.roi_pad;
	
	for (int i = 0;
		i < imageheight;
		i++, src1_data = PtrOffset(src1_data, src1_pad),
		src2_data = PtrOffset(src2_data, src2_pad),
			dest_data = PtrOffset(dest_data, dest_pad)) {
		for (int j = 0; j < imagewidth; j++, ++src1_data, ++src2_data,
				 ++dest_data) {
			*dest_data = *src1_data * *src2_data;
				NSLog(@"multiplied value: %f\n", *dest_data);
		}
	}
}

-(void) elementwiseDivision:(FXMatrix*)src1
									secondMat:(FXMatrix*)src2
											width:(uint)imagewidth
										 height:(uint)imageheight
									resultMat:(FXMatrix*)dest {
	
	float* src1_data = src1.roi_data;
	float* src2_data = src2.roi_data;
	float* dest_data = dest.roi_data;
	
	const uint src1_pad = src1.roi_pad;
	const uint src2_pad = src2.roi_pad;
	const uint dest_pad = dest.roi_pad;
	
	for (int i = 0;
			 i < imageheight;
			 i++, src1_data = PtrOffset(src1_data, src1_pad),
			 src2_data = PtrOffset(src2_data, src2_pad),
			 dest_data = PtrOffset(dest_data, dest_pad)) {
		for (int j = 0; j < imagewidth; j++, ++src1_data, ++src2_data,
				 ++dest_data) {
			*dest_data = *src1_data / *src2_data;
				NSLog(@"division value: %f\n", *dest_data);
		}
	}
}

-(void) elementwiseAddition:(FXMatrix*)src1
									secondMat:(FXMatrix*)src2
											width:(uint)imagewidth
										 height:(uint)imageheight
									resultMat:(FXMatrix*)dest {
	
	float* src1_data = src1.roi_data;
	float* src2_data = src2.roi_data;
	float* dest_data = dest.roi_data;
	
	const uint src1_pad = src1.roi_pad;
	const uint src2_pad = src2.roi_pad;
	const uint dest_pad = dest.roi_pad;
	
	for (int i = 0;
			 i < imageheight;
			 i++, src1_data = PtrOffset(src1_data, src1_pad),
			 src2_data = PtrOffset(src2_data, src2_pad),
			 dest_data = PtrOffset(dest_data, dest_pad)) {
		for (int j = 0; j < imagewidth; j++, ++src1_data, ++src2_data,
				 ++dest_data) {
			*dest_data = *src1_data + *src2_data;
			NSLog(@"summed value: %f\n", *dest_data);
		}
	}
}

-(void) elementwiseSubtraction:(FXMatrix*)src1
									secondMat:(FXMatrix*)src2
											width:(uint)imagewidth
										 height:(uint)imageheight
									resultMat:(FXMatrix*)dest {
	
	float* src1_data = src1.roi_data;
	float* src2_data = src2.roi_data;
	float* dest_data = dest.roi_data;
	
	const uint src1_pad = src1.roi_pad;
	const uint src2_pad = src2.roi_pad;
	const uint dest_pad = dest.roi_pad;
	
	for (int i = 0;
			 i < imageheight;
			 i++, src1_data = PtrOffset(src1_data, src1_pad),
			 src2_data = PtrOffset(src2_data, src2_pad),
			 dest_data = PtrOffset(dest_data, dest_pad)) {
		for (int j = 0; j < imagewidth; j++, ++src1_data, ++src2_data,
				 ++dest_data) {
			*dest_data = *src1_data - *src2_data;
				NSLog(@"subtraction value: %f\n", *dest_data);
		}
	}
}


-(void) nonmaximalSupression:(FXMatrix*)src
						supressionRadius:(int)suprad 
				 supressionThreshold:(float)threshold
									rowsVector:(std::vector<int>*)rows
									colsVector:(std::vector<int>*)cols{
	
	NSLog(@"Before replication\n");
	if (![src hasBorderOfAtLeast:suprad]) {
    NSLog(@"HarrisCorners:nonmaximalSupression: expecting src-matrix with border of %d", suprad);
    return;
  }
	//[src copyReplicateBorderPixels:suprad];
	// Compute the space offset
	const int diam = 2*suprad + 1;
	std::vector<int> space_ofs (diam*diam);
	uint space_sz = 0;
	
	for (int i = -suprad; i <= suprad; i++)
		for (int j = -suprad; j <= suprad; j++) {
			space_ofs[space_sz] = i*src.width_step + j*sizeof(float);
			space_sz++;
		}
	
	float* src_data = src.roi_data;
	const uint src_pad = src.roi_pad;
	for (int i = 0;
		i < src.height;
		i++, src_data = PtrOffset(src_data, src_pad)) {
		for (int j = 0 ; j < src.width; j++, ++src_data) {
			// Extract the neighborhood of the values in which 
			// to find the maximum.
			NSLog(@"The src after replication %f  ", *src_data);
			bool res = true;
			for (int k = 0; k < space_sz; k++) {
				if (*src_data < *PtrOffset(src_data, space_ofs[k]))
					res = false;
			//	NSLog(@"Comparing...centre value %f to neighbor %f", *src_data, *PtrOffset(src_data, space_ofs[k]));
			}
			if(res && (*src_data > threshold)) {
				rows->push_back(i);
				cols->push_back(j);
			}
		}
		NSLog(@"\n");
	}
}


-(void) computeHarrisCorners:(FXImage*)image
								filterRadius:(int)radius
							 sigmaGaussian:(float)sigma
								supressionThreshold:(float)thresh
											supressionRadius:(int)suprad
									resultRows:(std::vector<int>*)rows
									resultCols:(std::vector<int>*)cols {
	
// We first need to get the image derivates in x and y
// direction.
	const int img_width = image.roi.width;
  const int img_height = image.roi.height;
	FXMatrix* gra_X = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                             matrixHeight:img_height + 2*radius
                                               zeroMatrix:false];
  
  FXMatrix* gra_Y = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                             matrixHeight:img_height + 2*radius
                                               zeroMatrix:false];
  
  gra_X.roi = FXRect(radius, radius, img_width, img_height);
  gra_Y.roi = FXRect(radius, radius, img_width, img_height);
  
  [image copyReplicateBorderPixels:1];
  [image computeGradientX:gra_X gradientY:gra_Y];
  
	// Now compute gra_X*gra_X , gra_Y*gra_Y, gra_X*gra_Y
	FXMatrix* gra_X2 = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                                matrixHeight:img_height + 2*radius
                                                  zeroMatrix:false];
	FXMatrix* gra_Y2 = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																							matrixHeight:img_height + 2*radius
																								zeroMatrix:false];
	FXMatrix* gra_XY = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																							matrixHeight:img_height + 2*radius
																								zeroMatrix:false];
	
  gra_X2.roi = FXRect(radius, radius, img_width, img_height);
	gra_Y2.roi = FXRect(radius, radius, img_width, img_height);
	gra_XY.roi = FXRect(radius, radius, img_width, img_height);
  
	// Compute the squared image derivative
	[self elementwiseMultiplication:gra_X
											secondMat:gra_X
													width:img_width
												 height:img_height
												resultMat:gra_X2];
	
	[self elementwiseMultiplication:gra_Y
												secondMat:gra_Y
														width:img_width
													 height:img_height
												resultMat:gra_Y2];
	
	[self elementwiseMultiplication:gra_X
												secondMat:gra_Y
														width:img_width
													 height:img_height
												resultMat:gra_XY];
	
	[gra_X2 copyReplicateBorderPixels:radius];
	[gra_Y2 copyReplicateBorderPixels:radius];
	[gra_XY copyReplicateBorderPixels:radius];
	
	// Release the image gradients
	[gra_X release];
	[gra_Y release];
	
	// Calculate the appropriate size of kernel
	int kernelwidth = 6 * sigma;
	// Make the kernelwidth odd 
	if(kernelwidth % 2 == 0)
		kernelwidth++;
	
	FXMatrix* gra_X2_smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																							matrixHeight:img_height + 2*radius
																								zeroMatrix:false];
	FXMatrix* gra_Y2_smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																							matrixHeight:img_height + 2*radius
																								zeroMatrix:false];
	FXMatrix* gra_XY_smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																							matrixHeight:img_height + 2*radius
																								zeroMatrix:false];
	
	// Smooth the square of the image derivatives
	[gra_X2 gaussianFilterWithSigma:sigma radius:(kernelwidth - 1)/2 outputMatrix:gra_X2_smoothed];
	[gra_Y2 gaussianFilterWithSigma:sigma radius:(kernelwidth - 1)/2 outputMatrix:gra_Y2_smoothed];
	[gra_XY gaussianFilterWithSigma:sigma radius:(kernelwidth - 1)/2 outputMatrix:gra_XY_smoothed];
	
	[gra_X2_smoothed copyReplicateBorderPixels:radius];
	[gra_Y2_smoothed copyReplicateBorderPixels:radius];
	[gra_XY_smoothed copyReplicateBorderPixels:radius];
	
	
	// Release the unsmoothed square of the image derivatives.
	[gra_X2 release];
	[gra_Y2 release];
	[gra_XY release];
	
	// Computing the Harris corner measure
	FXMatrix* gra_X2Y2_smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																											 matrixHeight:img_height + 2*radius
																												 zeroMatrix:false];
	FXMatrix* gra_XYXY_smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																												 matrixHeight:img_height + 2*radius
																													 zeroMatrix:false];
	
	FXMatrix* gra_numerator = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																												 matrixHeight:img_height + 2*radius
																													 zeroMatrix:false];
	
	FXMatrix* gra_denom = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																												 matrixHeight:img_height + 2*radius
																													 zeroMatrix:false];
	// This matrix contains the harris cim measure.
	FXMatrix* harris_cim_measure = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
																								 matrixHeight:img_height + 2*radius
																									 zeroMatrix:false];
	
	
	// Create a matrix of same size as gra_denom. All the values are set to epsilon and 
	// then we add this matrix to	the denominator to avoid division by zero.
	int epsilon_size = (img_width + 2*radius) * (img_height + 2*radius);
	float eps[epsilon_size];
	for (int i = 0; i < epsilon_size; i++)
		eps[i] = 1e-20;
	
	FXMatrix* epsilon = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                               matrixHeight:img_height + 2*radius
                                                 matrixData:eps]; 
	
	[self elementwiseMultiplication:gra_X2_smoothed
												secondMat:gra_Y2_smoothed
														width:img_width
													 height:img_height
												resultMat:gra_X2Y2_smoothed];
	
	[self elementwiseMultiplication:gra_XY_smoothed
												secondMat:gra_XY_smoothed
														width:img_width
													 height:img_height
												resultMat:gra_XYXY_smoothed];
	
	[self elementwiseSubtraction:gra_X2Y2_smoothed
										 secondMat:gra_XYXY_smoothed
												 width:img_width
												height:img_height
										 resultMat:gra_numerator];
	
	[self elementwiseAddition:gra_X2_smoothed
										 secondMat:gra_Y2_smoothed
												 width:img_width
												height:img_height
										 resultMat:gra_denom];
	
	[self elementwiseAddition:gra_denom
									secondMat:epsilon
											width:img_width
										 height:img_height
									resultMat:gra_denom];
	
	[self elementwiseDivision:gra_numerator
									secondMat:gra_denom
											width:img_width
										 height:img_height
									resultMat:harris_cim_measure];
	
	[gra_X2_smoothed release];
	[gra_Y2_smoothed release];
	[gra_X2Y2_smoothed release];
	[gra_XYXY_smoothed release];
	[gra_denom release];
	[epsilon release];
	[gra_numerator release];
	
	// Find local maxima in the neighbourhood of radius suprad 
	// Also check if it is more than a threshold. If it is a local
	// maxima and more than a threshold, it is a harris corner.
		[self nonmaximalSupression:harris_cim_measure
							supressionRadius:suprad
					 supressionThreshold:thresh
										rowsVector:rows
										colsVector:cols];
	
	[harris_cim_measure release];	
}

@end
	
