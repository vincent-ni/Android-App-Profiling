//
//  pano_client.m
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

#import "pano_client_tcp.h"
#import "pano_client_udp.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <CFNetwork/CFSocketStream.h>


@implementation PanoClientTCP
@synthesize delegate=delegate_;

-(id)init {
  if (self = [super init]) {
    socket_ = 0;
    read_stream_ = 0;
    write_stream_ = 0;
  }
  return self;
}

-(void)message:(NSString*)what {
  UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"PanoClient"
                                                  message:what
                                                 delegate:nil
                                        cancelButtonTitle:@"OK"
                                        otherButtonTitles:nil];
  [alert show];
  [alert release];
}

static void SocketCallBack(CFSocketRef socket, 
                           CFSocketCallBackType type,
                           CFDataRef address,
                           const void *data,
                           void *info) {
  PanoClientTCP* client = (PanoClientTCP*) info;
  if (type == kCFSocketDataCallBack) {
    CFDataRef the_data = (CFDataRef) data;
    [client incomingData:the_data];
  }
}

-(void)connectToIpAddress:(NSString*)ip_address onPort:(int)port {
  
  if (socket_) {
    CFSocketInvalidate(socket_);
    CFRelease(socket_);
  }
  
  CFSocketContext socketCtxt = {0, self, NULL, NULL, NULL};
  socket_ = CFSocketCreate(kCFAllocatorDefault,
                           PF_INET,
                           SOCK_STREAM,
                           IPPROTO_TCP, 
                           kCFSocketDataCallBack,
                           (CFSocketCallBack)&SocketCallBack, 
                           &socketCtxt);
  
  if (socket_ == NULL) {
    [self message:@"Can't create TCP socket!"];
    return;
  }
  
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = inet_addr([ip_address UTF8String]);
  
  CFDataRef address = CFDataCreate(NULL, (unsigned char*)&sin, sizeof(sin));
  CFTimeInterval timeout = 3;
  
  CFSocketError e = CFSocketConnectToAddress(socket_, address, timeout);
  CFRelease(address);
  
  switch (e) {
    case kCFSocketError:
      [self message:@"Can't connect to server. Server running?"];
      CFRelease(socket_);
      socket_ = 0;
      return;
    case kCFSocketTimeout:
      [self message:@"Time out while connecting to server"];
      CFRelease(socket_);
      socket_ = 0;
      return;
    default:
      break;
  }
  
  // Schedule socket in run-loop.
  CFRunLoopRef run_loop = CFRunLoopGetCurrent();
  CFRunLoopSourceRef new_source = CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket_, 0);
  CFRunLoopAddSource(run_loop, new_source, kCFRunLoopCommonModes);
  CFRelease(new_source);
  
  [self message: @"TCP connection established!"];
}

-(void)incomingData:(CFDataRef)data {
  int len = CFDataGetLength(data);
  int bytes_processed = 0;
  const uint8_t* the_data = CFDataGetBytePtr(data);

  // Multiple packages possible.
  while (bytes_processed < len) {
    // Continue package.
    if (pkg_.size()) {
      int remaining_data_bytes = len - bytes_processed;
      int remaining_bytes = target_size_ - pkg_.size();
      if (remaining_data_bytes <= remaining_bytes) {
        int start_idx = pkg_.size();
        pkg_.resize(pkg_.size() + remaining_data_bytes);
        memcpy(&pkg_[start_idx], the_data + bytes_processed, remaining_data_bytes);
        bytes_processed += remaining_data_bytes;
      } else {
        // Another package is contained in same data.
        int start_idx = pkg_.size();
        pkg_.resize(pkg_.size() + remaining_bytes);
        memcpy(&pkg_[start_idx], the_data + bytes_processed, remaining_bytes);
        bytes_processed += remaining_bytes;
      }
    } else {
      // New package.
      // Snoop in header.
      int msg_id = *(int*)(the_data + bytes_processed);
      bytes_processed += sizeof(int);
      
      int msg_len = *(int*)(the_data + bytes_processed);
      bytes_processed += sizeof(int);
      
      switch(msg_id) {
        case 0x13030002: 
        {
          msg_id_ = msg_id;
          target_size_ = msg_len;
          int remaining_data_bytes = len - bytes_processed;
          if (msg_len > remaining_data_bytes) {
            pkg_.resize(remaining_data_bytes);
            memcpy(&pkg_[0], the_data + bytes_processed, remaining_data_bytes);
            bytes_processed += remaining_data_bytes;
          } else {
            pkg_.resize(msg_len);
            memcpy(&pkg_[0], the_data + bytes_processed, msg_len);
            bytes_processed += msg_len;
          }
          break;
        }
        case 0x13030003:
        case 0x13030005:
        case 0x13030006:
        {
          NSData* img_id = [NSData dataWithBytes:the_data + bytes_processed length:sizeof(int)];
          bytes_processed += sizeof(int);
          [delegate_ incomingMessage:msg_id withData:img_id];
          break;
        }
        default:
          [self message:@"Unknown package incoming ..."];
          break;
      }
    }
    
    if (pkg_.size() == target_size_) {
      switch(msg_id_) {
       case 0x13030002: 
        {
          NSData* package = [[[NSData alloc] initWithBytes:&pkg_[0]
                                                    length:target_size_] autorelease];
          [delegate_ incomingMessage:msg_id_ withData:package];
          pkg_.clear();
          break;
        }
        default:
          break;
      }
    }
  }
}

-(void)send:(CFDataRef)data {
  // Ignore send command if no connection established.
  if (!socket_) {
    [self message:@"Error: Send command canceled because no connection to server."];
    return;
  }
  
  CFSocketSendData(socket_, NULL, data, 1);
}

-(void)dealloc {
  
  if (read_stream_)
    CFRelease(read_stream_);
  if (write_stream_)
    CFRelease(write_stream_);
  
  if (socket_) {
    CFSocketInvalidate(socket_);
    CFRelease(socket_);
  }
  
  [super dealloc];
}

@end
