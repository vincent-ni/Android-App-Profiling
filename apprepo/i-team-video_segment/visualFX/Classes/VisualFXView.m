//
//  mainScreen.m
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TODO: save edited images: UIImageWriteToSavedPhotosAlbum

#import "VisualFXview.h"
#import "visualFXViewController.h"
#import "ThumbnailView.h"

@implementation VisualFXView

@synthesize controller=my_controller_;

- (id)initWithFrame:(CGRect)frame {
	if (self = [super initWithFrame:frame]) {
		// Initialization code
		self.backgroundColor = [UIColor whiteColor];
		CGFloat buttonWidth = 150;
    start_button_ = [[UIButton alloc] initWithFrame: 
                        CGRectMake(frame.size.width/2-buttonWidth/2, 50, buttonWidth, 50)];
    
    [start_button_ setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    [start_button_ setTitleColor:[UIColor whiteColor] forState:UIControlStateHighlighted];
    [start_button_ setBackgroundImage:[[UIImage imageNamed: @"whiteButton.png"] 
                                        stretchableImageWithLeftCapWidth: 12 
                                        topCapHeight: 0 ]
                             forState:UIControlStateNormal];
    
    [start_button_ setBackgroundImage:[[UIImage imageNamed: @"blueButton.png"] 
                                        stretchableImageWithLeftCapWidth: 12 
                                        topCapHeight: 0 ]
                             forState:UIControlStateHighlighted];
    
		[start_button_ setTitle:@"Start" forState:UIControlStateNormal];
		
		[start_button_ addTarget:self 
                      action:@selector(startButtonPressed) 
            forControlEvents:UIControlEventTouchUpInside];
		
		UIImage* logo_image = [UIImage imageNamed:@"visualFX.png"];
		logo_ = [[UIImageView alloc] initWithImage: logo_image];
		
		logo_.frame = CGRectMake(0, 
                             frame.size.height - logo_image.size.height,
                             logo_image.size.width, 
                             logo_image.size.height);
		
		[self addSubview:start_button_];
		[self addSubview:logo_];
  }
  
  return self;
}

- (void)startButtonPressed {
  [self.controller pushImageViewController];
}

- (void)dealloc {
	[start_button_ release];
	[logo_ release];
	[super dealloc];
}


@end
