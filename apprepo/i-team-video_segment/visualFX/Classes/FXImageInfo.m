//
//  FXImageInfo.m
//  visualFX
//
//  Created by Matthias Grundmann on 3/21/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

#import "FXImageInfo.h"
#import "FXImage.h"

#include <algorithm>

@implementation FXImageInfo

@synthesize scale=scale_, rotation=rotation_, trans_x=trans_x_, trans_y=trans_y_,
image=image_;

@dynamic img_width, img_height;

- (float) img_width {
  return image_.roi.width * image_.tex_scale;
}

- (float) img_height {
  return image_.roi.height * image_.tex_scale;
}

- (id) initWithFXImage:(FXImage*)img {
  if (self = [super init]) {
    image_ = [img retain];
    // Compute translation to center image in view
    CGRect bounds = [[UIScreen mainScreen] applicationFrame];
    scale_ = std::min(bounds.size.width / self.img_width,
                      bounds.size.height / self.img_height);
    trans_x_ = (bounds.size.width - (self.img_width * scale_)) * 0.5;
    trans_y_ = (bounds.size.height - (self.img_height * scale_)) * 0.5;
  }
  return self;
}

- (id) initWithFXImage:(FXImage*)img rotation:(float)rot {
  if (self = [super init]) {
    image_ = [img retain];
    // Compute translation to center image in view
    CGRect bounds = [[UIScreen mainScreen] applicationFrame];
    CGPoint min_pt, max_pt;
    rotation_ = rot;
    scale_ = 1.0;
    [self boundingRectMinPt:&min_pt maxPt:&max_pt];
    const float img_width_rot = fabs(max_pt.x - min_pt.x);
    const float img_height_rot = fabs(max_pt.y - min_pt.y);
    rotation_ = 0;
    
    scale_ = std::min(bounds.size.width / img_width_rot,
                      bounds.size.height / img_height_rot);
    trans_x_ = (bounds.size.width - (self.img_width * scale_)) * 0.5;
    trans_y_ = (bounds.size.height - (self.img_height * scale_)) * 0.5;
    
    CGPoint mid_pt = CGPointMake(bounds.size.width * 0.5, bounds.size.height * 0.5);
    CGPoint mid_img = [self frameToImageCoords:mid_pt];
    CGPoint mid_img_rot = CGPointRotate(mid_img, rot);
    
    CGPoint error = CGPointSub(mid_img_rot, mid_img);
    
    error = CGPointScale(error, scale_);
    
    trans_x_ -= error.x;
    trans_y_ -= error.y;
    rotation_ = rot;
  }
  return self;
}

- (CGPoint) frameToImageCoords:(CGPoint)pt {
  float screen_height = [[UIScreen mainScreen] applicationFrame].size.height;
  
  // Convert to openGL coords.
  pt.y = screen_height - 1 - pt.y;
  
  // Undo transformation.
  pt.x -= trans_x_;
  pt.y -= trans_y_;
  
  // Undo scale.
  pt = CGPointScale(pt, 1.0/scale_);
  
  // Undo rotation.
  pt = CGPointRotate(pt, -rotation_);  
  return pt;
}

- (void) boundingRectMinPt:(CGPoint*)out_min_pt maxPt:(CGPoint*)out_max_pt {
  float img_w = self.img_width;
  float img_h = self.img_height;
  
  CGPoint corners[4] = { {0, 0}, {0, img_h}, {img_w, 0}, {img_w, img_h} };
  CGPoint min_pt = CGPointMake(1e38, 1e38);
  CGPoint max_pt = CGPointMake(-1e38, -1e38);
  
  // Transform each point
  for (int i = 0; i < 4;  ++i) {
    corners[i] = CGPointAdd(CGPointScale(CGPointRotate(corners[i], rotation_),
                                         scale_),
                            CGPointMake(trans_x_, trans_y_));
    min_pt.x = std::min(min_pt.x, corners[i].x);
    min_pt.y = std::min(min_pt.y, corners[i].y);
    max_pt.x = std::max(max_pt.x, corners[i].x);
    max_pt.y = std::max(max_pt.y, corners[i].y);
  }
  *out_min_pt = min_pt;
  *out_max_pt = max_pt;
}

- (bool) isPointInImage:(CGPoint)p {
  if (p.x >= 0 && p.y >= 0 && p.x <= self.img_width-1 && p.y <= self.img_height-1)
    return true;
  else
    return false;
}

- (void) dealloc {
  [image_ release];
  [super dealloc];
}

@end
