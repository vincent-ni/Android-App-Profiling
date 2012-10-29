//
//  TiledImage.m
//  visualFX
//
//  Created by Matthias Grundmann on 9/4/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import "TiledImage.h"
#import "FXImage.h"

@interface TiledImage (PrivateMehtods)
-(void) initTexturesFromSize:(int)size;
@end

@implementation TiledImage

@synthesize image=image_, tile_size=tile_size_;

-(void) initTexturesFromSize:(int)size {
  if (texture_ids_->size());
    glDeleteTextures(texture_ids_->size(), &(*texture_ids_)[0]);

  tiles_x_ = ceil((float)image_.roi.width / (float)size);
  tiles_y_ = ceil((float)image_.roi.height / (float)size);
  tile_size_ = size;
  
  const int num_textures = tiles_x_ * tiles_y_;
  texture_ids_->resize(num_textures);
  glGenTextures(num_textures, &(*texture_ids_)[0]);
  
  for (vector<GLuint>::const_iterator i = texture_ids_->begin(); i != texture_ids_->end(); ++i) {
    glBindTexture(GL_TEXTURE_2D, *i);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
  }
}

-(id) initWithFXImage:(FXImage*)img {
  if (self = [super init]) {
    texture_ids_ = new vector<GLuint>;
    image_ = [img retain];
  }
  return self;
}

-(void) loadTo8bitTileSize:(int)size mode:(GLuint)mode {
  [self initTexturesFromSize:size];
  mode_ = mode;
    
  delete [] tex_data_;
  tex_data_ = new uchar[size*size];
  
  for (int i = 0; i < tiles_y_; ++i) {
    for (int j = 0; j < tiles_x_; ++j) {
      [self reloadTileAtX:j andY:i];
    }
  }  
}

-(void) reloadTileAtX:(int)j andY:(int)i {
  const int img_width = image_.roi.width;
  const int img_height = image_.roi.height;

  const int start_x = j * tile_size_;
  const int end_x = std::min((j + 1) * tile_size_, img_width);
  const int dx = end_x - start_x;
  
  const int start_y = i * tile_size_;
  const int end_y = std::min((i + 1) * tile_size_, img_height);
  
  uchar* dst_ptr = tex_data_;
  uchar* src_ptr = [image_ mutableValueAtX:start_x andY:start_y];
  
  for (int k = start_y; k < end_y; ++k, src_ptr += image_.width_step, dst_ptr += tile_size_) {
    memcpy(dst_ptr, src_ptr, dx);
  }
  
  glBindTexture(GL_TEXTURE_2D, (*texture_ids_)[i*tiles_x_+j]);
  glTexImage2D(GL_TEXTURE_2D,
               0,                 // mipmap level 0
               mode_,             // internal format
               tile_size_, 
               tile_size_,
               0,                
               mode_,             // external format
               GL_UNSIGNED_BYTE,  // 8 bit per channel
               tex_data_);     
}

-(void) drawImageOpenGL {
  const int img_width = image_.roi.width;
  const int img_height = image_.roi.height;
  
  float tile_size_inv = 1.0/(float)tile_size_;
  GLfloat vertices[8];
  const float fix_offset = 0.007;
  GLfloat coords[8] = {fix_offset, fix_offset, fix_offset, fix_offset,
                       fix_offset, fix_offset, fix_offset, fix_offset};

	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, coords);  
  
  for (int i = 0; i < tiles_y_; ++i) {
    for (int j = 0; j < tiles_x_; ++j) {

      const int start_x = j * tile_size_;
      const int end_x = std::min((j + 1) * tile_size_, img_width);
      const int dx = end_x - start_x;
        
 //     const int start_y = img_height - i * tile_size_;
 //     const int end_y = img_height - std::min((i + 1) * tile_size_, img_height);
      const int start_y = img_height - std::min((i + 1) * tile_size_, img_height);
      const int end_y = img_height - i * tile_size_;
      const int dy = end_y - start_y;
      
      vertices[0] = vertices[4] = start_x;
      vertices[1] = vertices[3] = start_y;
      vertices[2] = vertices[6] = end_x;
      vertices[5] = vertices[7] = end_y;
      
      float s = (float)dx * tile_size_inv-fix_offset;
      float t = (float)dy * tile_size_inv-fix_offset;
      
      coords[2] = coords[6] = s;
      coords[1] = coords[3] = t;
          
      glBindTexture(GL_TEXTURE_2D, (*texture_ids_)[i*tiles_x_+j]);
      glTexCoordPointer(2, GL_FLOAT, 0, coords);  
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }
}

-(void) dealloc {
  if (texture_ids_->size());
    glDeleteTextures(texture_ids_->size(), &(*texture_ids_)[0]);
    
  [image_ release];
  delete tex_data_;
  delete texture_ids_;
  [super dealloc];
}

@end
