//
//  pano_client.m
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

#import "pano_client_udp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <CFNetwork/CFSocketStream.h>

#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>

@implementation PanoClientUDP
@synthesize delegate=delegate_;

-(id)init {
  if (self = [super init]) {
    socket_ = 0;
    last_update_ = [[NSDate date] retain];
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
  PanoClientUDP* client = (PanoClientUDP*) info;
  if (type == kCFSocketDataCallBack) {
    CFDataRef the_data = (CFDataRef) data;
    [client incomingData:the_data];
  }
}

-(void)connectToPort:(int)port {
  
  if (socket_) {
    CFSocketInvalidate(socket_);
    CFRelease(socket_);
  }
  
  connected_ = false;
  
  CFSocketContext socketCtxt = {0, self, NULL, NULL, NULL};
  socket_ = CFSocketCreate(kCFAllocatorDefault,
                           PF_INET,
                           SOCK_DGRAM,
                           IPPROTO_UDP, 
                           kCFSocketDataCallBack,
                           (CFSocketCallBack)&SocketCallBack, 
                           &socketCtxt);
  
  if (socket_ == NULL) {
    [self message:@"Error: Can't create UDP socket!"];
    return;
  }
  
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  
  NSString* my_ip = [self localIp];
  if (my_ip == nil) {
    CFRelease(socket_);
    socket_ = 0;
    [self message:@"Error: Can't determine local Ip."];
    return;
  }
  
  sin.sin_addr.s_addr = inet_addr([my_ip UTF8String]);
  
  CFDataRef address = CFDataCreate(NULL, (unsigned char*)&sin, sizeof(sin));
  CFSocketError e = CFSocketSetAddress(socket_, address);
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
  
  // Send packages to establish connection.
  int msg_id = 0x13030000;
  CFDataRef msg = CFDataCreate(kCFAllocatorDefault, (uint8_t*)&msg_id, sizeof(int));
  for (int i = 0; i < 10; ++i) {
    [self send:msg];
    [NSThread sleepForTimeInterval:0.01];
  }
  
  if(msg)
    CFRelease(msg);
}

-(void)incomingData:(CFDataRef)data {  
  int len = CFDataGetLength(data);
  if(len <=0 )
    return;
  
  int bytes_processed = 0;
  
  while (bytes_processed < len) {
    // Snoop in header.
    const uint8_t* the_data = CFDataGetBytePtr(data);
  
    int msg_id = *(int*)the_data;
    the_data += sizeof(int);
    ++bytes_processed;
    
    int msg_len = 0;
        
    switch(msg_id) {
      case 0x13030000:
        if (!connected_) {
          [self message:@"ACK from server. Connected!"];
          connected_ = true;
        }
        
        break;
      case 0x13030001: 
      {
        msg_len = *(int*)the_data;
        the_data += sizeof(int);
        ++bytes_processed;
        
        NSData* package = [[[NSData alloc] initWithBytes:the_data length:msg_len] autorelease];
        [delegate_ incomingMessage:msg_id withData:package];
        
        break;
      }
      default:
        [self message:@"Unknown package incoming ..."];
        break;
    }
    
    the_data += msg_len;
    bytes_processed += msg_len;
  }
}

-(void)send:(CFDataRef)data {
  // Ignore send command if no connection established.
  if (!socket_) {
    [self message:@"Error: Send command canceled because no connection to server."];
    return;
  }
  
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(34567);
  sin.sin_addr.s_addr = inet_addr([@"10.0.1.2" UTF8String]);
  
  CFDataRef address = CFDataCreate(NULL, (unsigned char*)&sin, sizeof(sin));
  
  NSDate* cur_time = [NSDate date];
  if ([cur_time timeIntervalSinceDate:last_update_] > 1.0 / 50.0) {
    CFSocketSendData(socket_, address, data, 0);
    [last_update_ autorelease];
    last_update_ = [cur_time retain];
  }

  CFRelease(address);
}

-(NSString*)localIp {
  struct ifaddrs* list;
	if(getifaddrs(&list) < 0)
		return nil;
	
	NSMutableArray *array = [NSMutableArray array];
	struct ifaddrs *cur;	
	for(cur = list; cur != NULL; cur = cur->ifa_next)
	{
		if(cur->ifa_addr->sa_family != AF_INET)
			continue;
		
		struct sockaddr_in *addrStruct = (struct sockaddr_in *)cur->ifa_addr;
		NSString *name = [NSString stringWithUTF8String:cur->ifa_name];
		NSString *addr = [NSString stringWithUTF8String:inet_ntoa(addrStruct->sin_addr)];
		[array addObject:
     [NSDictionary dictionaryWithObjectsAndKeys:
      name, @"name",
      addr, @"address",
      [NSString stringWithFormat:@"%@ - %@", name, addr], @"formattedName",
      nil]];
	}
	
	freeifaddrs(list);
  
  if ([array count] < 2)
    return nil;
  
  // local WLAN ip at index one.
  return [[array objectAtIndex:1] valueForKey:@"address"];
}

-(void)dealloc {
  
  if (socket_) {
    CFSocketInvalidate(socket_);
    CFRelease(socket_);
  }
  
  [super dealloc];
}

@end
