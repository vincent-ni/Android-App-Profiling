//
//  ThumbnailView.m
//  visualFX
//
//  Created by Matthias Grundmann on 8/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
#import "ThumbnailView.h"
#import "FXImage.h"

@interface ThumbnailView (PrivateMehtods)
- (CGRect) getInnerFrame;
@end

@implementation ThumbnailView

@synthesize closeTitle=close_title_;
@dynamic thumbnail;

- (CGImageRef) thumbnail {
  return thumbnail_;
}

-(void) setThumbnail:(CGImageRef)cg_img {
  if(cg_img != thumbnail_) {
    CGImageRelease(thumbnail_);
    CGImageRetain(cg_img);
    thumbnail_ = cg_img;
    [self setNeedsDisplay];
  }
}

- (UIImage*) thumbnailImage {
  return [UIImage imageWithCGImage: thumbnail_];
}

- (void) setThumbnailFromImage:(UIImage*)img {
  // Frees local thumbnail
  CGImageRelease(thumbnail_);
  
  if (!img) {
    thumbnail_ = nil;
    return;
  }
  
  // Creates a new thumbnail
  CGRect frame_inner = [self getInnerFrame];
  FXImage* fx_img = [[FXImage alloc] initWithImgWidth:frame_inner.size.width 
                                            imgHeight:frame_inner.size.height
                                          imgChannels:4];
  CGContextRef bmp_context = [fx_img createBitmapContext];
  CGImageRef cg_image = img.CGImage;
  const float img_width = CGImageGetWidth(cg_image);
  const float img_height = CGImageGetHeight(cg_image);
  
  if (img.imageOrientation != UIImageOrientationUp) {
    CGContextTranslateCTM(bmp_context,  frame_inner.size.width*0.5, frame_inner.size.height*0.5);
    switch (img.imageOrientation) {
      case UIImageOrientationDown:
        CGContextRotateCTM(bmp_context, M_PI);
        break;
      case UIImageOrientationLeft:
        CGContextRotateCTM(bmp_context, M_PI * 0.5);
        break;
      case UIImageOrientationRight:
        CGContextRotateCTM(bmp_context, M_PI * 1.5);
        break;
      default:
        NSLog(@"ThumbnailView:setThumbnailFromImage unknown image orientation");
        return;
    }
    CGContextTranslateCTM(bmp_context, -frame_inner.size.width*0.5, -frame_inner.size.height*0.5);
  }
  
  if (img_width <= img_height) {
    float aspect = img_height/img_width;    // >= 1
    float diff = frame_inner.size.width*(aspect-1)/2.0;
    CGRect img_rect = CGRectMake(0, -diff, frame_inner.size.width,
                                 frame_inner.size.width+2*diff);
    CGContextDrawImage (bmp_context, img_rect, cg_image);
  } else {
    float aspect = img_width/img_height;    // >= 1
    float diff = frame_inner.size.height*(aspect-1)/2.0;
    CGRect img_rect = CGRectMake(-diff, 0, frame_inner.size.width+2*diff,
                                 frame_inner.size.width);
    CGContextDrawImage (bmp_context, img_rect, cg_image);
  }
  CGContextRelease (bmp_context);
  
  thumbnail_ = [fx_img copyToNewCGImage];
  [fx_img release];
  [self setNeedsDisplay];
}

- (CGRect) getInnerFrame {
  CGRect frame = self.bounds;
  float border = kThumbnailViewBorder;
  CGRect frame_inner = CGRectMake(border, border, frame.size.width - 2*border, 
                                  frame.size.height - 2*border);
  return frame_inner;
}

- (id) initWithFrame:(CGRect)frame displayImage:(UIImage*)img {
  if (self = [super initWithFrame:frame]) {
    self.backgroundColor = [UIColor whiteColor];
    [self setThumbnailFromImage:img];
    close_img_ = [UIImage imageNamed:@"closeButton.png"];
    close_img_pressed_ = [UIImage imageNamed:@"closeButtonPressed.png"];
    is_close_pressed_ = NO;
    is_highlighted_ = NO;
  }
  return self;
}

- (void) setTarget:(id)obj closeAction:(SEL)method closeParam:(id)param {
  close_object_ = [obj retain];
  close_method_ = method;
  close_param_ = [param retain];
}

- (void) setTarget:(id)obj selectAction:(SEL)method selectParam:(id)param {
  select_object_ = [obj retain];
  select_method_ = method;
  select_param_ = [param retain];
}

- (void) drawRect:(CGRect) rc {
  CGRect frame = self.bounds;
  CGRect frame_inner = [self getInnerFrame];
  
  // Outline rect.
  CGContextRef cg_ref = UIGraphicsGetCurrentContext ();
  CGContextStrokeRect (cg_ref, frame_inner);
  
  // Translate and invert y.
  CGContextTranslateCTM(cg_ref, 0, frame.size.height);
  CGContextScaleCTM(cg_ref, 1.0, -1.0);
  
  if (thumbnail_ != nil) {
    CGContextDrawImage (cg_ref, frame_inner, thumbnail_);
    
    // Blend black rectangle over image in case it is selected.
    if (is_highlighted_) {
      CGFloat fillColor[4] = {0, 0, 0, 0.6};
      CGContextSetFillColor(cg_ref, fillColor);
      CGContextFillRect(cg_ref, frame_inner);
    }
    
    // Draw close button.
    const float closeButtonSz = kThumbnailViewCloseButtonSz;
    CGRect closeButtonRect = CGRectMake (0, frame.size.height-closeButtonSz,
                                         closeButtonSz, closeButtonSz);
    if (is_close_pressed_) 
      CGContextDrawImage(cg_ref, closeButtonRect, close_img_pressed_.CGImage);
    else
      CGContextDrawImage(cg_ref, closeButtonRect, close_img_.CGImage);   
  }
}

- (void) touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  CGRect close_button_rect = CGRectMake(0, 0, kThumbnailViewCloseButtonSz,
                                        kThumbnailViewCloseButtonSz);
  CGRect frame_inner = [self getInnerFrame];
  
  // Get only first touch.
  UITouch* touch = [[touches objectEnumerator] nextObject];
  CGPoint pt = [touch locationInView:self];
  if (CGRectContainsPoint(close_button_rect, pt)) {
    is_close_pressed_ = YES;
    [self setNeedsDisplay];
  } else if (CGRectContainsPoint(frame_inner, pt)) {
    is_highlighted_ = YES;
    [self setNeedsDisplay];
  }
}

- (void) touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  CGRect close_button_rect = CGRectMake(0, 0, kThumbnailViewCloseButtonSz,
                                        kThumbnailViewCloseButtonSz);
  CGRect frame_inner = [self getInnerFrame];
  
  // Get only first touch.
  UITouch* touch = [[touches objectEnumerator] nextObject];
  CGPoint pt = [touch locationInView:self];
  if (CGRectContainsPoint(close_button_rect, pt)) {
    if (!is_close_pressed_) {
      is_close_pressed_ = YES;
      is_highlighted_ = NO;
      [self setNeedsDisplay];
    }
  } else if (CGRectContainsPoint(frame_inner, pt)) {
    if (!is_highlighted_) {
      is_highlighted_ = YES;
      is_close_pressed_ = NO;
      [self setNeedsDisplay];
    }
  } else {
    if (is_close_pressed_) {
      is_close_pressed_ = NO;
      [self setNeedsDisplay];
    }
    if (is_highlighted_) {
      is_highlighted_ = NO;
      [self setNeedsDisplay];
    }
  }
}

- (void) touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  is_close_pressed_ = NO;
  is_highlighted_ = NO;
  [self setNeedsDisplay];
  
  CGRect close_button_rect = CGRectMake(0, 0, kThumbnailViewCloseButtonSz,
                                        kThumbnailViewCloseButtonSz);
  CGRect frame_inner = [self getInnerFrame];
  
  // Get only first touch.
  UITouch* touch = [[touches objectEnumerator] nextObject];
  CGPoint pt = [touch locationInView: self];
  if (CGRectContainsPoint(close_button_rect, pt)) {    
    UIActionSheet* question = [[UIActionSheet alloc] initWithTitle:close_title_
                                                          delegate:self 
                                                 cancelButtonTitle:@"Cancel"
                                            destructiveButtonTitle:@"Remove"
                                                 otherButtonTitles:nil];
    [question showInView:self.superview];
    [question release];
  } else if (CGRectContainsPoint(frame_inner, pt)) {
    if ([select_object_ respondsToSelector:select_method_]) 
      [select_object_ performSelector:select_method_ withObject:select_param_];
    else
      NSLog(@"ThumbnailView target does not respond to selectAction");
  }
}

- (void) actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
  if (buttonIndex == 0) 
    if ([close_object_ respondsToSelector:close_method_])
      [close_object_ performSelector:close_method_ withObject:close_param_];
    else
      NSLog(@"ThumbnailView target does not respond to closeAction");
}

- (void) dealloc {
  CGImageRelease(thumbnail_);
  [close_img_ release];
  [close_img_pressed_ release];
  [close_title_ release];
  
  [close_object_ release];
  [close_param_ release];
  [select_object_ release];
  [select_param_ release];
  
  [super dealloc];
}

@end
