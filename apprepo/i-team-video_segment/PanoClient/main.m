//
//  main.m
//  PanoClient
//
//  Created by Matthias Grundmann on 3/14/09.
//  Copyright Matthias Grundmann 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

int main(int argc, char *argv[]) {
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int retVal = UIApplicationMain(argc, argv, nil, @"PanoClientAppDelegate");
    [pool release];
    return retVal;
}
