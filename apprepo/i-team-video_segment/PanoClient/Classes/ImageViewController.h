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
using std::vector;

@class PanoClientTCP;
@class PanoClientUDP;
#import "pano_client_udp.h"

struct HostCount {
  HostCount(int _host_id, int count = -1) : host_id(_host_id), pkg_count(count) {}
  bool operator==(const HostCount& rhs) { return host_id == rhs.host_id; }
  int host_id;
  int pkg_count;
};

@interface ImageViewController : UIViewController<UIImagePickerControllerDelegate,
                                                  UINavigationControllerDelegate,
                                                  UIActionSheetDelegate,
                                                  PanoClientDelegate> {

  PanoClientUDP* pano_client_udp_;                                                  
  PanoClientTCP* pano_client_tcp_;
  int my_pkg_count_;
  vector<HostCount> host_counts_;
  FXImageInfo* pending_img_;     
                                                    
  int pending_lock_;
  int pending_release_;
}

-(void)displayImageWithInfo:(FXImageInfo*)img_info;
-(void)sendFXImageInfo:(FXImageInfo*)img_info;
-(void)aquireImgLock:(int)img_id;
-(void)releaseImgLock:(int)img_id;
-(void)incomingMessage:(int)msg_id withData:(NSData*)data;
-(void)clearAllImages;

@property (nonatomic, assign) PanoClientTCP* pano_client_tcp;
@property (nonatomic, assign) PanoClientUDP* pano_client_udp;

@end
