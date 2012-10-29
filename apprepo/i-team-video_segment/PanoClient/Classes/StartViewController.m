//
//  StartView.m
//  PanoClient
//
//  Created by Matthias Grundmann on 3/21/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

#import "StartViewController.h"

#include "pano_client_udp.h"
#include "pano_client_tcp.h"

@implementation StartViewController

@synthesize image_view=image_view_, pano_client_udp=pano_client_udp_, 
            pano_client_tcp=pano_client_tcp_;

 // The designated initializer.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]) {
        // Custom initialization
    }
    return self;
}

// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView {
  CGRect frame = [[UIScreen mainScreen] applicationFrame];
  UIView* start_view = [[UIView alloc] initWithFrame:frame];
  start_view.backgroundColor = [UIColor whiteColor];
  
  CGFloat buttonWidth = 200;
  UIButton* connect_button = [[[UIButton alloc] initWithFrame:
                               CGRectMake(frame.size.width/2-buttonWidth/2, 50, buttonWidth, 50)]
                              autorelease];
  
  [connect_button setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
  [connect_button setTitleColor:[UIColor whiteColor] forState:UIControlStateHighlighted];
  [connect_button setBackgroundImage:[[UIImage imageNamed: @"whiteButton.png"] 
                                      stretchableImageWithLeftCapWidth: 12 
                                      topCapHeight: 0 ]
                            forState:UIControlStateNormal];
  
  [connect_button setBackgroundImage: [[UIImage imageNamed: @"blueButton.png"] 
                                       stretchableImageWithLeftCapWidth: 12 
                                       topCapHeight: 0 ]
                            forState: UIControlStateHighlighted];
  
  [connect_button setTitle:@"Connect to Server" forState:UIControlStateNormal];
  
  [connect_button addTarget:self 
                     action:@selector(connectServer) 
           forControlEvents:UIControlEventTouchUpInside];
  
  [start_view addSubview:connect_button];
  
  
  UIButton* start_button = [[[UIButton alloc] initWithFrame:
                             CGRectMake(frame.size.width/2-buttonWidth/2, 120, buttonWidth, 50)]
                            autorelease];
  
  [start_button setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
  [start_button setTitleColor:[UIColor whiteColor] forState:UIControlStateHighlighted];
  [start_button setBackgroundImage:[[UIImage imageNamed: @"whiteButton.png"] 
                                    stretchableImageWithLeftCapWidth: 12 
                                    topCapHeight: 0 ]
                          forState:UIControlStateNormal];
  
  [start_button setBackgroundImage: [[UIImage imageNamed: @"blueButton.png"] 
                                     stretchableImageWithLeftCapWidth: 12 
                                     topCapHeight: 0 ]
                          forState: UIControlStateHighlighted];
  
  [start_button setTitle:@"Enter collaboration" forState:UIControlStateNormal];
  
  [start_button addTarget:self 
                   action:@selector(pushImageView) 
         forControlEvents:UIControlEventTouchUpInside];
  
  [start_view addSubview:start_button];  

  self.view = start_view;
  [start_view release];
}

-(void)connectServer {
  // Clear everything we loaded.
  [image_view_ clearAllImages];
  
  [self.pano_client_udp connectToPort:34567];
  [self.pano_client_tcp connectToIpAddress:@"10.0.1.2" onPort:34568];
}

-(void)pushImageView {
  [self.navigationController pushViewController:image_view_ animated:YES];
}

- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}


- (void)dealloc {
    [super dealloc];
}


@end
