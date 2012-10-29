//
//  pano_client.h
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

@class ImageViewController;
@protocol PanoClientDelegate;

#include <vector>
using std::vector;

@interface PanoClientTCP : NSObject {
  CFSocketRef socket_;  
  CFReadStreamRef read_stream_;
  CFWriteStreamRef write_stream_;
  id <PanoClientDelegate> delegate_;

  vector<uint8_t> pkg_;
  int target_size_;
  int msg_id_;
}

-(id)init;

-(void)message:(NSString*)what;
-(void)connectToIpAddress:(NSString*)address onPort:(int)port;
-(void)incomingData:(CFDataRef)data;
-(void)send:(CFDataRef)data;

@property (nonatomic, retain) id <PanoClientDelegate> delegate;
@end
