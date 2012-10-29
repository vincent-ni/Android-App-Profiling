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
@class ImageViewController;

@interface ImageView : EAGLView {
  // Current image to show.
  vector<FXImageInfo*>      img_infos_;
  FXImageInfo*              selected_img_;
  FXImageInfo*              old_selected_img_;
  BOOL                      lock_authorized_;
  BOOL                      release_authorized_;
  
  /*
  // Layer image.
  TiledImage*               layer_img_;
  CGPoint                   layer_top_left_;
  CGPoint                   layer_bottom_right_;
  */
   
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
  
  /*
  // Select mode is a generic edit mode. If altered is_selectable will be altered as well.
  // Use is_selectable_ to switch between selection and moving the picture but to retain 
  // the current selection.
  bool                      is_select_mode_;
  bool                      is_selectable_;
  
  // Selection data.
  vector<float>*            select_scales_;
  vector<vector<CGPoint> >* select_points_;
  
  // Ensure sampling is not too dense.
  CGPoint                   old_select_pt_;
  
  // Brush like texture.
  FXImage*                  particle_;
  UIImage*                  particle_orig_;
  FXImage*                  particle_scaled_;
  */
   
  // Parent controller.
  ImageViewController*      controller_;
}

-(void) addImageWithInfo:(FXImageInfo*)img_info;
-(void) removeImageInfo:(FXImageInfo*)img_info;
-(void) updateView;
-(void) selectionPoints:(const vector<CGPoint>*)sel_points 
              withScale:(float)scale 
          toDensePoints:(vector<CGPoint>*)points;

-(void) scaleAndRotateFromPoint1:(CGPoint)p1
                           point2:(CGPoint)p2
                      prev_point1:(CGPoint)p1_prev
                      prev_point2:(CGPoint)p2_prev;
-(FXImageInfo*) imgInfoFromId:(int)img_id;
-(FXImageInfo*) findClosestImage:(CGPoint)pt;

//-(void) setOutline:(vector<CGPoint>*)p;

//-(void) scaleParticle:(float)scale;

//-(void) updateSelection;

-(void)sendFXImgInfoUpdate;
-(void)aquireLock:(int)img_id;
-(void)releaseLock:(int)img_id;

-(void)lockImage:(int)img_id;
-(void)releaseImage:(int)img_id;
-(void)clearSelectedImage;
-(void)resetToOldSelection;
-(void)makeFrontImage:(FXImageInfo*)img_info;
-(void)clearAllImages;

/*
@property (nonatomic, assign) bool is_select_mode;
@property (nonatomic, assign) bool is_selectable;
@property (nonatomic, readonly) FXImage* selection;
@property (nonatomic, readonly) FXRect selection_frame;

@property (nonatomic, readonly) const vector<float>* select_scales;
@property (nonatomic, readonly) const vector<vector<CGPoint> >* select_points;
*/

@property (nonatomic, readonly) BOOL lock_authorized;
@property (nonatomic, readonly) BOOL release_authorized;
@property (nonatomic, assign) ImageViewController* controller;

@end
