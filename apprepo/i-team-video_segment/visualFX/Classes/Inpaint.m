//
//  Inpaint.m
//  visualFX
//
//  Created by Matthias Grundmann on 9/23/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import "Inpaint.h"

#import "Contour.h"
#import "FXImage.h"
#import "FXMatrix.h"
#include "Util.h"

@implementation Inpaint

-(id) initWithPatchRadius:(int)inpaint_radius {
  if (self = [super init]) {
    kInpaintPatchRadius_ = inpaint_radius;
    const int kInpaintPatchDiam = kInpaintPatchRadius_*2 + 1;
    
    gra_x_patch_ = [[FXMatrix alloc] initWithMatrixWidth:kInpaintPatchDiam
                                            matrixHeight:kInpaintPatchDiam
                                              zeroMatrix:false];
    
    gra_y_patch_ = [[FXMatrix alloc] initWithMatrixWidth:kInpaintPatchDiam
                                           matrixHeight:kInpaintPatchDiam
                                             zeroMatrix:false];    
    
    patch_img_ = [[FXImage alloc] initWithImgWidth:kInpaintPatchDiam
                                        imgHeight:kInpaintPatchDiam
                                      imgChannels:4];
    
    patch_mask_ = [[FXImage alloc] initWithImgWidth:kInpaintPatchDiam
                                         imgHeight:kInpaintPatchDiam
                                       imgChannels:1];
  }
  return self;
}

-(void) inpaintImage:(FXImage*)orig_image 
           maskImage:(FXImage*)mask_img
      selectionFrame:(FXRect)selection_frame {
  
  const int kInpaintPatchRadius = kInpaintPatchRadius_;
  const int kInpaintPatchDiam = kInpaintPatchRadius*2 + 1;
  const uint img_width = orig_image.roi.width;
  const uint img_height = orig_image.roi.height;
  
  // Precompute some stuff used for each run.
  [mask_img binaryThreshold:1];
  [mask_img setBorder:kInpaintPatchRadius toConstant:0];
  
  FXMatrix* confidence = [[FXMatrix alloc] 
                          initWithMatrixWidth:selection_frame.width + 2*kInpaintPatchRadius
                          matrixHeight:selection_frame.height + 2*kInpaintPatchRadius
                          zeroMatrix:false];
  
  confidence.roi = FXRect(kInpaintPatchRadius,
                          kInpaintPatchRadius,
                          selection_frame.width,
                          selection_frame.height);
  
  [Contour convertMask:mask_img toConfidence:confidence];
  [confidence setBorder:kInpaintPatchRadius toConstant:1.0];
  
  vector<FXRect> outer_rects;
  vector<const uchar*> inner_positions;
  
  [Contour getValidPositionsFromImage:orig_image
                              andMask:mask_img
                           maskOffset:selection_frame.offset()
                          patchRadius:kInpaintPatchRadius
                        outerPosition:&outer_rects
                       innerPositions:&inner_positions];
  
  
  // Gray image for fast computation of gradients.
  FXImage* gray_img = [[FXImage alloc] initWithImgWidth:img_width
                                              imgHeight:img_height
                                            imgChannels:1
                                            borderWidth:1];
  [orig_image rgbaToGrayImage:gray_img];
  [gray_img copyReplicateBorderPixels:1];
  
  FXMatrix* gradient_x = [[FXMatrix alloc] initWithMatrixWidth:selection_frame.width
                                                  matrixHeight:selection_frame.height
                                                    zeroMatrix:false];
  
  FXMatrix* gradient_y = [[FXMatrix alloc] initWithMatrixWidth:selection_frame.width
                                                  matrixHeight:selection_frame.height
                                                    zeroMatrix:false];
  
  // First run.
  vector<FXPoint> region_outline;
  vector<CGPoint> outline_normals;
  
  [Contour regionBorderFromImage:mask_img 
                    regionBorder:&region_outline 
                   borderNormals:&outline_normals];
  
  vector<CGPoint> image_normals(region_outline.size()); 

  FXRect gray_img_old_roi = gray_img.roi;
  gray_img.roi = selection_frame.shiftOffset(gray_img_old_roi.offset());
  [Contour computeGradientX:gradient_x 
                  gradientY:gradient_y 
                  fromImage:gray_img
                   atPoints:&region_outline
                    results:&image_normals];
  
  gray_img.roi = gray_img_old_roi;
  
  while (!region_outline.empty()) {
    const int num_pts = region_outline.size();
    
    NSLog(@"region border points: %d", num_pts);

    // Determine angle between normal of gradient and outline normal
    vector<float> priorities(num_pts);
    for (int i = 0; i < num_pts; ++i) {
      priorities[i] = fabs(-image_normals[i].y * outline_normals[i].x + 
                            image_normals[i].x * outline_normals[i].y) / 255.0 + 0.001;
    }
    
    vector<float> confidences(num_pts);
    [Contour getConfidence:confidence
                  atPoints:&region_outline
                    radius:kInpaintPatchRadius
                   results:&confidences];
    
    // Multiply priorities and confidences and get maximum
    float max_mutliplied = -1e38;
    int selected_idx;
    
    for (int i = 0; i < num_pts; ++i) {
      float val = priorities[i] * confidences[i];
      if (val > max_mutliplied) {
        max_mutliplied = val;
        selected_idx = i;
      }
    }
    
    FXPoint ref_point = region_outline[selected_idx];
    
    FXRect ref_point_roi = FXRect(ref_point.x - kInpaintPatchRadius,
                                  ref_point.y - kInpaintPatchRadius,
                                  kInpaintPatchDiam,
                                  kInpaintPatchDiam);
    
    // Precompute patch information to find best match more quickly.
    vector<int> img_space_ofs;
    vector<uint> color_values;
    [Contour computePatchInformationFromImage:orig_image
                                      andMask:mask_img
                                   maskOffset:selection_frame.offset()
                                  patchCenter:ref_point
                                  patchRadius:kInpaintPatchRadius
                                 spaceOffsets:&img_space_ofs
                                  colorValues:&color_values];

    // Extract mask around ref_point and mark as processed
    FXRect mask_roi = mask_img.roi;
    mask_img.roi = ref_point_roi.shiftOffset(mask_roi.offset());
    [mask_img copyToImage:patch_mask_];
    
    // Update mask
    [mask_img setToConstant:0];    
    mask_img.roi = mask_roi;
    
    
    // Find best match.
    FXPoint best_match = [Contour findBestMatchInImage:orig_image
                                               inRects:&outer_rects
                                           atPositions:&inner_positions
                                          spaceOffsets:&img_space_ofs
                                           patchValues:&color_values];
    
    if (best_match.x < kInpaintPatchRadius ||
        best_match.y < kInpaintPatchRadius ||
        best_match.x >= img_width - kInpaintPatchRadius ||
        best_match.y >= img_height - kInpaintPatchRadius) {
      NSLog(@"Inpaint:inpaintImage: best_match is outside of image!");
      return;
    }
    
    FXRect best_match_roi = FXRect(best_match.x - kInpaintPatchRadius,
                                   best_match.y - kInpaintPatchRadius,
                                   kInpaintPatchDiam,
                                   kInpaintPatchDiam);
    
    // Compute gradient values from best match
    FXRect gray_roi = gray_img.roi;
    gray_img.roi = best_match_roi.shiftOffset(gray_roi.offset());
    
    [gray_img computeGradientX:gra_x_patch_ gradientY:gra_y_patch_];
    gray_img.roi = gray_roi;
    
    // Copy to local gradient images.
    FXRect gra_roi = gradient_x.roi;
    gradient_x.roi = ref_point_roi.shiftOffset(gra_roi.offset());
    
    [gra_x_patch_ copyToMatrix:gradient_x usingMask:patch_mask_];
    gradient_x.roi = gra_roi;
    
    gra_roi = gradient_y.roi;
    gradient_y.roi = ref_point_roi.shiftOffset(gra_roi.offset());
    
    [gra_y_patch_ copyToMatrix:gradient_y usingMask:patch_mask_];
    gradient_y.roi = gra_roi;    
    
    // Copy Image values from best match
    FXRect img_roi = orig_image.roi;
    orig_image.roi = best_match_roi.shiftOffset(img_roi.offset());
    
    [orig_image copyToImage:patch_img_];
    
    orig_image.roi = ref_point_roi.shiftOffset(selection_frame.offset())
                                  .shiftOffset(img_roi.offset());

    [patch_img_ copyToImage:orig_image usingMask:patch_mask_];
    orig_image.roi = img_roi;
    
    // Update confidence values
    float set_conf = confidences[selected_idx];    
    FXRect conf_roi = confidence.roi;
    confidence.roi = ref_point_roi.shiftOffset(conf_roi.offset());
    [confidence setToConstant:set_conf usingMask:patch_mask_];
    confidence.roi = conf_roi;
    
    // Get new outline;
    [Contour regionBorderFromImage:mask_img 
                      regionBorder:&region_outline 
                     borderNormals:&outline_normals];
    
    // Get new image gradients
    [Contour sampleGradientX:gradient_x 
                   gradientY:gradient_y 
                    atPoints:&region_outline
                     results:&image_normals];
  }
  
  [confidence release];
  [gradient_x release];
  [gradient_y release];
  [gray_img release];    
}

-(void) dealloc {
  [patch_mask_ release];
  [gra_x_patch_ release];
  [gra_y_patch_ release];    
  [patch_img_ release];
  
  [super dealloc];
}

@end
