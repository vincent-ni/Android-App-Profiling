//
//  visualFXAppDelegate.m
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright __MyCompanyName__ 2008. All rights reserved.
//

#import "visualFXViewController.h"
#import "VisualFXview.h"
#import "FXImage.h"
#import "ImageViewController.h"

#import "FXImageInfo.h"

#include <algorithm>

@implementation VisualFXViewController

@synthesize image_view=image_view_;

- (id)initWithNibName:(NSString*) nibName bundle:(NSBundle*) nibBundle {
  if (self = [super initWithNibName: nibName bundle: nibBundle]) {
    
  }
  return self;
}

// Implement loadView if you want to create a view hierarchy programmatically
- (void)loadView {
	VisualFXView* welcomeScreen = [[VisualFXView alloc] 
                               initWithFrame:[[UIScreen mainScreen] applicationFrame]];
	welcomeScreen.controller = self;
	self.view = welcomeScreen;
	[welcomeScreen release];
}

-(void)pushImageViewController {
  [self.navigationController pushViewController:image_view_ animated:YES];
}

/*
 Implement viewDidLoad if you need to do additional setup after loading the view.
- (void)viewDidLoad {
	[super viewDidLoad];
}
 */


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	// Return YES for supported orientations
	return (interfaceOrientation == UIInterfaceOrientationPortrait);
}


- (void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning]; // Releases the view if it doesn't have a superview
	// Release anything that's not essential, such as cached data
}


- (void)dealloc {

	[super dealloc];
}

@end
