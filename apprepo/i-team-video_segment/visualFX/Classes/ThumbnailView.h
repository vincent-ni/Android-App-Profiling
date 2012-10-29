//
//  ThumbnailView.h
//  visualFX
//
//  Created by Matthias Grundmann on 8/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
 
#define kThumbnailViewCloseButtonSz 25
#define kThumbnailViewBorder 12

@interface ThumbnailView : UIView<UIActionSheetDelegate>
{
  CGImageRef  thumbnail_;
  UIImage*    close_img_;
  UIImage*    close_img_pressed_;
  BOOL        is_close_pressed_;
  BOOL        is_highlighted_;
  
  NSString*   close_title_;
  
  SEL         close_method_;
  id          close_object_;
  id          close_param_;
  SEL         select_method_;
  id          select_object_;
  id          select_param_;
}

- (id) initWithFrame:(CGRect)frame displayImage:(UIImage*)img;
- (void) setTarget:(id)obj closeAction:(SEL)method closeParam:(id)param;
- (void) setTarget:(id)obj selectAction:(SEL)method selectParam:(id)param;

// Resizes the image if new image is passed. Does not retain the original image.  
- (void) setThumbnailFromImage:(UIImage*)img;
// creates a temporary UIImage and is therefore a bit costly.
- (UIImage*) thumbnailImage;

// retains the passed CGImageRef
@property (nonatomic/*,retain*/) CGImageRef thumbnail;
@property (nonatomic, retain) NSString* closeTitle;
@end

