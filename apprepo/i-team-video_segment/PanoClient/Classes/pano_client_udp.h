//
//  pano_client.h
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

@class ImageViewController;

@protocol PanoClientDelegate
-(void) incomingMessage:(int)msg_id withData:(NSData*)data;
@end

@interface PanoClientUDP : NSObject {
  CFSocketRef socket_;  
  id <PanoClientDelegate> delegate_;
  NSDate* last_update_;
  bool connected_;
}

-(id)init;

-(void)message:(NSString*)what;
-(void)connectToPort:(int)port;
-(void)incomingData:(CFDataRef)data;
-(void)send:(CFDataRef)data;
-(NSString*)localIp;

@property (nonatomic, retain) id <PanoClientDelegate> delegate;
@end
