//
//  FXMatrix.m
//  visualFX
//
//  Created by Matthias Grundmann on 7/31/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "FXMatrix.h"
#include <vector>

@implementation FXMatrix

@synthesize width=width_, height=height_, width_step=width_step_, data=data_, roi=roi_;

@dynamic roi_data, roi_pad;

-(float*) roi_data {
  return PtrOffset(data_, width_step_*roi_.y + roi_.x*sizeof(float));
}

-(uint) roi_pad {
  return width_step_ - roi_.width*sizeof(float);
}

-(id) initWithMatrixWidth:(uint)width matrixHeight:(uint)height zeroMatrix:(bool)zero {
  if (self = [super init]) {
    width_ = width;
    height_ = height;
    width_step_ = width_ * sizeof(float);
    data_ = (float*) malloc(width_step_*height_);
    roi_ = FXRect(0, 0, width_, height_);
    
    if (!data_) {
      NSLog(@"FXMatrix init*: couldn't malloc memory.");
      return nil;
    }
    
    if (zero) {
      memset(data_, 0, width_step_*height_);
    }
  }
  return self;
}

-(id) initWithMatrixWidth:(uint)width matrixHeight:(uint)height matrixData:(float*)data {
  if (self = [self initWithMatrixWidth:width matrixHeight:height zeroMatrix:false]) {
    memcpy(data_, data, width_step_*height_);
  }
  return self;
}

-(id) initWithFXImage:(FXImage*)img {
  if (self = [self initWithMatrixWidth:img.width matrixHeight:img.height zeroMatrix:false]) {
    uchar* src_ptr = img.img_data;
    float* dst_ptr = data_;
    
    for (int i=0; i < height_; ++i) {
      for (int j=0; j < width_; ++j) {
        *dst_ptr++ = (float)*src_ptr++;
      }
    }
    roi_ = img.roi;
  }
  return self;
}

-(void) gaussianFilterWithSigma:(float)sigma radius:(int)rad outputMatrix:(FXMatrix*)output {
  if (!roi_.isEqualSize(output.roi)) {
    NSLog(@"FXMatrix:gaussianFilterWithSigma: output FXMatrix has different size.");
    return;
  }
  
  const int radius = rad;
  const int diam = 2*radius+1;
  
  if (![self hasBorderOfAtLeast:radius]) {
    NSLog(@"FXMatrix:gaussianFilterWithSigma: expecting src-matrix with border of %d", radius);
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
      space_ofs[space_sz] = i*width_step_ + j*sizeof(float);
      ++space_sz;
    }
  
  // Normalize
  weight_sum = 1.0/weight_sum;
  for (int i = 0; i < space_sz; ++i) {
    space_weights[i] *= weight_sum;
  }
  
  // Apply filter
  int src_padding = width_step_ - roi_.width*sizeof(float);
  int dst_padding = output.width_step - output.roi.width*sizeof(float);
  
  const float* src_ptr = self.roi_data;
  float* dst_ptr = output.roi_data;
  
  for (int i = 0; 
       i < roi_.height;
       ++i, src_ptr = PtrOffset(src_ptr, src_padding), dst_ptr = PtrOffset(dst_ptr, dst_padding)) {
    for (int j = 0; j < roi_.width; ++j, ++src_ptr, ++dst_ptr) {
      float val = 0;
      for (int k = 0; k < space_sz; ++k) {
        val += space_weights[k] * *PtrOffset(src_ptr, space_ofs[k]);
      }
      *dst_ptr = val;
    }
  }  
}

-(void) gaussianFilterSeparableWithSigma:(float)sigma
                                  radius:(int)rad
                               tmpMatrix:(FXMatrix*)tmpMatrix
                            outputMatrix:(FXMatrix*)output {
  if (!roi_.isEqualSize(output.roi)) {
    NSLog(@"FXMatrix:gaussianFilterSeparableWithSigma: output FXMatrix has different size.");
    return;
  }
  
  const int radius = rad;
  const int diam = 2*radius+1;
  if (![self hasBorderOfAtLeast:radius]) {
    NSLog(@"FXMatrix:gaussianFilterSeparableWithSigma: expecting src-matrix with border of %d", 
          radius);
    return;
  }
  
  bool tmpAllocated = false;
  if (tmpMatrix) {
    if (!roi_.isEqualSize(tmpMatrix.roi) || ![tmpMatrix hasBorderOfAtLeast:radius]) {
      NSLog(@"FXImage:gaussianFilterSeparableWithSigma tmp has different size or too small border");
      return;
    }
  } else {
    tmpMatrix= [[FXMatrix alloc] initWithMatrixWidth:roi_.width+2*radius
                                         matrixHeight:roi_.height+2*radius
                                            zeroMatrix:false];
    tmpMatrix.roi = FXRect(radius, radius, roi_.width, roi_.height);
    tmpAllocated = true;
  }

  [self copyReplicateBorderPixels:radius mode:BORDER_HORIZ];
  
  // Compute space offsets and weights;
  std::vector<int> space_ofs_horiz (diam);
  std::vector<int> space_ofs_vert (diam);
  std::vector<float> space_weights(diam);
  
  float weight_sum = 0;
  const float gauss_coeff = -0.5/(sigma*sigma);
  
  for (int i = -radius, idx=0; i <= radius; ++i, ++idx) { 
    space_weights[idx] = exp(static_cast<float>(i*i) * gauss_coeff);
    weight_sum += space_weights[idx];
    space_ofs_horiz[idx] = i*sizeof(float);
    space_ofs_vert[idx] = i*width_step_;
  }
  
  // Normalize
  weight_sum = 1.0/weight_sum;
  for (int i = 0; i < diam; ++i) {
    space_weights[i] *= weight_sum;
  }
  
  // Apply filter
  int src_padding = width_step_ - roi_.width*sizeof(float);
  int dst_padding = tmpMatrix.width_step - tmpMatrix.roi.width*sizeof(float);
  
  const float* src_ptr = self.roi_data;
  float* dst_ptr = tmpMatrix.roi_data;
  
  for (int i = 0; 
       i < roi_.height;
       ++i, src_ptr = PtrOffset(src_ptr, src_padding), dst_ptr = PtrOffset(dst_ptr, dst_padding)) {
    for (int j = 0; j < roi_.width; ++j, ++src_ptr, ++dst_ptr) {
      float val = 0;
      for (int k = 0; k < diam; ++k) {
        val += space_weights[k] * *PtrOffset(src_ptr, space_ofs_horiz[k]);
      }
      *dst_ptr = val;
    }
  }  
  
  [tmpMatrix copyReplicateBorderPixels:radius mode:BORDER_VERT];
  
  // Apply filter
  src_padding = dst_padding;
  dst_padding = output.width_step - output.roi.width*sizeof(float);
  
  src_ptr = tmpMatrix.roi_data;
  dst_ptr = output.roi_data;
  
  for (int i = 0; 
       i < roi_.height;
       ++i, src_ptr = PtrOffset(src_ptr, src_padding), dst_ptr = PtrOffset(dst_ptr, dst_padding)) {
    for (int j = 0; j < roi_.width; ++j, ++src_ptr, ++dst_ptr) {
      float val = 0;
      for (int k = 0; k < diam; ++k) {
        val += space_weights[k] * *PtrOffset(src_ptr, space_ofs_vert[k]);
      }
      *dst_ptr = val;
    }
  }  
  
  if (tmpAllocated)
    [tmpMatrix release];
}

-(void) copyToMatrix:(FXMatrix*)dst {
  if (!roi_.isEqualSize(dst.roi) ) {
    NSLog (@"FXMatrix:copyToMatrix: Matrices differ in size.");
    return;
  }
  
  float* src_ptr = self.roi_data;
  float* dst_ptr = dst.roi_data;
  
  if (roi_.width * sizeof(float) == width_step_ &&
      dst->roi_.width * sizeof(float) == dst->width_step_) {
    memcpy(dst_ptr, src_ptr, width_step_*roi_.height);
  } else {
    for (int i = 0; 
         i < roi_.height;
         ++i, src_ptr = PtrOffset(src_ptr, width_step_), 
              dst_ptr = PtrOffset(dst_ptr, dst->width_step_)) {
      memcpy(dst_ptr, src_ptr, roi_.width*sizeof(float)); 
    }
  }
}

-(void) copyToMatrix:(FXMatrix*)dst usingMask:(FXImage*)mask {
  if (!roi_.isEqualSize(dst.roi) || 
      !roi_.isEqualSize(mask.roi) ||
      mask.channels != 1) {
    NSLog(@"FXMatrix:copyToMatrix:usingMask: Matrices and mask differ in size or mask not "
          @"grayscale");
    return;
  }
  
  float* src_ptr = self.roi_data;
  float* dst_ptr = dst.roi_data;
  uchar* mask_ptr = mask.roi_data;
  
  const int src_pad = self.roi_pad;
  const int dst_pad = dst.roi_pad;
  const int mask_pad = mask.roi_pad;
  
  const int width = roi_.width;
  const int height = roi_.height;
  
  for (int i = 0; 
       i < height;
       ++i, src_ptr = PtrOffset(src_ptr, src_pad), dst_ptr = PtrOffset(dst_ptr, dst_pad),
       mask_ptr += mask_pad) {
    for (int j = 0; j < width; ++j, ++src_ptr, ++dst_ptr, ++mask_ptr) {
      if (*mask_ptr) {
        *dst_ptr = *src_ptr;
      }
    }
  }
}

-(void) copyToMatrix:(FXMatrix*)dst replicateBorder:(int)border {
  if (![dst hasBorderOfAtLeast:border]) {
    NSLog(@"FXMatrix:copyToMatrix:replicateBorder: " 
          @"Specified border around ROI exceeds image dimensions");
    return;
  }
  
  if (!roi_.isEqualSize(dst->roi_)) {
    NSLog (@"FXMatrix:copyToMatrix:replicateBorder: "
           "Matrices differ in size.");
    return;
  }
  
  
  float* src_ptr = self.roi_data;
  float* dst_ptr = PtrOffset(dst.roi_data, -dst.width_step*border - border*sizeof(float));
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = dst.width_step - (width+border2)*sizeof(float);
  
  for (int j = 0; j < height+border2; ++j, dst_ptr = PtrOffset(dst_ptr, dst_pad)) {
    int i = 0;
    while (i < border) {
      *dst_ptr++ = src_ptr[0];
      ++i;
    }
    
    // Always copy
    memcpy (dst_ptr, src_ptr, width*sizeof(float));
    
    i += width;
    dst_ptr += width;
    
    while (i < width+border2) {
      *dst_ptr++ = src_ptr[width-1];
      ++i;
    }
    
    if (j >= border && j < height + border - 1)
      src_ptr = PtrOffset(src_ptr, width_step_);
  }    
}


- (void) copyReplicateBorderPixels:(int)border {
  [self copyReplicateBorderPixels:border mode:BORDER_BOTH];
}

- (void) copyReplicateBorderPixels:(int)border mode:(BorderMode)mode {
  if (![self hasBorderOfAtLeast:border]) {
    NSLog(@"FXMatrix:copyReplicateBorderPixels: " 
          @"Specified border around ROI exceeds image dimensions");
    return;
  }
  
  float* src_ptr = self.roi_data;
  float* dst_ptr;
  
  if (mode == BORDER_HORIZ) {
    dst_ptr = PtrOffset(src_ptr, - border*sizeof(float));
  } else {
    dst_ptr = PtrOffset(src_ptr, -width_step_*border - border*sizeof(float));
  }
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = width_step_ - (width+border2)*sizeof(float);
  
  const int start_row = mode == BORDER_HORIZ ? border : 0;
  const int end_row = mode == BORDER_HORIZ ? height+border : height+border2;
  
  for (int j = start_row; j < end_row; ++j, dst_ptr = PtrOffset(dst_ptr,dst_pad)) {
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
      memcpy (dst_ptr, src_ptr, width*sizeof(float));
    
    i += width;
    dst_ptr += width;
    
    if (mode == BORDER_VERT) {
      dst_ptr += border;
    } else {
      while (i < width+border2) {
        *dst_ptr++ = src_ptr[width-1];
        ++i;
      }
    }
    
    if (j >= border && j < height + border - 1)
      src_ptr = PtrOffset(src_ptr, width_step_);
  }  
}

-(void) setBorder:(int)border toConstant:(float)value {
  [self setBorder:border toConstant:value mode:BORDER_BOTH];
}

-(void) setBorder:(int)border toConstant:(float)value mode:(BorderMode)mode {
  if (![self hasBorderOfAtLeast:border]) {
    NSLog(@"FXMatrix:copyReplicateBorderPixels: " 
          @"Specified border around ROI exceeds image dimensions");
    return;
  }
  
  float* src_ptr = self.roi_data;
  float* dst_ptr;
  
  if (mode == BORDER_HORIZ) {
    dst_ptr = PtrOffset(src_ptr, - border*sizeof(float));
  } else {
    dst_ptr = PtrOffset(src_ptr, -width_step_*border - border*sizeof(float));
  }
  
  const int width = roi_.width;
  const int height = roi_.height;
  const int border2 = 2*border;
  
  const int dst_pad = width_step_ - (width+border2)*sizeof(float);
  
  const int start_row = mode == BORDER_HORIZ ? border : 0;
  const int end_row = mode == BORDER_HORIZ ? height+border : height+border2;
  
  for (int j = start_row; j < end_row; ++j, dst_ptr = PtrOffset(dst_ptr,dst_pad)) {
    int i = 0;
    if (mode == BORDER_VERT) {
      i = border;
      dst_ptr += border;
    } else {
      while (i < border) {
        *dst_ptr++ = value;
        ++i;
      }
    }
    
    // only copy if at border
    if ( j < border || j >= height+border) {
      for (int k = 0; k < width; ++k)
        *dst_ptr++ = value;
      i += width;
    }
    else {
      i += width;
      dst_ptr += width;
    }
    
    if (mode == BORDER_VERT) {
      dst_ptr += border;
    } else {
      while (i < width+border2) {
        *dst_ptr++ = value;
        ++i;
      }
    }
  }  
}

-(void) convertToGrayImage:(FXImage*)img {
  if (!roi_.isEqualSize(img.roi) || img.channels != 1) {
    NSLog(@"FXMatrix:convertToGrayImage: size differ of img not grayscale");
  }
  
  // Determine min and max value in matrix.
  float min_val, max_val;
  [self minimumValue:&min_val maximumValue:&max_val];
  
  if (fabs(max_val - min_val) < 1e-30) {
    NSLog(@"FXMatrix:convertToGrayImage: Matrix is constant. Aborting.");
    return;
  }
  
  const float denom = 1.0/(max_val - min_val)*255.0;
  
  float* src_ptr = self.roi_data;
  const uint src_padding = width_step_ - roi_.width*sizeof(float);
  
  uchar* dst_ptr = img.roi_data;
  const uint dst_padding = img.width_step - img.roi.width;
  
  for (int i = 0; 
       i < roi_.height; 
       ++i, src_ptr = PtrOffset(src_ptr, src_padding), dst_ptr += dst_padding) {
    for (int j = 0; j < roi_.width; ++j, ++src_ptr, ++dst_ptr) {
      *dst_ptr = (uchar) ((*src_ptr - min_val)*denom);
    }
  }
}


-(void) scaleBy:(float)scale {
  float* src_dst = self.roi_data;
  const uint pad = self.roi_pad;
  
  for (int i = 0; i < roi_.height; ++i, src_dst = PtrOffset(src_dst, pad)) {
    for (int j = 0; j < roi_.width; ++j, ++src_dst) {
      *src_dst *= scale;
    }
  }
}

-(void) setToConstant:(float)value {
  float* src_dst = self.roi_data;
  const int src_pad = self.roi_pad;
  const int width = roi_.width;
  const int height = roi_.height;
  
  for (int i = 0; i < height; ++i, src_dst = PtrOffset(src_dst, src_pad)) {
      for (int j = 0; j < width; ++j, ++src_dst) {
        *src_dst = value;
      }
  }
}

-(void) setToConstant:(float)value usingMask:(FXImage*)mask {
    if (!roi_.isEqualSize(mask.roi)) {
      NSLog(@"FXMatrix:setToConstant:usingMask: matrix and mask must have the same size.");
      return;
    }
    
    float* src_dst = self.roi_data;
    const int src_pad = self.roi_pad;
    uchar* mask_ptr = mask.roi_data;
    const int mask_pad = mask.roi_pad;
    
    const int width = roi_.width;
    const int height = roi_.height;
    
    for (int i = 0;
         i < height;
         ++i, src_dst = PtrOffset(src_dst, src_pad), mask_ptr += mask_pad) {
      for (int j = 0; j < width; ++j, ++src_dst, ++mask_ptr) {
        if (*mask_ptr)
          *src_dst = value;
      }
    }
    
}

-(void) minimumValue:(float*)p_min_val maximumValue:(float*)p_max_val {
  float* src_ptr = self.roi_data;
  const uint src_padding = width_step_ - roi_.width*sizeof(float);
  
  float min_val = 1e38;
  float max_val = -1e38;
  
  for (int i = 0; i < roi_.height; ++i, src_ptr = PtrOffset(src_ptr, src_padding)) {
    for (int j=0; j < roi_.width; ++j, ++src_ptr) {
      if (*src_ptr < min_val)
        min_val = *src_ptr;
      if (*src_ptr > max_val)
        max_val = *src_ptr;
    }
  }
  
  *p_min_val = min_val;
  *p_max_val = max_val;
}

-(float) valueAtX:(uint)x andY:(uint)y {
  return *PtrOffset(data_, width_step_*(roi_.y + y) + (roi_.x + x)*sizeof(float));
}

-(float*) mutableValueAtX:(uint)x andY:(uint)y {
  return PtrOffset(data_, width_step_*(roi_.y + y) + (roi_.x + x)*sizeof(float));  
}

- (BOOL) hasBorderOfAtLeast:(uint)border {
  return roi_.x >= border && 
  roi_.y >= border && 
  roi_.width + 2*border <= width_ &&
  roi_.height + 2*border <= height_;
}

-(void) dealloc {
  if(data_)
    free(data_);
  [super dealloc];
}

@end
