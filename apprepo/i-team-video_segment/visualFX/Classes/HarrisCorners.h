/*
 *  HarrisCorners.h
 *  visualFX
 *
 *  Created by Ramkrishan kumar on 8/30/08.
 *  Copyright 2008 Ramkrishan kumar. All rights reserved.
 *
 */

#import <UIKit/UIKit.h>

@class FXImage;
@class FXMatrix;

#include <vector>

@interface HarrisCorners : NSObject {
	
}

-(id) init;

// Given two matrices, this function computes their product
// elementwise. It basically mimics the matlab equivalent of
// operator ".*"
-(void) elementwiseMultiplication:(FXMatrix*)src1
												secondMat:(FXMatrix*)src2
														width:(uint)imagewidth
													 height:(uint)imageheight
												resultMat:(FXMatrix*)dest;

// Given two matrices, this function computes their division
// elementwise. It basically mimics the matlab equivalent of
// operator "./"
-(void) elementwiseDivision:(FXMatrix*)src1
												secondMat:(FXMatrix*)src2
														width:(uint)imagewidth
													 height:(uint)imageheight
												resultMat:(FXMatrix*)dest;

// Given two matrices, this function computes their division
// elementwise. It basically mimics the matlab equivalent of
// operator ".+"
-(void) elementwiseAddition:(FXMatrix*)src1
									secondMat:(FXMatrix*)src2
											width:(uint)imagewidth
										 height:(uint)imageheight
									resultMat:(FXMatrix*)dest;

// Given two matrices, this function computes their division
// elementwise. It basically mimics the matlab equivalent of
// operator "./"
-(void) elementwiseSubtraction:(FXMatrix*)src1
									secondMat:(FXMatrix*)src2
											width:(uint)imagewidth
										 height:(uint)imageheight
									resultMat:(FXMatrix*)dest;

// Find the non maximal suppresion within the a specified radius
// If a value is a local maxima and also greater then a certain
// threshold, then we store its row and column position.
// This is used in Harris Conrer detector function.
-(void) nonmaximalSupression:(FXMatrix*)src
						supressionRadius:(int)suprad 
				 supressionThreshold:(float)threshold
									rowsVector:(std::vector<int>*)rows
									colsVector:(std::vector<int>*)cols;

// Computes the Harris Corners of an image. For each of the
// corners, we stroe the (row, col) information.
-(void) computeHarrisCorners:(FXImage*)image
								filterRadius:(int)radius
							 sigmaGaussian:(float)sigma
				 supressionThreshold:(float)thresh
						supressionRadius:(int)suprad
									resultRows:(std::vector<int>*)rows
									resultCols:(std::vector<int>*)cols;
@end


