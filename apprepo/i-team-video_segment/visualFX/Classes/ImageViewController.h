//
//  ImageViewController.h
//  visualFX
//
//  Created by Matthias Grundmann on 8/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#define kToolbarHeight 30

@class FXImageInfo;
#include <vector>

@interface ImageViewController : UIViewController<UIActionSheetDelegate> {
}
-(void) displayImageWithInfo:(FXImageInfo*)img_info;
@end
