//
//  ImageView.m
//  visualFX
//
//  Created by Matthias Grundmann on 7/31/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "ImageView.h"

#import "FXImage.h"
#import "FXImageInfo.h"
#import "TiledImage.h"
#import "ImageViewController.h"

@interface ImageView (PrivateMethods)
-(void) addParticleToLayer:(CGPoint)where lastPt:(CGPoint)lastPt;
-(void) addParticleToLayerImpl:(CGPoint)where;
@end

@implementation ImageView

@synthesize controller=controller_, lock_authorized=lock_authorized_,
            release_authorized=release_authorized_;

//@synthesize is_selectable=is_selectable_, img_info=img_info_;
//@dynamic is_select_mode, select_scales, select_points, selection, selection_frame;
/*
-(FXImage*) selection {
  return layer_img_.image;
}

-(FXRect) selection_frame {
  FXRect rc (layer_top_left_.x, 
             layer_top_left_.y, 
             layer_bottom_right_.x - layer_top_left_.x + 1,
             layer_bottom_right_.y - layer_top_left_.y + 1);
  return rc;
}

-(const vector<float>*) select_scales {
  return select_scales_;
}

-(const vector<vector<CGPoint> >*) select_points {
  return select_points_;
}

-(bool) is_select_mode {
  return is_select_mode_;
}

-(void) setIs_select_mode:(bool)mode {
  is_select_mode_ = mode;
  select_scales_->clear();
  select_points_->clear();
  
//  [layer_img_.image setToConstant:0];
  FXImage* mask_img = [[FXImage alloc] initWithUIImage:[UIImage imageNamed:@"inpaint_mask.png"]
                                           borderWidth:0];
  FXImage* mask_gray = [[FXImage alloc] initWithImgWidth:mask_img.width
                                               imgHeight:mask_img.height
                                             imgChannels:1];
  [mask_img rgbaToGrayImage:mask_gray];
  [mask_gray copyToImage:layer_img_.image];
  [mask_img release];
  [mask_gray release];
  
  [layer_img_ loadTo8bitTileSize:kLayerImgTileSize mode:GL_ALPHA];
  layer_top_left_ = CGPointMake(img_info_.img_width, img_info_.img_height);
  layer_bottom_right_ = CGPointMake(0, 0);

  is_selectable_ = is_select_mode_;
  
  [self updateView];
}
 */

-(id) initWithFrame:(CGRect)frame {
	if((self = [super initWithFrame:frame pixelFormat:GL_RGB565_OES 
                      depthFormat:0 
               preserveBackbuffer:YES])) {
  //  select_scales_ = new vector<float>;
  //  select_points_ = new vector<vector<CGPoint> >;
    
  //  particle_orig_ = [[UIImage imageNamed:@"ParticleSmall.png"] retain];
  //  particle_ = [[FXImage alloc] initWithUIImage:particle_orig_ borderWidth:0 alphaSupport:false];
  //  [particle_ loadTo16bitTexture];
		
    selected_img_ = 0;
    ref_point_time_ = 0;
    
    [self setCurrentContext];
		// Enable all openGL extensions we use
    glDisable(GL_DITHER);
    glDisable(GL_LIGHTING);
    
    // Setup standard projection
		glMatrixMode(GL_PROJECTION);
		glOrthof(0, frame.size.width, 0, frame.size.height, -1, 1);
		glMatrixMode(GL_MODELVIEW);
    //Initialize OpenGL states
    glEnable(GL_TEXTURE_2D);
    // Set a blending function to use
 //   glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 //   glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
    // Enable blending
    glEnable(GL_BLEND);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
		glEnable(GL_POINT_SPRITE_OES);
		glTexEnvf(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
    
    [self setMultipleTouchEnabled:YES];
  }
	return self;
}

- (void) addImageWithInfo:(FXImageInfo*)img_info {
  
  if (![img_info.image isTextureLoaded]) {
    if (img_info.image.channels > 1)
      [img_info.image loadTo16bitTexture];
    else
      [img_info.image loadTo8bitTextureMode:GL_LUMINANCE];
  }
  
  img_infos_.push_back([img_info retain]);
  
  if (selected_img_) {
    old_selected_img_ = selected_img_;
    [self releaseLock:selected_img_.img_id];
    selected_img_ = nil;
  }

  /*    [layer_img_ release];
    FXImage* layer = [[FXImage alloc] initWithImgWidth:img_info.img_width
                                             imgHeight:img_info.img_height
                                           imgChannels:1
                                           borderWidth:2*kLayerClosureRadius];
    
    layer_img_ = [[TiledImage alloc] initWithFXImage:layer];
    [layer release];
    */
  
  [self updateView];
  
  // Triggers update view
  //self.is_select_mode = false;
  //[self scaleParticle:1.0/img_info_.scale];
}


- (void)updateView {
	glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_BLEND);
  glLoadIdentity();
  
  glColor4f(1.0, 1.0, 1.0, 1.0);
  
  // Draw images.
  for (vector<FXImageInfo*>::iterator ii = img_infos_.begin(); ii != img_infos_.end(); ++ii) {
    glPushMatrix();
    glTranslatef(ii->trans_x, ii->trans_y, 0);
    glScalef(ii->scale, ii->scale, 1.0);
    glRotatef(ii->rotation / M_PI * 180.0, 0, 0, 1.0);
    GLuint tex_id = ii->image.texture_id;
    glBindTexture(GL_TEXTURE_2D, tex_id);				
    
    GLfloat	 coordinates[] = {0,                      ii->image.tex_coord_y,
                              ii->image.tex_coord_x,	ii->image.tex_coord_y,
                              0,                      0,
                              ii->image.tex_coord_x,	0};
    
    float img_w = ii->img_width;
    float img_h = ii->img_height;
    
    GLfloat	vertices[] = {0,			0,      
                          img_w,	0,      
                          0,  		img_h,	
                          img_w,	img_h };
    
    
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, coordinates);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    if (*ii == selected_img_) {
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glColor4f(1.0, 0, 0, 0.5);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisable(GL_BLEND);      
      glEnable(GL_TEXTURE_2D);
      glColor4f(1.0, 1.0, 1.0, 1.0);
    } else if (ii->locked) {
      glDisable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glColor4f(1.0, 0.941, 0, 0.5);      // Yellow.
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glDisable(GL_BLEND);      
      glEnable(GL_TEXTURE_2D);
      glColor4f(1.0, 1.0, 1.0, 1.0);  
    }
    glPopMatrix();
  }
  /*glEnable(GL_BLEND);
  
  glColor4f(1.0, 0, 0, 0.5);  
  [layer_img_ drawImageOpenGL];
  
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glColor4f(0, 1, 0, 1);
   */
  
  //Display the buffer
	[self swapBuffers];  
}


-(void) selectionPoints:(const vector<CGPoint>*)sel_points 
              withScale:(float)scale 
          toDensePoints:(vector<CGPoint>*)points {
  const float diam = kParticleSize * scale;
  const vector<CGPoint>& cur_pts = *sel_points;
  const int num_pts = cur_pts.size(); 

  points->clear();
  points->reserve (num_pts*4);
  
  for (int j = 0; j < num_pts; ++j) {
    CGPoint p = cur_pts[j];
    points->push_back(p);
    
    if (j+1<num_pts) {
      CGPoint q = cur_pts[j+1];
      const float diff = CGPointNorm(CGPointSub(q,p));
      CGPoint diff_vec = CGPointNormalize(CGPointSub(q,p));
        
      const int steps = ceil(diff/diam*4.0);
      const float diff_step = diff / static_cast<float>(steps);
      float cur_diff = diff_step;
        
      for (int k = 1; k < steps; ++k, cur_diff+=diff_step) {
        CGPoint tmp = CGPointAdd(p, CGPointScale(diff_vec, cur_diff));
        points->push_back(tmp);
      }
    }
  }
}

- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  /*
  if (is_selectable_ && [touches count] == 1 && touch_count_ == 0) {
    touch_count_ = 1;
    select_scales_->push_back(1.0/img_info_.scale);
    select_points_->push_back(vector<CGPoint>());
    CGPoint p = [(UITouch*)[touches anyObject] locationInView:self];
    old_select_pt_ = p;
    p = [img_info_ frameToImageCoords:p];
    if ([img_info_ isPointInImage:p]) {
      select_points_->back().push_back(p);
      [self addParticleToLayer:p lastPt:p];
      [self updateView];
    }
  } else if(!is_selectable_) {*/
  
  if ([touches count] == 2) {
    touch_count_ = 2;
    NSArray* touch_array = [touches allObjects];
    UITouch* t1 = [touch_array objectAtIndex:0];
    UITouch* t2 = [touch_array objectAtIndex:1];
    
    CGPoint p1 = [t1 locationInView:self];
    CGPoint p2 = [t2 locationInView:self];
    ref_point_ = CGPointScale(CGPointAdd(p1, p2), 0.5);
  } else if ([touches count] == 1) {
    // Double tap logic goes here.
    if (touch_count_ == 0) {
      touch_count_ = 1;
      UITouch* t = [touches anyObject];
      
      // Is the touch close the old one and timestamps are close as well?
      if (CGPointNorm(CGPointSub(ref_point_, [t locationInView:self])) < 30 &&
          [t timestamp] - ref_point_time_ < 0.4) {
        
        // Don't allow to fast double taps. We need to synchronize with server.
        if ([t timestamp] - last_double_tap_time_ > 1.5) {
          last_double_tap_time_ = [t timestamp];
          
          BOOL deselected = NO;
          if(selected_img_) {
            CGPoint pt_frame = [selected_img_ frameToImageCoords:ref_point_];
            if ([selected_img_ isPointInImage:pt_frame]) {
              [self releaseLock:selected_img_.img_id];
              old_selected_img_ = selected_img_;
              selected_img_ = nil;
              deselected = YES;
              [self updateView];
            }
          } 
          
          if (deselected == NO) {
            FXImageInfo* selection = [self findClosestImage:ref_point_];
            
            if (selection && selection.locked == NO) {
              if (selected_img_) {
                old_selected_img_ = selected_img_;
                [self releaseLock:selected_img_.img_id];
              }
              
              selected_img_ = selection;
              [self makeFrontImage:selected_img_];
              [self aquireLock:selected_img_.img_id];
              [self updateView];
            }
            /*
            FXImageInfo* old_selected_img = selected_img_;
            selected_img_ = [self findClosestImage:ref_point_];
            if (selected_img_) {
              if (old_selected_img) {
                old_selected_img_ = old_selected_img;
                [self releaseLock:old_selected_img.img_id];
              }
              
              [self makeFrontImage:selected_img_];
              [self aquireLock:selected_img_.img_id];
              [self updateView];
            }
             */
          }
        } 
      }
      
      ref_point_ = [t locationInView:self];
      ref_point_time_ = [t timestamp];
    } else if (touch_count_ == 1) {
      touch_count_ = 2;
      UITouch* t = [touches anyObject];
      ref_point_ = CGPointScale(CGPointAdd(ref_point_, [t locationInView:self]), 0.5);
    }
  }
  //}
  
  old_scale_diff_ = 1;
  old_angle_diff_ = 0;
}
  
-(FXImageInfo*) findClosestImage:(CGPoint)pt {
  
  FXImageInfo* closest_img = nil;
  float min_dist = 1e10;
  
  for (vector<FXImageInfo*>::const_iterator i = img_infos_.begin(); i != img_infos_.end(); ++i) {
    CGPoint p_image= [*i frameToImageCoords:pt];
    if ([*i isPointInImage:p_image]) {
      // Get mid_pt.
      CGPoint min_pt, max_pt;
      [*i boundingRectMinPt:&min_pt maxPt:&max_pt];
      CGPoint mid_pt = CGPointScale(CGPointAdd(min_pt, max_pt), 0.5);
      float dist = CGPointNorm(CGPointSub(mid_pt, pt));
      if (dist < min_dist) {
        closest_img = *i;
        min_dist = dist;
      }
    }
  }
  
  return closest_img;
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
 /* if (is_selectable_ && [touches count] == 1 && touch_count_ == 1) {
    UITouch* t = [touches anyObject];
    CGPoint pt = [t locationInView: self];
    if (CGPointNorm(CGPointSub(pt, old_select_pt_)) > kMinSelectDist) {
      CGPoint lastPt = [img_info_ frameToImageCoords:old_select_pt_];
      old_select_pt_ = pt;
      pt = [img_info_ frameToImageCoords:pt];
      if ([img_info_ isPointInImage:pt]) {
        select_points_->back().push_back(pt);
        [self addParticleToLayer:pt lastPt:lastPt];
        [self updateView];
      }
    }
  }
  */
  if (selected_img_) {
    if ([touches count] == 1 && touch_count_ == 1) {
      UITouch* t = [touches anyObject];
      CGPoint p1 = [t locationInView:self];
      CGPoint p2 = [t previousLocationInView:self];
      CGPoint diff = CGPointSub(p1,p2);
      // Convert to OpenGL coordinate system.
      diff.y *= -1.0;
      selected_img_.trans_x += diff.x;
      selected_img_.trans_y += diff.y;
      [self sendFXImgInfoUpdate];
      [self updateView];
    } else if ([touches count] == 2) {
      // Get both touches.
      NSArray* touch_array = [touches allObjects];
      UITouch* t1 = [touch_array objectAtIndex:0];
      UITouch* t2 = [touch_array objectAtIndex:1];
      
      CGPoint p1 = [t1 locationInView:self];
      CGPoint p2 = [t2 locationInView:self];
      CGPoint p1_prev = [t1 previousLocationInView:self];
      CGPoint p2_prev = [t2 previousLocationInView:self];
      
      [self scaleAndRotateFromPoint1:p1 point2:p2 prev_point1:p1_prev prev_point2:p2_prev];
      [self sendFXImgInfoUpdate];
      old_touch_point1_ = p1;
      old_touch_point2_ = p2;
    } else if ([touches count] == 1 && touch_count_ == 2) {
      UITouch* t = [touches anyObject];
      // Determine predecessor
      CGPoint p = [t locationInView:self];
      CGPoint p_old = [t previousLocationInView:self];
      
      if (CGPointEqualToPoint(p_old,old_touch_point1_)) {
        [self scaleAndRotateFromPoint1:p 
                                point2:old_touch_point2_ 
                           prev_point1:p_old
                           prev_point2:old_touch_point2_];
        old_touch_point1_ = p; 
      } else if (CGPointEqualToPoint(p_old,old_touch_point2_)) {
        [self scaleAndRotateFromPoint1:p 
                                point2:old_touch_point1_ 
                           prev_point1:p_old
                           prev_point2:old_touch_point1_];
        old_touch_point2_ = p;
      }
      [self sendFXImgInfoUpdate];
    }
  }
}

- (void) scaleAndRotateFromPoint1:(CGPoint)p1
                           point2:(CGPoint)p2
                      prev_point1:(CGPoint)p1_prev
                      prev_point2:(CGPoint)p2_prev {
  CGPoint diff = CGPointSub(p1, p2);
  float norm_now = sqrt(diff.x * diff.x + diff.y * diff.y);
  float angle_now = atan2(diff.y, diff.x);
  
  diff = CGPointSub(p1_prev, p2_prev);
  
  float norm_before = sqrt(diff.x * diff.x + diff.y * diff.y);    
  float angle_before = atan2(diff.y, diff.x);
  float scale_diff = norm_now / norm_before;
  float angle_diff = angle_before - angle_now;
  while (angle_diff > M_PI) angle_diff -= 2*M_PI;
  while (angle_diff < -M_PI) angle_diff += 2*M_PI;
  BOOL scale_changed = NO, angle_changed = NO;
  const float filter_weight = 0.3;
  
  if (std::max(1/scale_diff, scale_diff) > 1.002) {
    scale_diff = (1.0-filter_weight)*scale_diff + filter_weight*old_scale_diff_;
    old_scale_diff_ = scale_diff;
    scale_changed = YES;
    
    CGPoint mid = ref_point_; 
    CGPoint mid_img = [selected_img_ frameToImageCoords:mid];
    
    CGPoint error = CGPointScale(mid_img, (scale_diff-1));
    
    // Scale and rotate error.
    error = CGPointScale(error, selected_img_.scale);
    error = CGPointRotate(error, selected_img_.rotation);
    
    selected_img_.trans_x -= error.x;
    selected_img_.trans_y -= error.y;
  }
  
  if (fabs(angle_diff) > 0.002) {
   angle_diff = (1.0-filter_weight)*angle_diff + filter_weight*old_angle_diff_;
    old_angle_diff_ = angle_diff;
    angle_changed = YES;
    
    CGPoint mid = ref_point_;
    CGPoint mid_img = [selected_img_ frameToImageCoords:mid];
    CGPoint mid_img_rot = CGPointRotate(mid_img, angle_diff);
    
    CGPoint error = CGPointSub(mid_img_rot, mid_img);
    
    error = CGPointScale(error, selected_img_.scale);
    error = CGPointRotate(error, selected_img_.rotation);
    
    // Compensate for movement.
    selected_img_.trans_x -= error.x;
    selected_img_.trans_y -= error.y;
  }
  
  if (scale_changed)
    selected_img_.scale *= scale_diff;
  if (angle_changed)
    selected_img_.rotation += angle_diff;
  
  if (scale_changed || angle_changed)
    [self updateView];
  
}

-(FXImageInfo*) imgInfoFromId:(int)img_id {
  for (vector<FXImageInfo*>::const_iterator i = img_infos_.begin(); i != img_infos_.end(); ++i) {
    if (i->img_id == img_id)
      return *i;
  }
  return nil;
}

-(void) removeImageInfo:(FXImageInfo*)img_info {
  for (vector<FXImageInfo*>::const_iterator i = img_infos_.begin(); i != img_infos_.end(); ++i) {
    if (*i == img_info) {
      [*i release];
      img_infos_.erase(i);
      [self updateView];
      break;
    }
  }
}

/*
-(void) scaleParticle:(float)scale {
  int diam = kParticleSize * scale;
  if (diam % 2 == 0)
    ++diam;
  
  FXImage* tmp = [[FXImage alloc] initWithImgWidth:diam imgHeight:diam imgChannels:4];
  CGContextRef bmp_context = [tmp createBitmapContext];
  CGRect rect = {0, 0, diam, diam};
  CGContextDrawImage(bmp_context, rect, particle_orig_.CGImage);
  
  [particle_scaled_ release];
  particle_scaled_ = [[FXImage alloc] initWithImgWidth:diam imgHeight:diam imgChannels:1];
  [tmp rgbaToGrayImage:particle_scaled_];
  CGContextRelease(bmp_context);
  [tmp release];
}

-(void) updateSelection {
  [layer_img_ loadTo8bitTileSize:kLayerImgTileSize mode:GL_ALPHA];
  [self updateView];
}

-(void) addParticleToLayer:(CGPoint)where lastPt:(CGPoint)lastPt {
  const int img_height = img_info_.img_height;
  const int img_width = img_info_.img_width;
  where.y = img_height - 1 - where.y;
  lastPt.y = img_height - 1 - lastPt.y;
  where = CGPointRound(where);
  lastPt = CGPointRound(lastPt);
  
  [self addParticleToLayerImpl:where];
  const float diam = kParticleSize * 1.0/img_info_.scale;

  const float diff = CGPointNorm(CGPointSub(where,lastPt));
  CGPoint diff_vec = CGPointNormalize(CGPointSub(where,lastPt));
      
  const int steps = ceil(diff/diam*4.0);
  const float diff_step = diff / static_cast<float>(steps);
  float cur_diff = diff_step;
      
  for (int k = 1; k < steps; ++k, cur_diff+=diff_step) {
    CGPoint tmp = CGPointAdd(lastPt, CGPointScale(diff_vec, cur_diff));
    [self addParticleToLayerImpl:tmp];
  }
  
  const float tile_size_inv = 1.0/(float)layer_img_.tile_size;
  const int rad = diam/2;
  
  const float min_x = std::max<float>(0, std::min(where.x, lastPt.x) - rad);
  const float max_x = std::min<float>(img_width - 1, std::max(where.x, lastPt.x) + rad);

  const float min_y = std::max<float>(0, std::min(where.y, lastPt.y) - rad);
  const float max_y = std::min<float>(img_height - 1, std::max(where.y, lastPt.y) + rad);
  
  layer_top_left_ = CGPointMin(layer_top_left_, CGPointMake(min_x, min_y));
  layer_bottom_right_ = CGPointMax(layer_bottom_right_, CGPointMake(max_x, max_y));
  
  const int tile_x_1 = min_x * tile_size_inv;
  const int tile_x_2 = max_x * tile_size_inv;
  const int tile_y_1 = min_y * tile_size_inv;
  const int tile_y_2 = max_y * tile_size_inv;
    
  for (int i = tile_y_1; i <= tile_y_2; ++i) 
    for (int j = tile_x_1; j <= tile_x_2; ++j)
      [layer_img_ reloadTileAtX:j andY:i];
}

-(void) addParticleToLayerImpl:(CGPoint)where {
  // Add particle centered at where to layer image
  // Cap values to 255.
  const int diam = particle_scaled_.roi.width;
  const int img_width = img_info_.img_width;
  const int img_height = img_info_.img_height;
  
  const int rad = diam/2;
  
  int start_x = where.x-rad;
  int end_x = where.x+rad;
  
  int start_y = where.y-rad;
  int end_y = where.y+rad;
  
  int off_x = 0, off_y = 0;
  if (start_x  < 0) {
    off_x = -start_x;
    start_x = 0;
  }
  if (start_y < 0) {
    off_y = -start_y;
    start_y = 0;
  }
  
  end_x = std::min(img_width-1, end_x);
  end_y = std::min(img_height-1, end_y);
  
  const int dx = end_x-start_x+1;
  
  uchar* src_ptr = [particle_scaled_ mutableValueAtX:off_x andY:off_y];
  const int src_pad = particle_scaled_.width_step - dx;
  
  uchar* dst_ptr = [layer_img_.image mutableValueAtX:start_x andY:start_y];
  const int dst_pad = layer_img_.image.width_step - dx;
  
  for (int i = start_y; i <= end_y; ++i, src_ptr += src_pad, dst_ptr += dst_pad) {
    for (int j = start_x; j <= end_x; ++j, ++src_ptr, ++dst_ptr) {
      int val = (int)*dst_ptr + (int)*src_ptr;
      if (val > 255) 
        *dst_ptr = 255;
      else
        *dst_ptr = val;
    }
  }
}
*/
-(void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  touch_count_ = 0;
  
//  [self scaleParticle:1.0/img_info_.scale];
  
/*  if ([touches count] == 1) {
    UITouch* t = [touches anyObject];
    CGPoint mid = [t locationInView:self];
    mid = [img_info_ frameToImageCoords:mid];
    uchar* moop = img_info_.image.roi_data + img_info_.image.width_step * (uint)mid.y +
    (uint) mid.x * 4;
    *moop = 255;
    *(moop+1) = 0;
    *(moop+2) = 0;
    [img_info_.image loadTo16bitTexture];
    [self updateView];
  }
  */ 
}

-(void)sendFXImgInfoUpdate {
  if (selected_img_)
    [controller_ sendFXImageInfo:selected_img_];
}
  
-(void)aquireLock:(int)img_id {
  lock_authorized_ = NO;
  [controller_ aquireImgLock:img_id];
}

-(void)releaseLock:(int)img_id {
  release_authorized_ = NO;
  [controller_ releaseImgLock:img_id];
}

-(void)lockImage:(int)img_id {
  FXImageInfo* img_info = [self imgInfoFromId:img_id];
  // Is that the image we selected?
  if (img_info == selected_img_) {
    lock_authorized_ = YES;
  }
  img_info.locked = YES;
  [self updateView];
}

-(void)releaseImage:(int)img_id {
  FXImageInfo* img_info = [self imgInfoFromId:img_id];
  if (img_info == old_selected_img_) {
    release_authorized_ = YES;
  }
  
  img_info.locked = NO;
  [self updateView];
}

-(void)clearSelectedImage {
  selected_img_ = nil;
  [self updateView];
}

-(void)resetToOldSelection {
  selected_img_ = old_selected_img_;
  [self makeFrontImage:selected_img_];
  [self updateView];
}

-(void)makeFrontImage:(FXImageInfo*)img_info {
  for(vector<FXImageInfo*>::iterator i = img_infos_.begin(); i != img_infos_.end(); ++i) {
    if (*i == img_info) {
      img_infos_.erase(i);
      break;
    }
  }
  
  img_infos_.push_back(img_info);
}
  

-(void)clearAllImages {
  for (vector<FXImageInfo*>::iterator i = img_infos_.begin(); i != img_infos_.end(); ++i)
    [*i release];
  img_infos_.clear(); 
}

- (void)dealloc {
//  delete select_points_;
//  delete select_scales_;

  for (vector<FXImageInfo*>::iterator i = img_infos_.begin(); i != img_infos_.end(); ++i)
    [*i release];
    
//  [particle_orig_ release];
//  [particle_scaled_ release];
 // [particle_ release];
	[super dealloc];
}


@end
