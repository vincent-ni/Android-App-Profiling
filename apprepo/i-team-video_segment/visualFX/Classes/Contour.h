//
//  Contour.h
//  visualFX
//
//  Created by Matthias Grundmann on 8/25/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import <UIKit/UIKit.h>
@class FXImage;
@class FXMatrix;

#include <vector>
using std::vector;

struct FXPoint;
struct FXRect;
typedef unsigned char uchar;


@interface Contour : NSObject {
}

-(id) init;

// Returns shift between point and image origin.
+(int) createMask:(FXImage**)selection
    fromSelection:(const vector<vector<CGPoint> >*)points
      pointRadius:(const vector<float>*)pt_scales
      pointRadius:(float)pt_radius
     overallScale:(float)scale
           border:(int)border;

// Computes smoothed tangents from a grayscale image by iteratively smoothing with a filter 
// of radius filter_radius. If fill_all is true, smoothing stops when all tangents are non-zero.
+(void) tangentsFromImage:(FXImage*)image 
             filterRadius:(int)rad
               iterations:(int)iter
                  fillAll:(bool)fill_all
                 tangentX:(FXMatrix*)tangent_x 
                 tangentY:(FXMatrix*)tangent_y;

// Returns amount of moved points
+(int) moveContour:(vector<CGPoint>*)contour
          tangentX:(FXMatrix*)tangent_x 
          tangentY:(FXMatrix*)tangent_y
             image:(FXImage*)img
         maxRadius:(int)rad;

+(void) smoothContour:(vector<CGPoint>*)contour
           iterations:(int)iter;

+(void) reparamContour:(vector<CGPoint>*)contour segmentSize:(float)segs_size;

+(void) contourFromImage:(FXImage*)img initialContour:(vector<CGPoint>*)contour;

// Non-Flat erosion. 
// Expects grayscale image, overwrites src_img with result. Uses tmpImage for separable 
// implementation.
+(void) erodeImage:(FXImage*)src_img tmpImage:(FXImage*)dst_img radius:(int)radius;

// Non-Flat dilation.
// Expects grayscale image, overwrites src_img with result. Uses tmpImage for separable 
// implementation.
+(void) dilateImage:(FXImage*)src_img tmpImage:(FXImage*)dst_img radius:(int)radius;

// Detects region border pixels and outputs their coordinates in border. Region is expected
// to be set to 1, background 0.
+(void) regionBorderFromImage:(FXImage*)img 
                 regionBorder:(vector<FXPoint>*)border
                borderNormals:(vector<CGPoint>*)normals;

+(void) computeGradientX:(FXMatrix*)gradient_x 
               gradientY:(FXMatrix*)gradient_y 
               fromImage:(FXImage*)gray_img
                atPoints:(vector<FXPoint>*)points
                 results:(vector<CGPoint>*)results;

+(void) sampleGradientX:(FXMatrix*)gradient_x 
              gradientY:(FXMatrix*)gradient_y 
               atPoints:(vector<FXPoint>*)points
                results:(vector<CGPoint>*)results;

// Expects mask to be 1-channel image with values of 0 and 1 exclusivly.
// Confidence will be set to 1-mask.
+(void) convertMask:(FXImage*)mask toConfidence:(FXMatrix*)confidence;

// Applies bascially a boxfilter to the confidence matrix at the sample points
+(void) getConfidence:(FXMatrix*)confidence 
             atPoints:(vector<FXPoint>*)points
               radius:(int)radius
              results:(vector<float>*)results;

// Gets a vector of valid positions, i.e. the patch around position is completly in the image
// and the intersection with the mask is empty
// Valid positions are represented in two ways, a set of FXRects (for large rectangular areas)
// and a vector of pointer positions for the mask.
+(void) getValidPositionsFromImage:(FXImage*)image 
                           andMask:(FXImage*)mask 
                        maskOffset:(FXPoint)offset
                       patchRadius:(int)radius
                     outerPosition:(vector<FXRect>*)outer
                    innerPositions:(vector<const uchar*>*)inner;

// Return those color values and space offsets for a patch centered at center
// that are not within the current mask. Origin is specified w.r.t to mask
// and uses the color values in image, where the pixel locations are shifted by offset.
+(void) computePatchInformationFromImage:(FXImage*)image
                                 andMask:(FXImage*)mask
                              maskOffset:(FXPoint)offset
                             patchCenter:(FXPoint)center
                             patchRadius:(int)radius
                            spaceOffsets:(vector<int>*)space_ofs
                             colorValues:(vector<uint>*)color_values;

+(FXPoint) findBestMatchInImage:(FXImage*)image
                        inRects:(vector<FXRect>*)rects
                    atPositions:(vector<const uchar*>*)positions
                   spaceOffsets:(vector<int>*)space_ofs
                    patchValues:(vector<uint>*)color_values;

@end
