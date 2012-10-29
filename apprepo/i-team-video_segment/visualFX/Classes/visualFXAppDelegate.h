//
//  visualFXAppDelegate.h
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright __MyCompanyName__ 2008. All rights reserved.
//

#import <UIKit/UIKit.h>

@class VisualFXViewController;
@class ImageViewController;

@interface VisualFXAppDelegate : NSObject <UIApplicationDelegate> {
	UIWindow* window_;
  UINavigationController* navigation_controller_;
	VisualFXViewController* visualfx_controller_;
  ImageViewController*    image_view_controller_;
}

@property (nonatomic, retain) UIWindow* window;
@end

