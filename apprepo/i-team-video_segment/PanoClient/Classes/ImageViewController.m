//
//  ImageViewController.m
//  visualFX
//
//  Created by Matthias Grundmann on 8/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "ImageViewController.h"
#import "ImageView.h"
#import "FXImage.h"
#import "FXImageInfo.h"
#import "FXMatrix.h"

#import "pano_client_tcp.h"
#import "pano_client_udp.h"

#include <vector>
#include <algorithm>

@implementation ImageViewController

@synthesize pano_client_udp=pano_client_udp_, pano_client_tcp=pano_client_tcp_;

// Override initWithNibName:bundle: to load the view using a nib file then perform additional customization that is not appropriate for viewDidLoad.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]) {
      // Custom initialization
      my_pkg_count_ = 0;
      
      pending_lock_ = -1;
      pending_release_ = -1;
    }
    return self;
}



// Implement loadView to create a view hierarchy programmatically.
- (void)loadView {
  CGRect frame = [[UIScreen mainScreen]  applicationFrame];
  ImageView* iv = [[ImageView alloc] initWithFrame:frame];
  iv.controller = self;
  self.view = iv;
  [iv release];
  
  CGRect tool_frame = CGRectMake(0,frame.size.height-kToolbarHeight, frame.size.width,
                                 kToolbarHeight);
  UIToolbar* toolbar = [[UIToolbar alloc] initWithFrame:tool_frame];
  toolbar.barStyle = UIBarStyleBlackTranslucent;
  
  UIBarButtonItem* backButton = [[UIBarButtonItem alloc] 
                                 initWithImage:[UIImage imageNamed:@"backButton.png"]
                                         style:UIBarButtonItemStylePlain 
                                        target:self
                                        action:@selector(backButtonPressed)];
  
  UIBarButtonItem* addButton = [[UIBarButtonItem alloc] 
                                initWithImage:[UIImage imageNamed:@"plusButton.png"]
                                style:UIBarButtonItemStylePlain 
                                target:self
                                action:@selector(addButtonPressed)];  
  /*
  UIBarButtonItem* selectButton = [[UIBarButtonItem alloc] 
                                   initWithImage:[UIImage imageNamed:@"selectButton.png"]
                                   style:UIBarButtonItemStylePlain 
                                   target:self
                                   action:@selector(selectButtonPressed)];
    
  UIBarButtonItem* lockButton = [[UIBarButtonItem alloc] 
                                   initWithImage:[UIImage imageNamed:@"lockButton.png"]
                                   style:UIBarButtonItemStylePlain 
                                   target:self
                                   action:@selector(lockButtonPressed)];
  */
  [toolbar setItems: [NSArray arrayWithObjects:
                              backButton,
                              addButton,
                          //    selectButton,
                          //    lockButton,
                              nil]];
  [iv addSubview:toolbar];
  [toolbar release];
  [backButton release];
  [addButton release];
 // [selectButton release];
 // [lockButton release];
}

- (void) displayImageWithInfo:(FXImageInfo*) img_info {
  [(ImageView*)self.view addImageWithInfo:img_info];	
}

-(void) backButtonPressed {
  [self.navigationController popViewControllerAnimated:YES];
}

- (void) actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
  UIImagePickerControllerSourceType img_src_type;
  if (buttonIndex == 0) 
    img_src_type = UIImagePickerControllerSourceTypeCamera;
  else
    img_src_type =  UIImagePickerControllerSourceTypePhotoLibrary;
  
  if ([UIImagePickerController 
       isSourceTypeAvailable:img_src_type]) {
    UIImagePickerController* img_picker = [[UIImagePickerController alloc] init];
    img_picker.sourceType = img_src_type;
    img_picker.delegate = self;
    img_picker.allowsImageEditing = NO;
    
    [self presentModalViewController:img_picker animated:YES];
    [img_picker release];
  } else {
    UIAlertView* message = 
    [[UIAlertView alloc] initWithTitle:@"Error picking image" 
                               message:@"This image source is not supported by you device." 
                              delegate:self 
                     cancelButtonTitle:@"Dismiss" 
                     otherButtonTitles:nil]; 
    [message show];
    [message release];
  }
}

-(void) addButtonPressed {
  UIActionSheet* question = [[UIActionSheet alloc] initWithTitle:@"Get image from?"
                                                        delegate:self 
                                               cancelButtonTitle:@"Photo Library"
                                          destructiveButtonTitle:@"Camera"
                                               otherButtonTitles:nil];
  [question showInView:self.view];
  [question release];
}

-(void)imagePickerController:(UIImagePickerController*)img_picker
       didFinishPickingImage:(UIImage*)img
                 editingInfo:(NSDictionary*)editingInfo {
  FXImage* fx_img = [[FXImage alloc] initWithCGImage:img.CGImage 
                                         borderWidth:0
                                        maxSize:512
                                         orientation:img.imageOrientation];
  
  FXImageInfo* img_info = [[FXImageInfo alloc] initWithFXImage:fx_img];
  pending_img_ = img_info;
  
  CGImageRef cg_img = [fx_img copyToNewCGImage];
  NSData* png_data = UIImagePNGRepresentation([UIImage imageWithCGImage:cg_img]);
  
  // Send to server via TCP.
  int preload_sz = 2 * sizeof(int) + 4 * sizeof(float);    // 24 bytes on 32 bit.
  
  uchar* data_to_send = (uchar*) malloc([png_data length] + preload_sz);
  
  float scale = img_info.scale;
  float rotation = img_info.rotation;
  float trans_x = img_info.trans_x;
  float trans_y = img_info.trans_y;
  
  *(int*)data_to_send = 0x13030002;
  *(int*)(data_to_send + 4) = [png_data length] + 4 * sizeof(float);
  *(float*)(data_to_send + 8) = scale;
  *(float*)(data_to_send + 12) = rotation;
  *(float*)(data_to_send + 16) = trans_x;
  *(float*)(data_to_send + 20) = trans_y;
  
  memcpy(data_to_send + preload_sz, [png_data bytes], [png_data length]);
  
  NSData* pkg = [NSData dataWithBytesNoCopy:data_to_send length:[png_data length] + preload_sz
                               freeWhenDone:YES];
  [pano_client_tcp_ send:(CFDataRef)pkg];
  
  CGImageRelease(cg_img);
  
  // Wake up to check whether image id was assigned by server.
  [NSTimer scheduledTimerWithTimeInterval:7.5 
                                   target:self 
                                 selector:@selector(imageRegistrationTimePassed:) 
                                 userInfo:nil 
                                  repeats:NO];
  [self displayImageWithInfo:img_info];
  
  /*NSString* msg = [NSString stringWithFormat:@"Tex-scale: %f", img_info.image.tex_scale];  
  UIAlertView* message = 
  [[UIAlertView alloc] initWithTitle:@"Error picking image" 
                             message:msg
                            delegate:self 
                   cancelButtonTitle:@"Dismiss" 
                   otherButtonTitles:nil]; 
  [message show];
  [message release];
  */
  
 // [fx_img release];
  [[img_picker parentViewController] dismissModalViewControllerAnimated: YES];
}
  
-(void)imageRegistrationTimePassed:(NSTimer*)the_timer {
  if (pending_img_.img_id < 0) {
    [(ImageView*)self.view removeImageInfo:pending_img_];
    
    UIAlertView* message = 
    [[UIAlertView alloc] initWithTitle:@"Image transmission error" 
                               message:@"Image was not successfully uploaded to server. Try again."
                              delegate:self 
                     cancelButtonTitle:@"Dismiss" 
                     otherButtonTitles:nil]; 
    [message show];
    [message release];    
  }
  pending_img_ = nil;
}

-(void)aquireImgLockTimePassed:(NSTimer*)the_timer {
  // TODO: Multiple locks shouldn't happen.
  //int img_id = *(int*) [(NSData*)[the_timer userInfo] bytes];
  //FXImageInfo* img_info = [(ImageView*)self.view imgInfoFromId:img_id];

  // Lock authorized by server?
  if ( ((ImageView*)self.view).lock_authorized == NO) {
    [(ImageView*)self.view clearSelectedImage];
  }
}

-(void)releaseImgLockTimePassed:(NSTimer*)the_timer {
  // TODO: Multiple releases shouldn't happen.
  //int img_id = *(int*) [(NSData*)[the_timer userInfo] bytes];  
  
  // Release authorized by server?
  if ( ((ImageView*)self.view).release_authorized == NO) {
    [(ImageView*)self.view resetToOldSelection];
  }
}

-(void)sendFXImageInfo:(FXImageInfo*)img_info {
  // Image already registered at server?
  if (img_info.img_id < 0) {
    return;
  }
      
  // Serialize image id and FXImageInfo.
  NSOutputStream* out = [NSOutputStream outputStreamToMemory];
  [out open];
  
  int msg_id = 0x13030001;
  int msg_length = sizeof(float) * 4 + 2 * sizeof(int);
  
  [out write:(const uint8_t*)&msg_id maxLength:sizeof(int)];
  [out write:(const uint8_t*)&msg_length maxLength:sizeof(int)];
  [out write:(const uint8_t*)&my_pkg_count_ maxLength:sizeof(int)];
  
  ++my_pkg_count_;
  
  int img_id = img_info.img_id;
  float scale = img_info.scale;
  float rotation = img_info.rotation;
  float trans_x = img_info.trans_x;
  float trans_y = img_info.trans_y;
  [out write:(const uint8_t*)&img_id maxLength:sizeof(int)];
  [out write:(const uint8_t*)&scale maxLength:sizeof(float)];
  [out write:(const uint8_t*)&rotation maxLength:sizeof(float)];
  [out write:(const uint8_t*)&trans_x maxLength:sizeof(float)];
  [out write:(const uint8_t*)&trans_y maxLength:sizeof(float)];
  
  CFDataRef data = (CFDataRef) [out propertyForKey: NSStreamDataWrittenToMemoryStreamKey];
  [pano_client_udp_ send:data];
  [out close];
}

-(void)aquireImgLock:(int)img_id {
  NSData* data = [NSData dataWithBytes:&img_id length:sizeof(int)];
  
  // Is another lock still pending? (Some user changed images real fast ...).
  // If so, DO NOT send another request to server.
  if (pending_lock_ < 0) {
    NSOutputStream* out = [NSOutputStream outputStreamToMemory];
    [out open];
    
    int msg_id = 0x13030005;
    int msg_length = sizeof(int);
    
    [out write:(const uint8_t*)&msg_id maxLength:sizeof(int)];
    [out write:(const uint8_t*)&msg_length maxLength:sizeof(int)];
    [out write:(const uint8_t*)&img_id maxLength:sizeof(int)];
    
    CFDataRef data = (CFDataRef) [out propertyForKey: NSStreamDataWrittenToMemoryStreamKey];
    [pano_client_tcp_ send:data];
    [out close];
  }
  
  // Wake up to check whether image could be locked on server.
  [NSTimer scheduledTimerWithTimeInterval:1.4
                                   target:self 
                                 selector:@selector(aquireImgLockTimePassed:) 
                                 userInfo:data 
                                  repeats:NO];
}

-(void)releaseImgLock:(int)img_id {
  NSData* data = [NSData dataWithBytes:&img_id length:sizeof(int)];
  // Is another release still pending? (Some user changed images real fast ...).
  // If so, DO NOT send another request to server.
  if (pending_release_ < 0) {
    NSOutputStream* out = [NSOutputStream outputStreamToMemory];
    [out open];
    
    int msg_id = 0x13030006;
    int msg_length = sizeof(int);
    
    [out write:(const uint8_t*)&msg_id maxLength:sizeof(int)];
    [out write:(const uint8_t*)&msg_length maxLength:sizeof(int)];
    [out write:(const uint8_t*)&img_id maxLength:sizeof(int)];
    
    CFDataRef data = (CFDataRef) [out propertyForKey: NSStreamDataWrittenToMemoryStreamKey];
    [pano_client_tcp_ send:data];
    [out close];
  }
  
  // Wake up to check whether image could be released on server.
  [NSTimer scheduledTimerWithTimeInterval:1.4
                                   target:self 
                                 selector:@selector(releaseImgLockTimePassed:) 
                                 userInfo:data 
                                  repeats:NO];
}

-(void) incomingMessage:(int)msg_id withData:(NSData*)data {
  switch (msg_id) {
    case 0x13030001: {
      const uint8_t* the_data = (uint8_t*) [data bytes];
      
      int client_id = *(int*)the_data;
      the_data += sizeof(int);
      
      int pkg_count = *(int*)the_data;
      the_data += sizeof(int);
      
      vector<HostCount>::iterator i = std::find(host_counts_.begin(), host_counts_.end(),
                                                HostCount(client_id));
      
      if (i == host_counts_.end()) {
        host_counts_.push_back(HostCount(client_id, pkg_count));
      } else if (pkg_count < i->pkg_count) {
        return;
      } else {
        i->pkg_count = pkg_count;
      }
      
      int img_id = *(int*)the_data;
      the_data += sizeof(int);
      
      float scale = *(float*)the_data;
      the_data += sizeof(float);
      
      float rotation = *(float*)the_data;
      the_data += sizeof(float);
      
      float trans_x = *(float*)the_data;
      the_data += sizeof(float);
      
      float trans_y = *(float*)the_data;
      the_data += sizeof(float);
      
      // Update FxImage and repaint.
      FXImageInfo* img_info = [(ImageView*)self.view imgInfoFromId:img_id];
      if (img_info == nil)
        break;
      
      img_info.scale = scale;
      img_info.rotation = rotation;
      img_info.trans_x = trans_x;
      img_info.trans_y = trans_y;
      
      [((ImageView*)self.view) updateView];
      break;
    }
    case 0x13030002:
    {
      // New image from server.
      // Unpack header.
      const uint8_t* the_data = (uint8_t*) [data bytes];
      const int preload_sz = sizeof(int) + 4 * sizeof(float);
      int img_id = *(int*)the_data;
      the_data += sizeof(int);
      
      float scale = *(float*)the_data;
      the_data += sizeof(float);
      
      float rotation = *(float*)the_data;
      the_data += sizeof(float);
      
      float trans_x = *(float*)the_data;
      the_data += sizeof(float);
      
      float trans_y = *(float*)the_data;
      the_data += sizeof(float);
      
      NSData* img_data = [NSData dataWithBytesNoCopy:(void*)the_data length:[data length] - preload_sz];
      
      UIImage* img = [[UIImage imageWithData:img_data] retain];
      FXImage* fx_img = [[FXImage alloc] initWithCGImage:img.CGImage 
                                             borderWidth:0
                                                 maxSize:1024
                                             orientation:img.imageOrientation];
      [img release];
      FXImageInfo* img_info = [[FXImageInfo alloc] initWithFXImage:fx_img];
      img_info.img_id = img_id;
      img_info.scale = scale;
      img_info.rotation = rotation;
      img_info.trans_x = trans_x;
      img_info.trans_y = trans_y;
      
      [self displayImageWithInfo:img_info];
      break;
    }
    case 0x13030003:
    {
      // Img id from server assigned.
      int img_id = *(int*)[data bytes];
      if (pending_img_) {
        pending_img_.img_id = img_id;
      } 
      // else: TODO server thinks image is registered but client removed already.
      // Restarting application helps.
      
      break;
    }
    case 0x13030005:
    {
      const uint8_t* the_data = (uint8_t*) [data bytes];
      int img_id = *(int*)the_data;
      [(ImageView*) self.view lockImage:img_id];
      
      break;
    } 
    case 0x13030006:
    {
      const uint8_t* the_data = (uint8_t*) [data bytes];
      int img_id = *(int*)the_data;
      [(ImageView*) self.view releaseImage:img_id];
      
      break;
    }
    default:
      break;
  }
}

-(void)clearAllImages {
  [(ImageView*)self.view clearAllImages];
}

/*
-(void) selectButtonPressed {
  // TODO: change button selectButton
  // introduce or remove lockButton 
  ((ImageView*)self.view).is_select_mode = !((ImageView*)self.view).is_select_mode;
}

-(void) lockButtonPressed {
  ((ImageView*)self.view).is_selectable = ! ((ImageView*)self.view).is_selectable; 
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
