//
//  PanoClientAppDelegate.m
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright Matthias Grundmann 2009. All rights reserved.
//

#import "PanoClientAppDelegate.h"
#import "ImageViewController.h"
#import "StartViewController.h"

#import "pano_client_udp.h"
#import "pano_client_tcp.h"

@implementation PanoClientAppDelegate

- (void)applicationDidFinishLaunching:(UIApplication *)application {  
  start_window_ = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  client_udp_ = [[PanoClientUDP alloc] init];
  client_tcp_ = [[PanoClientTCP alloc] init];
  
  image_view_controller_ = [[ImageViewController alloc] initWithNibName:nil bundle:nil];
  image_view_controller_.pano_client_udp = client_udp_;
  image_view_controller_.pano_client_tcp = client_tcp_;
  
  client_udp_.delegate = image_view_controller_;
  client_tcp_.delegate = image_view_controller_;
  
  start_view_controller_ = [[StartViewController alloc] initWithNibName:nil bundle:nil];
  start_view_controller_.pano_client_tcp = client_tcp_;
  start_view_controller_.pano_client_udp = client_udp_;
  start_view_controller_.image_view = image_view_controller_;

  navigation_controller_ = 
    [[UINavigationController alloc] initWithRootViewController:start_view_controller_];
    [navigation_controller_ setNavigationBarHidden: YES];
	
	// Override point for customization after app launch	
  [start_window_ addSubview: navigation_controller_.view];
	[start_window_ makeKeyAndVisible];
}

- (void)dealloc {
  [client_udp_ release];
  [client_tcp_ release];
  [image_view_controller_ release];
  [start_view_controller_ release];
  [navigation_controller_ release];
  [start_window_ release];
  [super dealloc];
}


@end
