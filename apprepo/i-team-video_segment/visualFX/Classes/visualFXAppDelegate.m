//
//  visualFXAppDelegate.m
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright __MyCompanyName__ 2008. All rights reserved.
//

#import "visualFXAppDelegate.h"
#import "visualFXViewController.h"
#import "ImageViewController.h"

@implementation VisualFXAppDelegate

@synthesize window=window_;

- (void)applicationDidFinishLaunching:(UIApplication *)application {	
	
	// create applications window
	window_ = [[UIWindow alloc] initWithFrame: [[UIScreen mainScreen] bounds]];
	
	// create the main window
  image_view_controller_ = [[ImageViewController alloc] initWithNibName:nil bundle:nil];
	visualfx_controller_ = [[VisualFXViewController alloc] initWithNibName:nil bundle:nil];
  visualfx_controller_.image_view = image_view_controller_;
  
  navigation_controller_ = 
      [[UINavigationController alloc] initWithRootViewController:visualfx_controller_];
  [navigation_controller_ setNavigationBarHidden: YES];
	
	// Override point for customization after app launch	
  [window_ addSubview: navigation_controller_.view];
	[window_ makeKeyAndVisible];
}


- (void)dealloc {
  [image_view_controller_ release];
  [visualfx_controller_ release];
  [navigation_controller_ release];
	[window_ release];
	[super dealloc];
}


@end
