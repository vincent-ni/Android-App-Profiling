//
//  Inpaint.h
//  visualFX
//
//  Created by Matthias Grundmann on 9/23/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import <UIKit/UIKit.h>

@class FXMatrix;
@class FXImage;
struct FXRect;

@interface Inpaint : NSObject {
  int kInpaintPatchRadius_;
  FXMatrix* gra_x_patch_;
  FXMatrix* gra_y_patch_;
  FXImage* patch_img_;
  FXImage* patch_mask_;
}

-(id) initWithPatchRadius:(int)inpaint_radius;

-(void) inpaintImage:(FXImage*)orig_image 
           maskImage:(FXImage*)mask_img
      selectionFrame:(FXRect)selection_frame;


@end
