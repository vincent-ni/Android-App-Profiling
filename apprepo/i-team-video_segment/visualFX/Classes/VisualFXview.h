//
//  mainScreen.h
//  visualFX
//
//  Created by Matthias Grundmann on 6/16/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@class VisualFXViewController;

@interface VisualFXView : UIView <UIImagePickerControllerDelegate,
                                UINavigationControllerDelegate> {
	UIButton*               start_button_;
	UIImageView*            logo_;
	VisualFXViewController* my_controller_;
}

@property (nonatomic, assign) VisualFXViewController* controller;

@end


