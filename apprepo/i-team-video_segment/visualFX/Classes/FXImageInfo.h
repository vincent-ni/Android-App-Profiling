//
//  FXImageInfo.h
//  visualFX
//
//  Created by Matthias Grundmann on 3/21/09.
//  Copyright 2009 Matthias Grundmann. All rights reserved.
//

@class FXImage;

// Class that holds additional information about the used image
@interface FXImageInfo : NSObject {
  FXImage*  image_;
  FXImage*  small_res_;
  float     scale_;
  float     rotation_;
  float     trans_x_;
  float     trans_y_;
}

-(id) initWithFXImage:(FXImage*)img;
-(id) initWithFXImage:(FXImage*)img rotation:(float)rot;
-(CGPoint) frameToImageCoords:(CGPoint)pt;
-(void) boundingRectMinPt:(CGPoint*)min_pt maxPt:(CGPoint*)max_pt;
-(bool) isPointInImage:(CGPoint)p;

@property (nonatomic, assign) float scale;
@property (nonatomic, assign) float rotation;
@property (nonatomic, assign) float trans_x;
@property (nonatomic, assign) float trans_y;
@property (nonatomic, readonly) float img_width;
@property (nonatomic, readonly) float img_height;
@property (nonatomic, readonly) FXImage* image;

@end
