//
//  PanoClientAppDelegate.h
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright Matthias Grundmann 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

@class StartViewController;
@class ImageViewController;

@class PanoClientUDP;
@class PanoClientTCP;

@interface PanoClientAppDelegate : NSObject <UIApplicationDelegate> {
  UIWindow* start_window_;
  PanoClientUDP* client_udp_;
  PanoClientTCP* client_tcp_;
  
  UINavigationController* navigation_controller_;
  StartViewController*       start_view_controller_;
  ImageViewController*    image_view_controller_;
}

@end

