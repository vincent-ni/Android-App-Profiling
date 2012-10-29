//
//  TiledImage.h
//  visualFX
//
//  Created by Matthias Grundmann on 9/4/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>

#include <vector>
using std::vector;
typedef unsigned char uchar;

@class FXImage;

@interface TiledImage : NSObject {
  FXImage*         image_;
  int              tile_size_;
  vector<GLuint>*  texture_ids_;
  int              tiles_x_;
  int              tiles_y_;
  
  uchar*           tex_data_;
  GLenum           mode_;
}


-(id) initWithFXImage:(FXImage*)img;

// Size must be a power of 2 between 64 and 1024.
// Mode should be GL_ALPHA or GL_LUMINANCE.
-(void) loadTo8bitTileSize:(int)size mode:(GLuint)mode;
-(void) reloadTileAtX:(int)x andY:(int)y;
-(void) drawImageOpenGL;

@property (nonatomic, readonly) FXImage* image;
@property (nonatomic, readonly) int      tile_size;

@end
