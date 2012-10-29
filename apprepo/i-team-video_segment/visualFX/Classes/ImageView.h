//
//  ImageView.h
//  visualFX
//
//  Created by Matthias Grundmann on 7/31/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "EAGLView.h"
#include "Util.h"

#include <vector>

using std::vector;
#define kMinSelectDist 3
#define kParticleSize 20.0
#define kLayerImgTileSize 64
#define kLayerClosureRadius 5

@class FXImageInfo;
@class FXImage;
@class TiledImage;

@interface ImageView : EAGLView {
  // Current image to show.
  vector<FXImageInfo*>*     img_infos_;
  FXImageInfo*              selected_img_;
  FXImageInfo*              prev_selected_img_;
  
  // Layer image.
  TiledImage*               layer_img_;
  CGPoint                   layer_top_left_;
  CGPoint                   layer_bottom_right_;
  
  // Reference point for scaling and rotation.
  CGPoint                   ref_point_;
  NSTimeInterval            ref_point_time_;
  NSTimeInterval            last_double_tap_time_;
  
  // Old scale and rotation changes used for smoothing.
  float                     old_scale_diff_;
  float                     old_angle_diff_;
  
  // Number of touches registered.
  uint                      touch_count_;
  
  // Old touch points used in case one touch is hold still.
  CGPoint                   old_touch_point1_;
  CGPoint                   old_touch_point2_;
  
  // Select mode is a generic edit mode. If altered is_selectable will be altered as well.
  // Use is_selectable_ to switch between selection and moving the picture but to retain 
  // the current selection.
  bool                      is_select_mode_;
  bool                      is_selectable_;
    
  // Ensure sampling is not too dense.
  CGPoint                   old_select_pt_;
  
  // Selection animation.
  NSDate*                   select_anim_start_;
  float                     select_alpha_;
  
  // Brush like texture.
  FXImage*                  particle_;
  UIImage*                  particle_orig_;
  FXImage*                  particle_scaled_;
  
  std::vector<CGPoint>* p_;
}

-(void) addImageWithInfo:(FXImageInfo*)img_info;
-(void) removeImageInfo:(FXImageInfo*)img_info;
-(void) selectImage:(FXImageInfo*)img_info;
-(void) updateView;

-(void) scaleAndRotateFromPoint1:(CGPoint)p1
                           point2:(CGPoint)p2
                      prev_point1:(CGPoint)p1_prev
                      prev_point2:(CGPoint)p2_prev;

-(void) setOutline:(vector<CGPoint>*)p;
-(void) scaleParticle:(float)scale;

@property (nonatomic, assign) bool is_select_mode;
@property (nonatomic, assign) bool is_selectable;
@property (nonatomic, readonly) FXImage* selection;
@property (nonatomic, readonly) FXRect selection_frame;

@end
