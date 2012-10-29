//
//  StartView.h
//  PanoClient
//
//  Created by Matthias Grundmann on 3/21/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

#import <UIKit/UIKit.h>
@class PanoClientTCP;
@class PanoClientUDP;
@class ImageViewController;

@interface StartViewController : UIViewController {
  ImageViewController* image_view_;
  PanoClientUDP* pano_client_udp_;
  PanoClientTCP* pano_client_tcp_;
}

-(void)connectToServer;
-(void)pushImageView;

@property (nonatomic, assign) ImageViewController* image_view;
@property (nonatomic, assign) PanoClientUDP* pano_client_udp;
@property (nonatomic, assign) PanoClientTCP* pano_client_tcp;

@end
