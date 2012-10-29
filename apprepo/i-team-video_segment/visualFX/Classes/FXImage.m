//
//  FXImage.m
//  visualFX
//
//  Created by Matthias Grundmann on 7/10/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import "FXImage.h"
#import "FXMatrix.h"

#include <vector>

@interface FXImage (PrivateMethods)
- (void) setTextureInformationFromROI;
- (void) initializeTexture;
- (void) scaleImageDataToTexture:(uchar*)tex_data;
@end

@implementation FXImage

@synthesize width=width_, height=height_, channels=channels_, width_step=width_step_,
            img_data=img_data_, texture_id=texture_id_,
            tex_coord_x=tex_coord_x_, tex_coord_y=tex_coord_y_, tex_scale=tex_scale_;
@dynamic roi, roi_data, roi_pad;

- (uchar*) roi_data {
  return img_data_ + width_step_ * roi_.y + roi_.x*channels_;
}

-(uint) roi_pad {
  return width_step_ - roi_.width*channels_;
}

-(FXRect) roi {
  return roi_;
}

-(void) setRoi:(FXRect)roi {
  roi_ = roi;
  [self setTextureInformationFromROI];
}

-(id) initWithUIImage:(UIImage*)img borderWidth:(uint)border {
  self = [self initWithUIImage:img borderWidth:border alphaSupport:false];
  return self;
}

-(id) initWithUIImage:(UIImage*)img borderWidth:(uint)border alphaSupport:(bool)alpha {
  
  self = [self initWithCGImage: img.CGImage borderWidth:border alphaSupport:(bool)alpha];
  return self;
}

-(id) initWithCGImage:(CGImageRef)img borderWidth:(uint)border {
  self = [self initWithCGImage:img borderWidth:border alphaSupport:false];
  return self;
}

-(id) initWithCGImage:(CGImageRef)img borderWidth:(uint)border alphaSupport:(bool)alpha {
  if ( self = [self initWithImgWidth:CGImageGetWidth(img)
                           imgHeight:CGImageGetHeight(img)
                         imgChannels:4
                         borderWidth:border]) {
     // Allocate bitmap context.
     CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
     int alpha_info = alpha ? kCGImageAlphaPremultipliedLast 
                                    : kCGImageAlphaNoneSkipLast;
     CGContextRef bitmap = CGBitmapContextCreate(img_data_, width_, height_, 
                                                 8,        // bits per channel
                                                 width_step_, 
                                                 colorSpace, 
                                                 alpha_info |                                            
                                                 kCGBitmapByteOrder32Big);
     if (bitmap) {
       CGContextDrawImage (bitmap, 
                           CGRectMake (roi_.x, roi_.y, roi_.width, roi_.height),
                           img);
      } else {
        NSLog (@"FXImage initWithCGImage: Can't create bitmap context");
        self = nil;
        
        CGContextRelease (bitmap);
        CGColorSpaceRelease (colorSpace);
        return self;
      }
     
     CGContextRelease (bitmap);
     CGColorSpaceRelease (colorSpace);
  }
  return self;
}

-(id) initWithImgWidth:(uint)width 
             imgHeight:(uint)height
           imgChannels:(uint)ch 
           borderWidth:(uint)border {
  if (self = [super init]) {
    width_ = width+2*border;
    height_ = height+2*border;
    channels_ = ch;
    width_step_ = channels_ * width_;
    
    self.roi = FXRect (border, border, width_-2*border, height_-2*border);
    
    img_data_ = (uchar*) malloc (width_*height_*channels_);
    if (!img_data_) {
      NSLog (@"FXImage: malloc failed");
      self = nil;
      return self;
    }
  }
  return self;
}

-(id) initWithCGImage:(CGImageRef)img 
          borderWidth:(uint)border
              maxSize:(uint)sz
          orientation:(UIImageOrientation)dir {
  int width, height;
  if (dir == UIImageOrientationUp || dir == UIImageOrientationDown) {
    width = CGImageGetWidth(img);
    height = CGImageGetHeight(img);
  } else {
    width = CGImageGetHeight(img);
    height = CGImageGetWidth(img);
  }
  
  float max_dim = (float) std::max(width, height);
  float scale = std::min((float) sz / max_dim, 1.0f);
  width = scale * width;
  height = scale * height;
  
  if ( self = [self initWithImgWidth:width
                           imgHeight:height
                         imgChannels:4
                         borderWidth:border]) {
    // Allocate bitmap context.
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    
    CGContextRef bitmap = CGBitmapContextCreate(img_data_, width_, height_, 
                                                8,        // bits per channel
                                                width_step_, 
                                                colorSpace, 
                                                kCGImageAlphaNoneSkipLast |                                            
                                                kCGBitmapByteOrder32Big);
    if (bitmap) {
      switch (dir) {
        case UIImageOrientationUp:
          CGContextTranslateCTM(bitmap, border, border);
          CGContextScaleCTM(bitmap, scale, scale);
          break;
        case UIImageOrientationDown:
          CGContextTranslateCTM(bitmap, width + border, height + border);
          CGContextScaleCTM(bitmap, scale, scale);
          CGContextRotateCTM(bitmap, M_PI);
          break;
        case UIImageOrientationLeft:
          CGContextTranslateCTM(bitmap, width + border, border);
          CGContextScaleCTM(bitmap, scale, scale);
          CGContextRotateCTM(bitmap, M_PI * 0.5);
          break;
        case UIImageOrientationRight:
          CGContextTranslateCTM(bitmap, border, height + border);
          CGContextScaleCTM(bitmap, scale, scale);
          CGContextRotateCTM(bitmap, -M_PI * 0.5);
          break;
        default:
          NSLog(@"FXImage:initWithCGImage: unknown image orientation");
      } 
      
      CGContextDrawImage (bitmap, 
                          CGRectMake (0, 0, CGImageGetWidth(img), CGImageGetHeight(img)),
                          img);
    } else {
      NSLog (@"FXImage initWithCGImage: Can't create bitmap context");
      self = nil;
      
      CGContextRelease (bitmap);
      CGColorSpaceRelease (colorSpace);
      return self;
    }
    
    CGContextRelease (bitmap);
    CGColorSpaceRelease (colorSpace);
  }
  return self;
}

-(id) initWithImgWidth:(uint)width imgHeight:(uint)height imgChannels:(uint)channels {
  self = [self initWithImgWidth:width
                      imgHeight:height
                    imgChannels:channels
                    borderWidth:0];
  return self;
}

-(CGContextRef) createBitmapContext {
  if (channels_ != 4) {
    NSLog(@"FXImage:bitmapContext: only four channel data supported");
    return nil;
  }
  
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef bitmap = CGBitmapContextCreate(self.roi_data, 
                                              roi_.width,
                                              roi_.height, 
                                              8,          // bits per channel
                                              width_step_, // width_step
                                              colorSpace, 
                                              kCGImageAlphaNoneSkipLast |
                                              kCGBitmapByteOrder32Big);
  CGColorSpaceRelease (colorSpace);
  if (!bitmap) {
    NSLog (@"FXImage:bitmapContext: Can't create bitmap context");
    return nil;
  }
  
  return bitmap;
}

-(CGImageRef) copyToNewCGImage {
  
  // standard data provider form our img_data_
  // only get a view, don't copy - doesn't work: boo !
 // CFDataRef cf_data = CFDataCreateWithBytesNoCopy (
 //   NULL, img_data_, width_step_*height_, kCFAllocatorNull);
  CFDataRef cf_data = CFDataCreate (kCFAllocatorDefault, img_data_, width_step_*height_);
  CGDataProviderRef cg_provider = CGDataProviderCreateWithCFData(cf_data);
  
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGImageRef cg_image = CGImageCreate (width_, 
                                      height_, 
                                      8,                  // bits per color
                                      8 * channels_,      // bits per pixel
                                      width_step_,
                                      colorSpace,
                                      kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big,
                                      cg_provider,
                                      NULL,   // no decode array
                                      true,   // interpolate colors
                                      kCGRenderingIntentDefault); // default gamut
  
  
  CGColorSpaceRelease (colorSpace);
  CGDataProviderRelease (cg_provider);
  CFRelease (cf_data);
  return cg_image;
}

-(void) copyToImage:(FXImage*)dst {
  if ( !roi_.isEqualSize(dst->roi_) || channels_ != dst->channels_) {
    NSLog (@"FXImage:copyToImage: Images differ in size or in number of channels.");
    return;
  }
  
  uchar* src_ptr = self.roi_data;
  uchar* dst_ptr = dst.roi_data;
  
  if (roi_.width*channels_ == width_step_ &&
      dst->roi_.width*dst.channels == dst->width_step_) {
    memcpy(dst_ptr, src_ptr, width_step_*roi_.height);
  } else {
    for (int i = 0; i < roi_.height; ++i, src_ptr+=width_step_, dst_ptr+=dst->width_step_) {
      memcpy(dst_ptr, src_ptr, roi_.width*channels_); 
    }
  }
}

-(void) copyToImage:(FXImage*)dst usingMask:(FXImage*)mask {
  if (!roi_.isEqualSize(dst.roi) || 
      !roi_.isEqualSize(mask.roi) ||
      channels_ != dst.channels ||
      mask.channels != 1) {
    NSLog(@"FXImage:copyToImage:usingMask: Images and mask differ in size, input images have "
          @"different number of channels or mask not grayscale.");
    return;
  }
  
  uchar* src_ptr = self.roi_data;
  uchar* dst_ptr = dst.roi_data;
  uchar* mask_ptr = mask.roi_data;
  
  const int src_pad = self.roi_pad;
  const int dst_pad = dst.roi_pad;
  const int mask_pad = mask.roi_pad;
  
  const int width = roi_.width;
  const int height = roi_.height;
  
  if (channels_ == 1) {
    for (int i = 0;
         i < height;
         ++i, src_ptr += src_pad, dst_ptr += dst_pad, mask_ptr += mask_pad) {
      for (int j = 0; j < width; ++j, ++src_ptr, ++dst_ptr, ++mask_ptr) {
        if (*mask_ptr) {
          *dst_ptr = *src_ptr;
        }
      }
    }
  } else if (channels_ == 4) {
    for (int i = 0;
         i < height;
         ++i, src_ptr += src_pad, dst_ptr += dst_pad, mask_ptr += mask_pad) {
      for (int j = 0; j < width; ++j, src_ptr+=4, dst_ptr+=4, ++mask_ptr) {
        if (*mask_ptr) {
          *reinterpret_cast<int*>(dst_ptr) = *reinterpret_cast<int*>(src_ptr);
        }
      }
    }
  }
}

-(void) setToConstant:(uchar)value {
  uchar* src_dest = self.roi_data;
  const int src_padding = self.roi_pad;
  const int img_width = roi_.width;
  const int img_height = roi_.height;
  
  if (channels_ == 1) {
    for (int i = 0; i < img_height; ++i, src_dest += src_padding) {
      for (int j = 0; j < img_width; ++j) {
        *src_dest++ = value;
      }
    }
  } else if(channels_ == 4) {
    for (int i = 0; i < img_height; ++i, src_dest += src_padding) {
      for (int j = 0; j < img_width; ++j) {
        *src_dest++ = value;
        *src_dest++ = value;
        *src_dest++ = value;
        ++src_dest;             // Skip alpha.
      }
    }
  }
}

-(void) setToConstant:(uchar)value usingMask:(FXImage*)mask {
  
  if (!roi_.isEqualSize(mask.roi)) {
    NSLog(@"FXImage:setToConstant:usingMask: image and mask must have the same size.");
    return;
  }
  
  uchar* src_dest = self.roi_data;
  const int src_padding = self.roi_pad;
  const int img_width = roi_.width;
  const int img_height = roi_.height;
  
  uchar* mask_ptr = mask.roi_data;
  const int mask_pad = mask.roi_pad;
  
  if (channels_ == 1) {
    for (int i = 0; i < img_height; ++i, src_dest += src_padding, mask_ptr += mask_pad) {
      for (int j = 0; j < img_width; ++j, ++src_dest, ++mask_ptr) {
        if(*mask_ptr)
          *src_dest = value;
      }
    }
  } else if(channels_ ==4) {
    for (int i = 0; i < img_height; ++i, src_dest += src_padding, mask_ptr += mask_pad) {
      for (int j = 0; j < img_width; ++j, ++mask_ptr) {
        if(*mask_ptr) {
          *src_dest++ = value;
          *src_dest++ = value;
          *src_dest++ = value;
          ++src_dest;             // Skip alpha.
        } else {
          src_dest += 4;
        }
      }
    }
  }
}


- (void) copyToImage:(FXImage*) dst replicateBorder:(int)border {
  if ( !roi_.isEqualSize(dst->roi_) || channels_ != dst->channels_) {
    NSLog (@"FXImage:copyToImage:replicateBorder: "
           "Images differ in size or in number of channels.");
    return;
  }
  
  if (![dst hasBorderOfAtLeast:border]) {
    NSLog(@"FXImage:copyToImage:replicateBorder: "
          @"Specified border around ROI exceeds destination image dimensions");
    return;
  }
  
  if (channels_ != 1 && channels_ != 4) {
    NSLog(@"FXImage:copyToImage:replicateBorder: Only one and four channel images supported");
    return;
  }
  
  uchar* src_ptr = self.roi_data;
  uchar* dst_ptr = dst.roi_data - dst->width_step_*border - border*channels_;
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = dst->width_step_ - (width+border2)*channels_;
  
  if ( channels_ == 1) {
    for (int j = 0; j < height+border2; ++j, dst_ptr += dst_pad) {
      int i = 0;
      while (i < border) {
        *dst_ptr++ = src_ptr[0];
        ++i;
      }
      
      // always copy
      memcpy (dst_ptr,src_ptr,width*channels_);
      
      i += width;
      dst_ptr += width*channels_;
      
      while (i < width+border2) {
        *dst_ptr++ = src_ptr[width-1];
        ++i;
      }
      
      if (j >= border && j < height + border - 1)
        src_ptr += width_step_;
    }
  } else {
    // channels == 4
    for (int j = 0; j < height+border2; ++j, dst_ptr += dst_pad) {
      int i = 0;
      while (i < border) {
        *(int*)dst_ptr = ((int*)src_ptr)[0];
        dst_ptr += channels_;
        ++i;
      }
      
      // always copy
      memcpy (dst_ptr,src_ptr,width*channels_);
      
      i += width;
      dst_ptr += width*channels_;
      
      while (i < width+border2) {
        *(int*)dst_ptr = ((int*)src_ptr)[width-1];
        dst_ptr += channels_;
        ++i;
      }
      
      if (j >= border && j < height + border - 1)
        src_ptr += width_step_;
    }    
  }
}

- (void) copyReplicateBorderPixels:(int)border {
  [self copyReplicateBorderPixels:border mode:BORDER_BOTH];
}

- (void) copyReplicateBorderPixels:(int)border mode:(BorderMode)mode {
  if (![self hasBorderOfAtLeast:border]) {
    NSLog(@"FXImage:copyReplicateBorderPixels: " 
          @"Specified border around ROI exceeds image dimensions");
    return;
  }
  
  if (channels_ != 1 && channels_ != 4) {
    NSLog(@"FXImage:copyReplicateBorderPixels: "
          @"Only one and four channel images supported");
    return;
  }
  
  uchar* src_ptr = self.roi_data;
  uchar* dst_ptr;
  
  if (mode == BORDER_HORIZ) {
    dst_ptr = src_ptr - border*channels_;
  } else {
    dst_ptr = src_ptr - width_step_*border - border*channels_;
  }
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = width_step_ - (width+border2)*channels_;
  
  const int start_row = mode == BORDER_HORIZ ? border : 0;
  const int end_row = mode == BORDER_HORIZ ? height+border : height+border2;
  
  if (channels_ == 1) {
    for (int j = start_row; j < end_row; ++j, dst_ptr += dst_pad) {
      int i = 0;
      if (mode == BORDER_VERT) {
        i = border;
        dst_ptr += border;
      } else {        
        while (i < border) {
          *dst_ptr++ = src_ptr[0];
          ++i;
        }
      }
      
      // only copy if at border
      if ( j < border || j >= height+border)
        memcpy (dst_ptr,src_ptr,width*channels_);
      
      i += width;
      dst_ptr += width*channels_;
      
      if (mode == BORDER_VERT) {
        dst_ptr += border;
      } else {
        while (i < width+border2) {
          *dst_ptr++ = src_ptr[width-1];
          ++i;
        }
      }
      
      if (j >= border && j < height + border - 1)
        src_ptr += width_step_;
    }
  } else {
    for (int j = start_row; j < end_row; ++j, dst_ptr += dst_pad) {
      int i = 0;
      if (mode == BORDER_VERT) {
        i = border;
        dst_ptr += border*channels_;
      } else {
        while (i < border) {
          *(int*)dst_ptr = ((int*)src_ptr)[0];
          dst_ptr += channels_;
          ++i;
        }
      }
      
      // only copy if at border
      if ( j < border || j >= height+border)
        memcpy (dst_ptr,src_ptr,width*channels_);
      
      i += width;
      dst_ptr += width*channels_;
      
      if (mode == BORDER_VERT) {
        dst_ptr += border*channels_;
      } else {
        while (i < width+border2) {
          *(int*)dst_ptr = ((int*)src_ptr)[width-1];
          dst_ptr += channels_;
          ++i;
        }
      }
      
      if (j >= border && j < height + border - 1)
        src_ptr += width_step_;
    }        
  }
}

-(void) setBorder:(int)border toConstant:(uchar)value {
  [self setBorder:border toConstant:value mode:BORDER_BOTH];
}


-(void) setBorder:(int)border toConstant:(uchar)value mode:(BorderMode)mode {
  if (![self hasBorderOfAtLeast:border]) {
    NSLog(@"FXImage:setBorder:toConstant: " 
          @"Specified border around ROI exceeds image dimensions");
    return;
  }
  
  if (channels_ != 1 && channels_ != 4) {
    NSLog(@"FXImage:setBorder:toConstant: "
          @"Only one and four channel images supported");
    return;
  }
  
  uchar* src_ptr = self.roi_data;
  uchar* dst_ptr;
  
  if (mode == BORDER_HORIZ) {
    dst_ptr = src_ptr - border*channels_;
  } else {
    dst_ptr = src_ptr - width_step_*border - border*channels_;
  }
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = width_step_ - (width+border2)*channels_;
  
  const int start_row = mode == BORDER_HORIZ ? border : 0;
  const int end_row = mode == BORDER_HORIZ ? height+border : height+border2;
  
  for (int j = start_row; j < end_row; ++j, dst_ptr += dst_pad) {
    if (mode != BORDER_VERT)
      memset(dst_ptr, value, border*channels_);
    
    dst_ptr += border*channels_;
    
    // only set if at border
    if (j < border || j >= height+border)
      memset(dst_ptr, value, width*channels_);
    
    dst_ptr += width*channels_;
    
    if (mode != BORDER_VERT) 
      memset(dst_ptr, value, border*channels_);
    
    dst_ptr += border*channels_;
  }
}

- (void) rgbaToGrayImage:(FXImage*) img {
  if ( !roi_.isEqualSize(img->roi_) || channels_ != 4 || img->channels_ != 1) {
    NSLog (@"FXImage:rgbaToGrayImage "
           @"Images differ in size or have wrong number of channels");
    return;
  }

  // initialize LUT
  const int shift=20;
  const int redWeight   = (int)(0.299f * (1<<shift) + 0.5);
  const int greenWeight = (int)(0.587f * (1<<shift) + 0.5);
  const int blueWeight  = (1<<shift) - redWeight - greenWeight;   // normalize to 1
  
  int red_table[256];
  int green_table[256];
  int blue_table[256];
  
  int r = 0;
  int g = 0;
  int b = 1<<(shift-1);   // avoids adding 0.5=1<<(shift-1) to the result
  
  for (int i=0; i<256; ++i) {
    red_table[i] = r;
    green_table[i] = g;
    blue_table[i] = b;
    r += redWeight;
    g += greenWeight;
    b += blueWeight;
  }
  
  const int src_step_diff = width_step_ - roi_.width*channels_;
  const int src_inc = channels_;
      
  const uchar* sptr = self.roi_data;
  uchar* dptr = img.roi_data;
  
  for (int i=0; i < roi_.height; ++i, sptr+=src_step_diff, dptr+=img->width_step_) {
    for (int j=0; j < roi_.width; ++j, sptr+=src_inc) {
      dptr[j] = (red_table[sptr[0]] + green_table[sptr[1]] + blue_table[sptr[2]]) >> shift;
    }
  }
}



- (void) grayToRgbaImage:(FXImage*) img {
  if ( !roi_.isEqualSize(img->roi_) || channels_ != 1 || img->channels_ != 4) {
    NSLog (@"FXImage grayToRgbaImage: "
           @"Images differ in size or have wrong number of channels");
    return;
  }
  
  const uchar* sptr = self.roi_data;
  uchar* dptr = img.roi_data;
  
  const int dest_step_diff = img->width_step_ - img->roi_.width*img->channels_;
  const int dest_inc = img->channels_;
  
  for (int i=0; i < roi_.height; ++i, sptr+=width_step_, dptr+=dest_step_diff) {
    for (int j=0; j < roi_.width; ++j, dptr+=dest_inc) {
      dptr[0] = dptr[1] = dptr[2] = sptr[j];
      dptr[3] = 255;
    }
  }  
}


- (void) computeGradientX:(FXMatrix*)graX gradientY:(FXMatrix*)graY {
  if (![self hasBorderOfAtLeast:1] || channels_ != 1) {
    NSLog(@"FXImage:computeGradientX expecting grayscale image with border of 1");
    return;
  }
  
  if (!roi_.isEqualSize(graX.roi) || !roi_.isEqualSize(graY.roi)) {
    NSLog(@"FXImage:computeGradientX input FXMatrices differ in size from image");
    return;
  }
  
  // Compute 3x3 space offsets
  //  0 1 2
  //  3 4 5
  //  6 7 8
  int space_ofs[9];
  for (int i = -1, idx=0; i <= 1; ++i)
    for (int j = -1; j <= 1; ++j, ++idx)
      space_ofs[idx] = i*width_step_ + j;
  
  // compute for x     for y
  // -1  0  1         -1  -2  -1
  // -2  0  2          0   0   0
  // -1  0  1          1   2   1
  
  const uchar* sptr = self.roi_data;
  float* xptr = graX.roi_data;
  float* yptr = graY.roi_data;
  
  const uint roi_width = roi_.width;
  const uint roi_height = roi_.height;
  
  const int src_padding = width_step_ - roi_width;
  const int x_padding = graX.width_step - roi_width * sizeof(float);
  const int y_padding = graY.width_step - roi_width * sizeof(float);
  
  int pix_val;
  
  for (int i = 0; 
       i < roi_height; 
       ++i, sptr += src_padding, xptr = PtrOffset(xptr, x_padding), 
       yptr = PtrOffset(yptr, y_padding)) {
    for (int j = 0; j < roi_width; ++j, ++sptr, ++xptr, ++yptr) {
      // Process the 4 corners
      pix_val = *(sptr+space_ofs[0]);
      int xval = -pix_val;
      int yval = -pix_val;
      
      pix_val = *(sptr+space_ofs[2]);
      xval += pix_val;
      yval -= pix_val;
      
      pix_val = *(sptr+space_ofs[6]); 
      xval -= pix_val;
      yval += pix_val;
      
      pix_val = *(sptr+space_ofs[8]); 
      xval += pix_val;
      yval += pix_val;     
      
      xval += 2 * ( *(sptr+space_ofs[5]) - *(sptr+space_ofs[3]) );
      yval += 2 * ( *(sptr+space_ofs[7]) - *(sptr+space_ofs[1]) );

      *xptr = (float)xval * (1.0/8.0);
      *yptr = (float)yval * (1.0/8.0);
    }
  }
}

-(void) computeGaussianKernel:(float*)gaussian_kernel kernelSize:(int)size kernelSigma:(float)sigma  {
		float sum_mask = 0.0f;
		const float pi = 3.1416;
		
		// Generate the gaussian kernel of specified size with the well known gaussian formula.
		const int halfsize = (size - 1) / 2;
	for(int i = -halfsize; i <= halfsize; i++) {
		for(int j = -halfsize; j <= halfsize; j++) {
			gaussian_kernel[(i+halfsize) * size + (j+halfsize)] = exp(-(i*i + j*j)/(2*sigma*sigma)) / ( sigma * sqrt(2*pi));
			sum_mask += gaussian_kernel[(i+halfsize) * size + (j+halfsize)];
		}
	}
			
		// Make sure that the kernel sums upto 1. Normalize them.																				
	for(int i = -halfsize; i <= halfsize; i++) {
			for(int j = -halfsize; j <= halfsize; j++) {
				gaussian_kernel[(i + halfsize) * size + (j + halfsize)] /= sum_mask;
			//	NSLog(@"The value of the Gaussian kernel is %f\n", gaussian_kernel[(i + halfsize) * size + (j + halfsize)]);
			}
	}
				
}

-(void)computeGaussianSmoothing:(FXMatrix*)imagemat kernelSize:(int)size kernelSigma:(float)sigma {
	
	if (![self hasBorderOfAtLeast:1] || channels_ != 1) {
    NSLog(@"FXImage:computeGaussianSmoothing expecting grayscale image with border of 1");
    return;
  }
  
  if (!roi_.isEqualSize(imagemat.roi)) {
    NSLog(@"FXImage:computeGaussianSmoothing input FXMatrices differ in size from image");
    return;
  }
	 
		const uchar* sptr = self.roi_data;
		float* dptr = imagemat.roi_data;
		
		const uint roi_width = roi_.width;
		const uint roi_height = roi_.height;
		float kernel[size * size];
		int halfsize = (size - 1) / 2;
		
		// Fill up the kernel with gaussian kernel value.
		[self computeGaussianKernel: kernel kernelSize: size kernelSigma: sigma];
		
		// Do the Gaussian Convolution with the obtained Gaussian Kernel.
		for (int row = halfsize; row < roi_height - halfsize; row++) {
		for (int col = halfsize; col < roi_width - halfsize; col++) {
			float newPixel = 0.0f;
			for (int rowOffset = -halfsize; rowOffset <= halfsize; rowOffset++) {
				for (int colOffset= -halfsize; colOffset <= halfsize; colOffset++) {
					int rowTotal = row + rowOffset;
					int colTotal = col + colOffset;
					int iOffset = (rowTotal*roi_width + colTotal);
					newPixel += ((float)*(sptr + iOffset)) * kernel[(halfsize + rowOffset)*size + (2 + colOffset)];
				}
			}
			
			int i = (unsigned long)(row*roi_width + col);
			*(dptr + i) = newPixel;
		}
	}
}


-(void) gaussianFilterWithSigma:(float)sigma radius:(int)rad outputImage:(FXImage*)output {
  if (!roi_.isEqualSize(output.roi) || channels_ != output.channels) {
    NSLog(@"FXImage:gaussianFilterWithSigma: output FXImage has different size or"
          @" differtent number of channels");
    return;
  }
  
  const int radius = rad;
  const int diam = 2*radius+1;
  
  if (![self hasBorderOfAtLeast:radius]) {
    NSLog(@"FXImage:gaussianFilterWithSigma: expecting src-image with border of %d",
          radius);
    return;
  }
  
  [self copyReplicateBorderPixels:radius];
  
  // Compute space offsets and weights;
  std::vector<int> space_ofs (diam*diam);
  std::vector<float> space_weights(diam*diam);
  
  float weight_sum = 0;
  const float gauss_coeff = -0.5/(sigma*sigma);
  const int radius2 = radius*radius;
  int cur_pos;
  uint space_sz = 0;
  
  for (int i = -radius; i <= radius; ++i) 
    for (int j = -radius; j <= radius; ++j) {
      cur_pos = i*i + j*j;
      if (cur_pos > radius2)
        continue;
      space_weights[space_sz] = exp(static_cast<float>(cur_pos) * gauss_coeff);
      weight_sum += space_weights[space_sz];
      space_ofs[space_sz] = i*width_step_ + j*channels_;
      ++space_sz;
    }
  
  // Normalize
  weight_sum = 1.0/weight_sum;
  for (int i = 0; i < space_sz; ++i) {
    space_weights[i] *= weight_sum;
  }
  
  // Convert float weights to integer weights
  const int shift = 20;
  int int_sum = 0;
  std::vector<int> space_weights_int(space_sz);
  // Process last one separately to ensure weights sum to 1;
  for (int i = 0; i < space_sz-1; ++i) { 
    int_sum += (space_weights_int[i] = (int)(space_weights[i] * (1<<shift) + 0.5));
  }
  space_weights_int[space_sz-1] = (1<<shift) - int_sum;
  
  // Apply filter
  int src_padding = width_step_ - roi_.width*channels_;
  int dst_padding = output.width_step - output.roi.width*channels_;
  
  const uchar* src_ptr = self.roi_data;
  uchar* dst_ptr = output.roi_data;
  
  if (channels_ == 1) {
    for (int i = 0; 
         i < roi_.height;
         ++i, src_ptr += src_padding, dst_ptr = PtrOffset(dst_ptr, dst_padding)) {
      for (int j = 0; j < roi_.width; ++j, ++src_ptr, ++dst_ptr) {
        int val = 0;
        for (int k = 0; k < space_sz; ++k) {
          val += space_weights_int[k] * static_cast<int>(*(src_ptr + space_ofs[k]));
        }
        *dst_ptr = (val + (1<<(shift-1))) >> shift;
      }
    }
  } else {
    const uint cn = channels_;
    for (int i = 0; 
         i < roi_.height;
         ++i, src_ptr += src_padding, dst_ptr += dst_padding) {
      for (int j = 0; j < roi_.width; ++j, src_ptr+=cn, dst_ptr+=cn) {
        int val_r = 0, val_g = 0, val_b = 0;
        for (int k = 0; k < space_sz; ++k) {
          val_r += space_weights_int[k] * static_cast<int>(*(src_ptr + space_ofs[k]));
          val_g += space_weights_int[k] * static_cast<int>(*(src_ptr + 1 + space_ofs[k]));
          val_b += space_weights_int[k] * static_cast<int>(*(src_ptr + 2 + space_ofs[k]));
        }
        *dst_ptr = (val_r + (1 << (shift-1))) >> shift;
        *(dst_ptr+1) = (val_g + (1 << (shift-1))) >> shift;
        *(dst_ptr+2) = (val_b + (1 << (shift-1))) >> shift;
        *(dst_ptr+3) = *(src_ptr+3);
      }
    }
  }
}

-(void) binaryThreshold:(uchar)value {
  if (channels_ != 1) {
    NSLog(@"FXImage:thresholdMax: only grayscale images are supported");
    return;
  }
  
  uchar* ptr = self.roi_data;
  const int pad = self.roi_pad;
  const int img_width = roi_.width;
  const int img_height = roi_.height;
  
  for (int j = 0; j < img_height; ++j, ptr += pad) {
    for (int i = 0; i < img_width; ++i, ++ptr) {
      if (*ptr >= value)
        *ptr = 1;
      else
        *ptr = 0;
    }
  }
}

-(uchar) valueAtX:(uint)x andY:(uint)y {
  return *(img_data_ + width_step_ * (roi_.y + y) + (roi_.x + x)*channels_);
}


-(uchar*) mutableValueAtX:(uint)x andY:(uint)y {
    return img_data_ + width_step_ * (roi_.y + y) + (roi_.x + x)*channels_;
}


- (GLuint) loadTo32bitTexture {
  
  if (channels_ != 4) {
    NSLog(@"loadTo32bitTexture: only 4 channels supported!");
    return 0;
  }
  
  [self initializeTexture];
  uchar* tex_data = (uchar*) malloc(tex_width_*tex_height_*4);
  
  if (tex_scale_ == 1) {
    // Copy row wise
    uchar* src_ptr = self.roi_data;
    uchar* dst_ptr = tex_data;
    for (int i = 0; i < roi_.height; ++i, dst_ptr+=tex_width_*4, src_ptr+=width_step_) {
      memcpy(dst_ptr, src_ptr, roi_.width*4);
    }
  } else {
    [self scaleImageDataToTexture:tex_data];
  }
  
  glTexImage2D(GL_TEXTURE_2D,
               0,                 // mipmap level 0
               GL_RGBA,           // internal format
               tex_width_, 
               tex_height_,
               0,                
               GL_RGBA,           // external format
               GL_UNSIGNED_BYTE,  // 8 bit per channel
               tex_data);      
  free(tex_data);
  return texture_id_;
}


- (GLuint) loadTo16bitTexture {
  if (channels_ != 4) {
    NSLog(@"loadTo16bitTexture: only 4 channels supported!");
    return 0;
  }
  
  [self initializeTexture];
  
  uchar* src_ptr;
  uchar* tex_data = 0;
  
  const int max_w = ceil(tex_scale_ * roi_.width);
  const int max_h = ceil(tex_scale_ * roi_.height);
  int src_padding;
  int dst_padding = ((tex_width_ - max_w) * 2);
  
  if (tex_scale_ != 1) {
    tex_data = (uchar*) malloc(tex_width_*tex_height_*4);
    [self scaleImageDataToTexture:tex_data];
    src_ptr = tex_data;
    src_padding = ((tex_width_ - max_w) * 4);
  } else {
    src_ptr = self.roi_data;
    src_padding = width_step_ - max_w*4;
  }
  
  // Make internal copy and convert to 16 bit.
  uchar* data16 = (uchar*) malloc(tex_width_*tex_height_*2);
  uchar* dst_ptr = data16;
  if (!data16) {
    NSLog(@"loadTo16bitTexture: can't alloc tmp buffer");
    return 0;
  }
  
  // 32bit to 16 bit.
  // R and B are 5 bits.
  // G is 6 bits.
  
  for (int i=0; i < max_h; ++i, src_ptr += src_padding, dst_ptr+= dst_padding) {
    for (int j=0; j < max_w; ++j, dst_ptr+=2, src_ptr+=4) {
      *(unsigned short*)dst_ptr = (unsigned short)(
      (((uint)src_ptr[0]) >> 3) << 11 | 
      (((uint)src_ptr[1]) >> 2) << 5 | 
      (((uint)src_ptr[2]) >> 3));
    }
  }
  
  free(tex_data);
  
  // Load to texture.
  glTexImage2D(GL_TEXTURE_2D,
               0,                 // mipmap level 0
               GL_RGB,            // internal format
               tex_width_, 
               tex_height_,
               0,                 // border
               GL_RGB,            // external format
               GL_UNSIGNED_SHORT_5_6_5,
               data16);      
  free(data16);
  return texture_id_;
}

- (GLuint) loadTo8bitTextureMode:(GLenum)mode {
  if (channels_ != 1) {
    NSLog(@"loadTo8bitTexture: only 1 channel supported!");
    return 0;
  }
  
  [self initializeTexture];
  uchar* tex_data = (uchar*) malloc(tex_width_*tex_height_);
  
  if (tex_scale_ == 1) {
    // Copy row wise
    uchar* src_ptr = self.roi_data;
    uchar* dst_ptr = tex_data;
    for (int i = 0; i < roi_.height; ++i, dst_ptr+=tex_width_, src_ptr+=width_step_) {
      memcpy(dst_ptr, src_ptr, roi_.width);
    }
  } else {
    NSLog(@"FXImage:loadTo8bitTexture does not support tex_scale_ != 1");
    return 0;
  }
  
  glTexImage2D(GL_TEXTURE_2D,
               0,                 // mipmap level 0
               mode,              // internal format
               tex_width_, 
               tex_height_,
               0,                
               mode,              // external format
               GL_UNSIGNED_BYTE,  // 8 bit per channel
               tex_data);      
  
  free(tex_data);
  return texture_id_;
}


- (void) unloadTexture {
  if (texture_id_)
    glDeleteTextures(1, &texture_id_);
  texture_id_ = 0;
}


- (BOOL) isTextureLoaded {
  return texture_id_ != 0;
}


- (void) setTextureInformationFromROI {
  // Determine texture width.
  if (roi_.width == 1 || 
      roi_.width & (roi_.width - 1) == 0) {  // power of two test
    tex_width_ = roi_.width;
  } else {
    int search_width = 1;
    while ((search_width*=2) < roi_.width);
    tex_width_ = search_width;
  }
  
  if (roi_.height == 1 ||
      roi_.height & (roi_.height - 1) == 0) {
    tex_height_ = roi_.height;
  } else {
    int search_height = 1;
    while ((search_height*=2) < roi_.height);
    tex_height_ = search_height;
  }
  
  tex_scale_ = 1.0;
  while (tex_scale_ * tex_width_ > kMaxTextureSize ||
         tex_scale_ * tex_height_ > kMaxTextureSize )
    tex_scale_ *= 0.5;
  
  tex_width_ = (float)tex_width_ * tex_scale_;
  tex_height_ = (float)tex_height_ * tex_scale_;
  
  tex_coord_x_ = (float)roi_.width*tex_scale_/(float)tex_width_;
  tex_coord_y_ = (float)roi_.height*tex_scale_/(float)tex_height_;  
}


- (void) initializeTexture {
  // First check whether texture is already allocated --> free
  if (texture_id_)
    glDeleteTextures(1, &texture_id_);
  
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  // In case picture needs to be minimized or maximized use bilinear interpolation
  // NO mipmap support to save memory.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
}


- (void) scaleImageDataToTexture:(uchar*)tex_data {
  int scaled_width = (float)roi_.width*tex_scale_;
  int scaled_height = (float)roi_.height*tex_scale_;

  // Use quartz to scale image.
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef bitmap = CGBitmapContextCreate(tex_data, 
                                              scaled_width,
                                              scaled_height,
                                              8,              // bits per channel
                                              tex_width_*4,   // width_step
                                              colorSpace, 
                                              kCGImageAlphaNoneSkipLast |
                                              kCGBitmapByteOrder32Big);
  CGColorSpaceRelease (colorSpace);
  
  if (!bitmap) {
    NSLog (@"FXImage copyImageDataToTexture: Can't create bitmap context");
    return;
  }
  
  // Setup transformation matrix
  // CGContextTranslateCTM(bitmap, 0, tex_height_ - scaled_height);
  CGContextConcatCTM(bitmap, CGAffineTransformMakeScale(tex_scale_, tex_scale_));
    
  // Load into new CGImage
  CGImageRef img_ref = [self copyToNewCGImage];
  CGContextDrawImage(bitmap, CGRectMake(0, 0, width_, height_), img_ref);
  CGImageRelease(img_ref);
  CGContextRelease(bitmap);
}


- (bool) hasBorderOfAtLeast:(uint)border {
  return roi_.x >= border && 
         roi_.y >= border && 
         roi_.width + 2*border <= width_ &&
         roi_.height + 2*border <= height_;
}


- (void) dealloc {
  if (texture_id_)
    glDeleteTextures(1, &texture_id_);

  if (img_data_)
    free (img_data_);
  
  [super dealloc];
}

@end
