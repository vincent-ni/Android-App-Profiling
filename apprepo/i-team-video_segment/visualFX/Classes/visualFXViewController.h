//
//  visualFXViewController.h
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright Matthias Grundmann 2008. All rights reserved.
//

#import <UIKit/UIKit.h>

@class ImageViewController;

@interface VisualFXViewController : UIViewController {
	NSMutableArray*       edit_img_list_;
  ImageViewController*  image_view_;
}

-(void)pushImageViewController;

@property (nonatomic, assign) ImageViewController* image_view;


@end

