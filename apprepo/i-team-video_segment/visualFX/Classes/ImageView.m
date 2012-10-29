//
//  ImageView.m
//  visualFX
//
//  Created by Matthias Grundmann on 7/31/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "ImageView.h"
#import "visualFXViewController.h"
#import "FXImage.h"
#import "FXImageInfo.h"
#import "TiledImage.h"

@interface ImageView (PrivateMethods)
-(void)drawGrayOverlayWithAlpha:(float)alpha;
-(void)addParticleToLayer:(CGPoint)where lastPt:(CGPoint)lastPt;
-(void)addParticleToLayerImpl:(CGPoint)where;
-(FXImageInfo*)findClosestImage:(CGPoint)pt;
-(void)makeFrontImage:(FXImageInfo*)img_info;
-(void)selectionAnimation:(NSTimer*)timer;
@end

#define kTargetAlpha 0.75
#define kFadeGray 0.3

@implementation ImageView

@synthesize is_selectable=is_selectable_;
@dynamic is_select_mode, selection, selection_frame;

-(FXImage*) selection {
  if (layer_img_)
    return layer_img_.image;
  else
    return 0;
}

-(FXRect) selection_frame {
  FXRect rc (layer_top_left_.x, 
             layer_top_left_.y, 
             layer_bottom_right_.x - layer_top_left_.x + 1,
             layer_bottom_right_.y - layer_top_left_.y + 1);
  return rc;
}

-(bool) is_select_mode {
  return is_select_mode_;
}

-(void) setIs_select_mode:(bool)mode {
  if (selected_img_ == nil) {
    UIAlertView* message = 
    [[UIAlertView alloc] initWithTitle:@"No image selected!" 
                               message:@"Please select image before switching to highlight mode" 
                              delegate:self 
                     cancelButtonTitle:@"Dismiss" 
                     otherButtonTitles:nil]; 
    [message show];
    [message release];
    return;
  }
  
  is_select_mode_ = mode;
  [layer_img_.image setToConstant:0];
  
  // TODO: debug inpainting.
/*  FXImage* mask_img = [[FXImage alloc] initWithUIImage:[UIImage imageNamed:@"inpaint_mask.png"]
                                           borderWidth:0];
  FXImage* mask_gray = [[FXImage alloc] initWithImgWidth:mask_img.width
                                               imgHeight:mask_img.height
                                             imgChannels:1];
  
  [mask_img rgbaToGrayImage:mask_gray];
  [mask_gray copyToImage:layer_img_.image];
  [mask_img release];
  [mask_gray release];
  */
  
  [layer_img_ loadTo8bitTileSize:kLayerImgTileSize mode:GL_ALPHA];
  layer_top_left_ = CGPointMake(selected_img_.img_width, selected_img_.img_height);
  layer_bottom_right_ = CGPointMake(0, 0);

  is_selectable_ = is_select_mode_;
}

-(id) initWithFrame:(CGRect)frame {
	if((self = [super initWithFrame:frame pixelFormat:GL_RGB565_OES 
                      depthFormat:0 
               preserveBackbuffer:YES])) {
    
    ref_point_time_ = 0;
    last_double_tap_time_ = 0;
    img_infos_ = new vector<FXImageInfo*>();
    
    particle_orig_ = [[UIImage imageNamed:@"ParticleSmall.png"] retain];
    particle_ = [[FXImage alloc] initWithUIImage:particle_orig_ borderWidth:0 alphaSupport:false];
    [particle_ loadTo16bitTexture];
		
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
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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

-(void) addImageWithInfo:(FXImageInfo*)img_info {
  
  if (![img_info.image isTextureLoaded]) {
    if (img_info.image.channels > 1)
      [img_info.image loadTo16bitTexture];
    else
      [img_info.image loadTo8bitTextureMode:GL_LUMINANCE];
  }
  
  img_infos_->push_back([img_info retain]);
  
  [self updateView];
}

-(void) removeImageInfo:(FXImageInfo*)img_info {
  for (vector<FXImageInfo*>::iterator i = img_infos_->begin(); i != img_infos_->end(); ++i) {
    if (*i == img_info) {
      [*i release];
      img_infos_->erase(i);
      [self updateView];
      break;
    }
  }
}

-(void)selectImage:(FXImageInfo*)img_info {
  // Animate based on previous selection.
  
  prev_selected_img_ = selected_img_;
  selected_img_ = img_info;

  // Selection layer.
  if (prev_selected_img_) {
    [layer_img_ release];
    layer_img_ = 0;
  }
  
  if (selected_img_) {
   FXImage* layer = [[FXImage alloc] initWithImgWidth:selected_img_.img_width
                                            imgHeight:selected_img_.img_height
                                            imgChannels:1
                                            borderWidth:2*kLayerClosureRadius];
   
   layer_img_ = [[TiledImage alloc] initWithFXImage:layer];
   [layer release];
  
    self.is_select_mode = false;
    [self scaleParticle:1.0/selected_img_.scale];
  }
  
  select_anim_start_ = [[NSDate date] retain];
  select_alpha_ = 0;  
  
  [NSTimer scheduledTimerWithTimeInterval:0.02 
                                   target:self 
                                 selector:@selector(selectionAnimation:) 
                                 userInfo:nil 
                                  repeats:YES];

}

-(void)selectionAnimation:(NSTimer*)timer{
  NSTimeInterval elapsed = -[select_anim_start_ timeIntervalSinceNow];
  
  if (elapsed > 0.3) {
    [select_anim_start_ release];
    select_anim_start_ = 0;
    [timer invalidate];
    elapsed = 0.3;
  }
  
  select_alpha_ = elapsed / 0.3 * kTargetAlpha;
  if (elapsed > 0.2)
    [self makeFrontImage:selected_img_];
  
  [self updateView];
  
}

-(void)makeFrontImage:(FXImageInfo*)img_info {
  for(vector<FXImageInfo*>::iterator i = img_infos_->begin(); i != img_infos_->end(); ++i) {
    if (*i == img_info) {
      img_infos_->erase(i);
      break;
    }
  }
  
  img_infos_->push_back(img_info);
}

-(void)drawGrayOverlayWithAlpha:(float)alpha {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glColor4f(kFadeGray, kFadeGray, kFadeGray, alpha);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisable(GL_BLEND);      
  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0, 1.0, 1.0, 1.0);
}

- (void)updateView {
  // Clear color based on animation.
  if (selected_img_ || prev_selected_img_) {
    if (prev_selected_img_ == nil) {  // Fade.
      float col = kFadeGray * select_alpha_;
      glClearColor(col, col, col, 1.0);
    } else if (selected_img_ == nil) {  // Unfade.
      float col = kFadeGray * (kTargetAlpha - select_alpha_);
      glClearColor(col, col, col, 1.0);
    } else {    // Keep color.
       glClearColor(kFadeGray, kFadeGray, kFadeGray, 1.0);
    }
  }
  
	glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_BLEND);
  glLoadIdentity();
  
  // Draw images.
  for (vector<FXImageInfo*>::iterator ii = img_infos_->begin(); ii != img_infos_->end(); ++ii) {
    glPushMatrix();
    glTranslatef(ii->trans_x, ii->trans_y, 0);
    glScalef(ii->scale, ii->scale, 1.0);
    glRotatef(ii->rotation / M_PI * 180.0, 0, 0, 1.0);
    GLuint tex_id = ii->image.texture_id;
    glBindTexture(GL_TEXTURE_2D, tex_id);			
    glColor4f(1.0, 1.0, 1.0, 1.0);
    
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
    
    // Anything to animate?
    if (selected_img_ || prev_selected_img_) {
      // Determine case.
      if (prev_selected_img_ == nil) {  // Fade all except selected_img.
        if (*ii != selected_img_) {
          [self drawGrayOverlayWithAlpha:select_alpha_];
        }
      } else if (selected_img_ == nil) {  // Unfade all except prev_selected_img.
        float inv_alpha = kTargetAlpha - select_alpha_;
        if (*ii != prev_selected_img_ && inv_alpha > 0) {
         [self drawGrayOverlayWithAlpha:inv_alpha];
        }
      } else {  // Fade prev_selected_img_, unfade selected_img, keep all remaining ones deselected.
        float inv_alpha = kTargetAlpha - select_alpha_;
        if (*ii == prev_selected_img_) {
          [self drawGrayOverlayWithAlpha:select_alpha_];
        }
        else if (*ii == selected_img_) {
          if (inv_alpha > 0)
            [self drawGrayOverlayWithAlpha:inv_alpha];
        } else {
          [self drawGrayOverlayWithAlpha:kTargetAlpha];
        }
      }
    }
    
    if (*ii == selected_img_ && is_select_mode_) {
      glEnable(GL_BLEND);
      glColor4f(1.0, 0.0, 0.0, .5);  
      [layer_img_ drawImageOpenGL];
      glDisable(GL_BLEND);
    }
      
    glPopMatrix();
  }
  
  /*    
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glColor4f(0, 1, 0, 1);
  if (p_) {
    glVertexPointer(2, GL_FLOAT, 0, &(*p_)[0]);
    glDrawArrays(GL_POINTS, 0, p_->size());  
  }
  */
  
  //Display the buffer
	[self swapBuffers];  
}

- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  // There are two modes global and local transformation mode.
  // The global mode require always two(!) touches and allows for translation
  // This is exclusive and depending on the movement within the first msecs.
  
  // The local mode is only accessible if an object is selected.
  
  if (is_selectable_ && [touches count] == 1 && touch_count_ == 0) {
    touch_count_ = 1;
    CGPoint p = [(UITouch*)[touches anyObject] locationInView:self];
    old_select_pt_ = p;
    p = [selected_img_ frameToImageCoords:p];
    if ([selected_img_ isPointInImage:p]) {
      [self addParticleToLayer:p lastPt:p];
      [self updateView];
    }
  } else if(!is_selectable_) {
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
          if ([t timestamp] - last_double_tap_time_ > 0.5) {
            last_double_tap_time_ = [t timestamp];
            
            if(selected_img_ && 
               [selected_img_ isPointInImage:[selected_img_ frameToImageCoords:ref_point_]]) {
              // Deselect
              [self selectImage:nil];
            } else if (FXImageInfo* selection = [self findClosestImage:ref_point_]) {
              [self selectImage:selection];
              [self updateView];
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
  }
    
  old_scale_diff_ = 1;
  old_angle_diff_ = 0;
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  if (is_selectable_ && [touches count] == 1 && touch_count_ == 1) {
    UITouch* t = [touches anyObject];
    CGPoint pt = [t locationInView: self];
    if (CGPointNorm(CGPointSub(pt, old_select_pt_)) > kMinSelectDist) {
      CGPoint lastPt = [selected_img_ frameToImageCoords:old_select_pt_];
      old_select_pt_ = pt;
      pt = [selected_img_ frameToImageCoords:pt];
      if ([selected_img_ isPointInImage:pt]) {
        [self addParticleToLayer:pt lastPt:lastPt];
        [self updateView];
      }
    }
  }
  
  if (!is_selectable_ && selected_img_) {
    if ([touches count] == 1 && touch_count_ == 1) {
      UITouch* t = [touches anyObject];
      CGPoint p1 = [t locationInView:self];
      CGPoint p2 = [t previousLocationInView:self];
      CGPoint diff = CGPointSub(p1,p2);
      // Convert to OpenGL coordinate system.
      diff.y *= -1.0;
      selected_img_.trans_x += diff.x;
      selected_img_.trans_y += diff.y;
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
    }
  }
}

-(void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  touch_count_ = 0;
  
  if(selected_img_)
    [self scaleParticle:1.0/selected_img_.scale];
  
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

-(FXImageInfo*) findClosestImage:(CGPoint)pt {
  FXImageInfo* closest_img = nil;
  float min_dist = 1e10;
  
  for (vector<FXImageInfo*>::const_iterator i = img_infos_->begin(); i != img_infos_->end(); ++i) {
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

-(void) setOutline:(vector<CGPoint>*)p {
  p_ = new vector<CGPoint>(*p);
}

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

-(void) addParticleToLayer:(CGPoint)where lastPt:(CGPoint)lastPt {
  const int img_height = selected_img_.img_height;
  const int img_width = selected_img_.img_width;
  where.y = img_height - 1 - where.y;
  lastPt.y = img_height - 1 - lastPt.y;
  where = CGPointRound(where);
  lastPt = CGPointRound(lastPt);
  
  [self addParticleToLayerImpl:where];
  const float diam = kParticleSize * 1.0/selected_img_.scale;

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
  const int img_width = selected_img_.img_width;
  const int img_height = selected_img_.img_height;
  
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

- (void)dealloc {
  delete p_;
  
  for (vector<FXImageInfo*>::iterator i = img_infos_->begin(); i != img_infos_->end(); ++i)
    [*i release];
  
  delete img_infos_;
    
  [particle_orig_ release];
  [particle_scaled_ release];
  [particle_ release];
	[super dealloc];
}


@end
