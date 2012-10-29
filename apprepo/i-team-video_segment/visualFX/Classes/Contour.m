//
//  Contour.m
//  visualFX
//
//  Created by Matthias Grundmann on 8/25/08.
//  Copyright 2008 Matthias Grundmann. All rights reserved.
//

#import "Contour.h"

#import "FXImage.h"
#import "FXMatrix.h"
#include <vector>
#include <algorithm>
#include <limits>

@implementation Contour

-(id) init {
  if (self= [super init]) {
    ;
  }
  return self;
}

+(int) createMask:(FXImage**)selection
    fromSelection:(const vector<vector<CGPoint> >*)points
      pointRadius:(const vector<float>*)pt_scales
      pointRadius:(float)pt_radius
     overallScale:(float)scale
           border:(int)border {
  // Determine image render size
  CGPoint min_pt = CGPointMake(1e38, 1e38);
  CGPoint max_pt = CGPointMake(-1e38, -1e38);
  int     max_extension = 0;
  
  int num_scales = pt_scales->size();
  
  for (int i = 0; i < num_scales; ++i) {
    CGPoint min_pt_local = CGPointMake(1e38, 1e38);
    CGPoint max_pt_local = CGPointMake(-1e38, -1e38);
    for (vector<CGPoint>::const_iterator p = (*points)[i].begin();
       p != (*points)[i].end();
       ++p) {
      min_pt_local.x = std::min(min_pt_local.x, p->x);
      min_pt_local.y = std::min(min_pt_local.y, p->y);
      max_pt_local.x = std::max(max_pt_local.x, p->x);
      max_pt_local.y = std::max(max_pt_local.y, p->y);
    }
    
    max_extension = std::max(max_extension, (int) ceil(pt_radius * (*pt_scales)[i] * scale));
    min_pt.x = std::min(min_pt.x, min_pt_local.x);
    min_pt.y = std::min(min_pt.y, min_pt_local.y);
    
    max_pt.x = std::max(max_pt.x, max_pt_local.x);
    max_pt.y = std::max(max_pt.y, max_pt_local.y);
  }
    
  int trans = border+max_extension;
  int img_width = ceil((max_pt.x - min_pt.x) * scale) + 2*trans;
  int img_height = ceil((max_pt.y - min_pt.y) * scale) + 2*trans;
  
  *selection = [[FXImage alloc] initWithImgWidth:img_width
                                       imgHeight:img_height
                                     imgChannels:0];
  [*selection setToConstant:255];
  
  for (int ns = 0; ns < num_scales; ++ns) {
    const int radius = pt_radius * (*pt_scales)[ns] * scale + 0.5;
  
    // Calculate space offsets.
    const int diam = 2*radius+1;
    const int radius2 = radius*radius;
    uint space_sz = 0;
    std::vector<int> space_ofs (diam*diam);
  
    for (int i = -radius; i <= radius; ++i) 
      for (int j = -radius; j <= radius; ++j) {
        if (i*i+j*j > radius2)
          continue;
        space_ofs[space_sz] = i * (*selection).width_step + j;
        ++space_sz;
      }
  
    // Render each blob.
    uchar* src_ptr = (*selection).roi_data;
  
    for (vector<CGPoint>::const_iterator i = (*points)[ns].begin();
       i != (*points)[ns].end();
       ++i) {
      int x = (int)(i->x * scale + 0.5) + trans;
      int y = (int)(i->y * scale + 0.5) + trans;
      uchar* ptr = src_ptr + y * (*selection).width_step + x;
      for (int k = 0; k < space_sz; ++k) {
        *(ptr + space_ofs[k]) = 0;
      }
    }
  }
  
  return trans;
}

+(void) tangentsFromImage:(FXImage*)image 
             filterRadius:(int)radius
               iterations:(int)max_iter
                  fillAll:(bool)fill_all
                 tangentX:(FXMatrix*)tangent_x 
                 tangentY:(FXMatrix*)tangent_y {
  
  uint img_width = image.roi.width;
  uint img_height = image.roi.height;
  
  if (!image.roi.isEqualSize(tangent_x.roi) || !image.roi.isEqualSize(tangent_y.roi)) {
    NSLog(@"Output matrices must have same ROI as input image");
    return;
  }
  
  FXMatrix* gra_X = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                             matrixHeight:img_height + 2*radius
                                               zeroMatrix:false];
  
  FXMatrix* gra_Y = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                             matrixHeight:img_height + 2*radius
                                               zeroMatrix:false];
  
  gra_X.roi = FXRect(radius, radius, img_width, img_height);
  gra_Y.roi = FXRect(radius, radius, img_width, img_height);
  
  [image copyReplicateBorderPixels:1];
  [image computeGradientX:gra_X gradientY:gra_Y];
  
  // Normalize and compute weights.
  FXMatrix* gra_magn = [[FXMatrix alloc] initWithMatrixWidth:img_width + 2*radius
                                                matrixHeight:img_height + 2*radius
                                                  zeroMatrix:false];

  gra_magn.roi = FXRect(radius, radius, img_width, img_height);
  
  float* src_x = gra_X.roi_data;
  float* src_y = gra_Y.roi_data;
  float* dst_magn = gra_magn.roi_data;
  
  const uint src_x_pad = gra_X.roi_pad;
  const uint src_y_pad = gra_Y.roi_pad;
  const uint dst_pad = gra_magn.roi_pad;
  
  float max_magn = -1e38;
  
  for (int i = 0; 
       i < img_height;
       ++i, src_x = PtrOffset(src_x, src_x_pad), src_y = PtrOffset(src_y, src_y_pad),
       dst_magn = PtrOffset(dst_magn, dst_pad)) {
    for (int j = 0; j < img_width; ++j, ++src_x, ++src_y, ++dst_magn) {
      float magnitude = sqrt(*src_x * *src_x + *src_y * *src_y);
      *dst_magn = magnitude;
      if (magnitude > max_magn) 
        max_magn = magnitude;
      // Invert to point towards minima
      if (magnitude > 1e-10) {
        magnitude = -1.0/magnitude;
        *src_x *= magnitude;
        *src_y *= magnitude;
      } else {
        *src_x = 0;
        *src_y = 0;
      }
    }
  }
  
  // Magnitude to be in interval 0 .. 1
  [gra_magn scaleBy:1.0/max_magn];
  
  [gra_X copyReplicateBorderPixels:radius];
  [gra_Y copyReplicateBorderPixels:radius];
  [gra_magn copyReplicateBorderPixels:radius];
  
  // Setup box filter and space-ofs
  const int diam = 2*radius+1;
  const int radius2 = radius*radius;
  uint space_sz = 0;
  std::vector<int> space_ofs (diam*diam);
  
  // gra_X, gra_Y and gra_magn should have equal width_step
  if (gra_X.width_step != gra_Y.width_step || gra_X.width_step != gra_magn.width_step) {
    NSLog(@"Contour:tangentsFromImage: width_step among internal matrices differ.");
    return;
  }
  
  for (int i = -radius; i <= radius; ++i) 
    for (int j = -radius; j <= radius; ++j) {
      if (i*i+j*j > radius2)
        continue;
      space_ofs[space_sz] = i*gra_X.width_step + j*sizeof(float);
      ++space_sz;
    }
  
  for (int iter = 0; iter < max_iter; ++iter) {
    const float* src_x = gra_X.roi_data;
    const float* src_y = gra_Y.roi_data;
    const float* src_magn = gra_magn.roi_data;
    float* dst_x = tangent_x.roi_data;
    float* dst_y = tangent_y.roi_data;
    
    const uint src_x_pad = gra_X.roi_pad;
    const uint src_y_pad = gra_Y.roi_pad;
    const uint src_magn_pad = gra_magn.roi_pad;
    const uint dst_x_pad = tangent_x.roi_pad;
    const uint dst_y_pad = tangent_y.roi_pad;
    
    uint zero_elems = 0;
    
    for (int i = 0;
         i < img_height;
         ++i, src_x = PtrOffset(src_x, src_x_pad), src_y = PtrOffset(src_y, src_y_pad),
         src_magn = PtrOffset(src_magn, src_magn_pad), dst_x = PtrOffset(dst_x, dst_x_pad),
         dst_y = PtrOffset(dst_y, dst_y_pad)) {
      for (int j = 0; j < img_width; ++j, ++src_x, ++src_y, ++src_magn, ++dst_x, ++dst_y) {
        const float my_magn = *src_magn;
        float gra_weight = 0, weight_sum = 0, x_tangent = 0, y_tangent = 0;
        
        for (int k = 0; k < space_sz; ++k) {
          gra_weight = 0.5 * (1 + *PtrOffset(src_magn, space_ofs[k]) - my_magn);
          x_tangent += *PtrOffset(src_x, space_ofs[k]) * gra_weight;
          y_tangent += *PtrOffset(src_y, space_ofs[k]) * gra_weight;
          weight_sum += gra_weight;
        }
        
        // gra_weight is always > 0;
        weight_sum = 1.0 / weight_sum;
        x_tangent *= weight_sum;
        y_tangent *= weight_sum;
        
        float norm = x_tangent*x_tangent + y_tangent*y_tangent;
        if (norm > 1e-20) {
          norm = 1.0/sqrt(norm);
          *dst_x = x_tangent * norm;
          *dst_y = y_tangent * norm;
        } else {
          *dst_x = 0;
          *dst_y = 0;
          ++zero_elems;
        }
      }
    }
    
    // Finished?
    if(fill_all && zero_elems == 0)
      break;
    if (iter != max_iter) {
      [tangent_x copyToMatrix:gra_X replicateBorder:radius];
      [tangent_y copyToMatrix:gra_Y replicateBorder:radius];
    }
  }
  
  [gra_magn release];
  [gra_Y release];
  [gra_X release];
}

+(int) moveContour:(std::vector<CGPoint>*)contour
          tangentX:(FXMatrix*)tangent_x 
          tangentY:(FXMatrix*)tangent_y
             image:(FXImage*)img 
         maxRadius:(int)rad {
  // Compute local coordinate system
  const int num_points = contour->size();
  
  std::vector<CGPoint> curvature(num_points);
  std::vector<CGPoint> normal(num_points);
  
  for (int i = 0; i < num_points; ++i) {
    // Difference to previous and next point.
    CGPoint diff_prev = CGPointSub((*contour)[i], (*contour)[(i+num_points-1)%num_points]);
    CGPoint diff_next = CGPointSub((*contour)[(i+1)%num_points], (*contour)[i]);
    
    diff_prev = CGPointNormalize(diff_prev);
    diff_next = CGPointNormalize(diff_next);
    
    curvature[i] = CGPointSub(diff_next, diff_prev);
    CGPoint tangent = CGPointNormalize(CGPointAdd(diff_prev, diff_next));
    normal[i] = CGPointMake(tangent.y, -tangent.x);
  }
  
  // Compute forces
  std::vector<float> internal(num_points);  // Curvature along normal filtered with [-1/2 1 -1/2]
  std::vector<float> external(num_points);  // Tangent-Field component along normal
  for (int i = 0; i < num_points; ++i) {
    const CGPoint my_normal = normal[i];
    int pos_x = (*contour)[i].x;
    int pos_y = (*contour)[i].y;
    
    external[i] = CGPointDot(CGPointMake([tangent_x valueAtX:pos_x andY:pos_y],
                                         [tangent_y valueAtX:pos_x andY:pos_y]),
                             normal[i]);
    int i_prev = (i+num_points-1)%num_points;
    int i_next = (i+1)%num_points;
    internal[i] = -0.5 * CGPointDot(curvature[i_prev], normal[i_prev]) +
                         CGPointDot(curvature[i], normal[i]) +
                  -0.5 * CGPointDot(curvature[i_next], normal[i_next]);
    
    // Limit internal energy
    internal[i] = std::max<float>(-rad, std::min<float>(rad, internal[i]));
  }
  
  // Apply forces to contour
  int ptns_moved = 0;
  for (int i = 0; i < num_points; ++i) {
    CGPoint my_pos = (*contour)[i];
    if (![img valueAtX:my_pos.x andY:my_pos.y]) {  // Reached border
      continue;
    } else {
      CGPoint new_pt = CGPointAdd(my_pos, CGPointScale(normal[i], 0.5*internal[i] + external[i]));
      new_pt.x = floor(new_pt.x + 0.5);
      new_pt.y = floor(new_pt.y + 0.5);
      if (!CGPointEqualToPoint(new_pt, my_pos)) {
        ++ptns_moved;
        (*contour)[i] = new_pt;
      }
    }
  }
  
  return ptns_moved;
}


+(void) displaceContour:(std::vector<CGPoint>*)contour 
              byVectors:(const std::vector<CGPoint>*)disp
                  scale:(float)s {
  for (int i = 0; i < contour->size(); ++i) {
    (*contour)[i] = CGPointAdd((*contour)[i], CGPointScale((*disp)[i], s));
  }
}
  
+(void) computeContour:(std::vector<CGPoint>*)contour
    radialDisplacement:(std::vector<CGPoint>*)disp {
  const int num_points = contour->size();
  if (disp->size() != num_points) {
    NSLog(@"Contour:computeContour:radialDisplacement: input vector differ in size.");
    return;
  }
  
  for (int i = 0; i < num_points; ++i) {
    CGPoint prev = (*contour)[(i+num_points-1)%num_points];
    CGPoint next = (*contour)[(i+1)%num_points];
    (*disp)[i] = CGPointSub((*contour)[i], CGPointScale(CGPointAdd(prev, next), 0.5));
  }
}


+(void) smoothContour:(std::vector<CGPoint>*)contour
           iterations:(int)iterations {
  for (int iter = 0; iter < iterations; ++iter) {
    const int num_points = contour->size();
    std::vector<CGPoint> mid_pts(num_points*2);
    std::vector<CGPoint> disp(num_points*2);
    
    for (int i = 0; i < num_points; ++i) {
      mid_pts[2*i] = (*contour)[i];
      mid_pts[2*i+1] = CGPointScale(CGPointAdd((*contour)[i], (*contour)[(i+1)%num_points]), 0.5);
    }
    
    [Contour computeContour:&mid_pts radialDisplacement:&disp];
    [Contour displaceContour:&mid_pts byVectors:&disp scale:0.5];
    [Contour computeContour:&mid_pts radialDisplacement:&disp];
    [Contour displaceContour:&mid_pts byVectors:&disp scale:-1];
    contour->swap(mid_pts);
  }
}

+(void) reparamContour:(std::vector<CGPoint>*)contour segmentSize:(float)segs_size {
  const float seg_low = segs_size*0.5;
  const float seg_high = segs_size*1.5;
  
  bool skip = false;
  const int num_points = contour->size();
  
  std::vector<CGPoint> result;
  result.reserve(num_points);
  
  for (int i = 0; i < num_points; ++i) {
    if (skip) {
      skip = false;
      continue;
    }
    CGPoint p = (*contour)[i];
    CGPoint diff = CGPointSub((*contour)[(i+1)%num_points], p);
    float diff_norm = CGPointNorm(diff);
    if (diff_norm <= seg_low) {             // Merge.
      // Last pt? Just erase.
      if (i == num_points-1) 
        break;
      result.push_back(CGPointAdd(p, CGPointScale(diff, 0.5)));
      skip = true;
    } else if (diff_norm >= seg_high) {     // Split.
      result.push_back(p);      
      result.push_back(CGPointAdd(p, CGPointScale(diff, 0.5)));
    } else {
      result.push_back(p);
    }
  }
  contour->swap(result);
}

+(void) contourFromImage:(FXImage*)img initialContour:(std::vector<CGPoint>*)contour {
  const int img_width = img.roi.width;
  const int img_height = img.roi.height;
  
  FXMatrix* tangent_x = [[FXMatrix alloc] initWithMatrixWidth:img_width
                                                 matrixHeight:img_height
                                                   zeroMatrix:false];
  FXMatrix* tangent_y = [[FXMatrix alloc] initWithMatrixWidth:img_width
                                                 matrixHeight:img_height
                                                   zeroMatrix:false];
  
  [Contour tangentsFromImage:img 
                filterRadius:2 
                  iterations:30 
                     fillAll:false 
                    tangentX:tangent_x 
                    tangentY:tangent_y];
  
  const float seg_size = 4.0*sqrt(2.0);
  
  for (int i = 0; i < 60; ++i) {
    [Contour moveContour:contour tangentX:tangent_x tangentY:tangent_y image:img maxRadius:1];
    [Contour reparamContour:contour segmentSize:seg_size];
  }
  
  [Contour smoothContour:contour iterations:2];
  
  [tangent_y release];
  [tangent_x release];
}

+(void) erodeImage:(FXImage*)src_img tmpImage:(FXImage*)dst_img radius:(int)radius {
  if (!src_img.roi.isEqualSize(dst_img.roi) || 
      ![src_img hasBorderOfAtLeast:radius] ||
      ![dst_img hasBorderOfAtLeast:radius] || 
      src_img.channels != 1 ||
      dst_img.channels != 1) {
    NSLog(@"Contour:erodeImage: Input images must have one channel, same ROI and "
          @"border of at least radius");
    return;
  }
  
  // Erode along x dimension
  uchar* src_ptr = src_img.roi_data;
  uchar* dst_ptr = dst_img.roi_data;
  
  int src_pad = src_img.roi_pad;
  int dst_pad = dst_img.roi_pad;
  
  const int img_width = src_img.roi.width;
  const int img_height = src_img.roi.height;
  const int radius2 = radius + radius;
  uchar min_val;

  // Horizontal.
  for (int i = 0; i < img_height; ++i, src_ptr += src_pad, dst_ptr += dst_pad) {
    int old_min_idx = 0;
    for (int j = 0; j < img_width; ++j, ++src_ptr, ++dst_ptr) {
      // Is old minimum still in kernel ?
      if (old_min_idx) {
        // Yes --> only check new elem.
        if (src_ptr[radius] <= min_val) {
          min_val = src_ptr[radius];
          old_min_idx = radius2;
        } else {
          --old_min_idx;
        }
      } else {
        // No, determine minimum the old way.
        min_val = src_ptr[-radius];
        for (int k=-radius+1; k <= radius; ++k) {
          if (src_ptr[k] <= min_val) {
            min_val = src_ptr[k];
            old_min_idx = k+radius;
          }
        }
      }
      *dst_ptr = min_val;
    }
  }

  // Vertical.
  [dst_img copyReplicateBorderPixels:radius];// mode:BORDER_VERT];
  
  // Erode along x dimension
  src_ptr = dst_img.roi_data;
  dst_ptr = src_img.roi_data;
  
  src_pad = dst_img.roi_pad;
  dst_pad = src_img.roi_pad;
  
  const int diam = 2*radius+1;
  uchar cur_elem;
  std::vector<int> space_ofs(diam);
  for (int i = -radius; i <= radius; ++i)
    space_ofs[i+radius] = i*dst_img.width_step;
  
  for (int i = 0; i < img_height; ++i, src_ptr += src_pad, dst_ptr += dst_pad) {
    for (int j = 0; j < img_width; ++j, ++src_ptr, ++dst_ptr) {
      min_val = *(src_ptr + space_ofs[0]);
      for (int k=1; k < diam ; ++k) {
        cur_elem = *(src_ptr + space_ofs[k]);
        if (cur_elem <= min_val) {
          min_val = cur_elem;
        }
      }
      *dst_ptr = min_val;
    }
  }
}

+(void) dilateImage:(FXImage*)src_img tmpImage:(FXImage*)dst_img radius:(int)radius {
  if (!src_img.roi.isEqualSize(dst_img.roi) || 
      ![src_img hasBorderOfAtLeast:radius] ||
      ![dst_img hasBorderOfAtLeast:radius] || 
      src_img.channels != 1 ||
      dst_img.channels != 1) {
    NSLog(@"Contour:dilateImage: Input images must have one channel, same ROI and "
          @"border of at least radius");
    return;
  }
  
  // Dilate along x dimension
  uchar* src_ptr = src_img.roi_data;
  uchar* dst_ptr = dst_img.roi_data;
  
  int src_pad = src_img.roi_pad;
  int dst_pad = dst_img.roi_pad;
  
  const int img_width = src_img.roi.width;
  const int img_height = src_img.roi.height;
  const int radius2 = radius + radius;
  uchar max_val;
  
  // Horizontal.
  for (int i = 0; i < img_height; ++i, src_ptr += src_pad, dst_ptr += dst_pad) {
    int old_max_idx = 0;
    for (int j = 0; j < img_width; ++j, ++src_ptr, ++dst_ptr) {
      // Is old minimum still in kernel ?
      if (old_max_idx) {
        // Yes --> only check new elem.
        if (src_ptr[radius] >= max_val) {
          max_val = src_ptr[radius];
          old_max_idx = radius2;
        } else {
          --old_max_idx;
        }
      } else {
        // No, determine minimum the old way.
        max_val = src_ptr[-radius];
        for (int k=-radius+1; k <= radius; ++k) {
          if (src_ptr[k] >= max_val) {
            max_val = src_ptr[k];
            old_max_idx = k+radius;
          }
        }
      }
      *dst_ptr = max_val;
    }
  }
  
  // Vertical.
  [dst_img copyReplicateBorderPixels:radius];// mode:BORDER_VERT];
  
  // Erode along x dimension
  src_ptr = dst_img.roi_data;
  dst_ptr = src_img.roi_data;
  
  src_pad = dst_img.roi_pad;
  dst_pad = src_img.roi_pad;
  
  const int diam = 2*radius+1;
  uchar cur_elem;
  std::vector<int> space_ofs(diam);
  for (int i = -radius; i <= radius; ++i)
    space_ofs[i+radius] = i*dst_img.width_step;
  
  for (int i = 0; i < img_height; ++i, src_ptr += src_pad, dst_ptr += dst_pad) {
    for (int j = 0; j < img_width; ++j, ++src_ptr, ++dst_ptr) {
      max_val = *(src_ptr + space_ofs[0]);
      for (int k=1; k < diam ; ++k) {
        cur_elem = *(src_ptr + space_ofs[k]);
        if (cur_elem >= max_val) {
          max_val = cur_elem;
        }
      }
      *dst_ptr = max_val;
    }
  }
}

+(void) regionBorderFromImage:(FXImage*)img 
                 regionBorder:(vector<FXPoint>*)border 
                borderNormals:(vector<CGPoint>*)normals {
  if (img.channels != 1 || ![img hasBorderOfAtLeast:1]) {
    NSLog(@"Contour:regionBorderFromImage: img must have one channel and border of at least 1.");
    return;
  }
  
  [img setBorder:1 toConstant:0];
  
  uchar* src_ptr = img.roi_data;
  const int src_pad = img.roi_pad;
  const int img_width = img.roi.width;
  const int img_height = img.roi.height;
  const int step = img.width_step;
  int left, right, top, bottom;
  const float sqrt2inv = 1.0/sqrt(2);
  
  border->clear();
  border->reserve(128);
  normals->clear();
  normals->reserve(128);
  
  for (int i = 0; i < img_height; ++i, src_ptr += src_pad) {
    for (int j = 0; j < img_width; ++j, ++src_ptr) {
      if (*src_ptr) {
        // hand coded laplace filter
        left = src_ptr[-1];
        right = src_ptr[1];
        top = src_ptr[-step];
        bottom = src_ptr[step];
        if (left + right + top + bottom < 4) {
          // Border pixel
          border->push_back (FXPoint(j,i));
          // Calculate normal.
          right -= left;    // normal x
          bottom -= top;    // normal y
          // There are just three cases for normal magnitude in binary images: 0 1 and 2.
          // Only normalize 2
          if (right*right + bottom*bottom == 2) {
            normals->push_back(CGPointMake((float)right*sqrt2inv, (float)bottom*sqrt2inv));
          } else {
            normals->push_back(CGPointMake(right, bottom));
          }
        }
      }
    }
  }
}

+(void) computeGradientX:(FXMatrix*)gradient_x 
               gradientY:(FXMatrix*)gradient_y 
               fromImage:(FXImage*)gray_img
                atPoints:(vector<FXPoint>*)points 
                 results:(vector<CGPoint>*)results {
  if (!gradient_x.roi.isEqualSize(gradient_y.roi) ||
      !gradient_x.roi.isEqualSize(gray_img.roi) ||
      gray_img.channels != 1 ||
      ![gray_img hasBorderOfAtLeast:1]) {
    NSLog(@"Contour:computeGradientX input images/matrices differ in size or gray_img is not"
          @" 1 channel image with border of at least 1");
    return;
  }
  
  if (results->size() != points->size())
    results->resize(points->size());
  
  const int step = gray_img.width_step;
  int val, x_val, y_val;
  uchar* sptr, *sptr_tmp;
  int idx = 0;

  for (vector<FXPoint>::const_iterator p = points->begin(); p != points->end(); ++p, ++idx) {
    sptr = [gray_img mutableValueAtX:p->x andY:p->y];
    sptr_tmp = sptr-step;
    val = *(sptr_tmp-1);
    x_val = -val;
    y_val = -val;

    y_val -= 2 * *(sptr_tmp);
    
    val = *(sptr_tmp+1);
    x_val += val;
    y_val -= val;
    
    x_val += 2 * ( *(sptr+1) - *(sptr-1));
    
    sptr_tmp = sptr+step;
    val = *(sptr_tmp-1);
    x_val -= val;
    y_val += val;
    
    y_val += 2 * *sptr_tmp;
    
    val = *(sptr_tmp+1);
    x_val += val;
    y_val += val;
  
    CGPoint gradient = { (float)x_val * (1.0/8.0), (float)y_val * (1.0/8.0) };
    *[gradient_x mutableValueAtX:p->x andY:p->y] = gradient.x;
    *[gradient_y mutableValueAtX:p->x andY:p->y] = gradient.y;
    
    (*results)[idx] = gradient;
  }
}

+(void) sampleGradientX:(FXMatrix*)gradient_x 
              gradientY:(FXMatrix*)gradient_y 
               atPoints:(vector<FXPoint>*)points
                results:(vector<CGPoint>*)results {
  if (results->size() != points->size())
    results->resize(points->size());
    
  int idx = 0;
  for (vector<FXPoint>::const_iterator p = points->begin(); p != points->end(); ++p, ++idx) {
    CGPoint gradient = CGPointMake([gradient_x valueAtX:p->x andY:p->y],
                                   [gradient_y valueAtX:p->x andY:p->y]);
    (*results)[idx] = gradient;
  }
}


+(void) convertMask:(FXImage*)mask toConfidence:(FXMatrix*)confidence {
  if (!confidence.roi.isEqualSize(mask.roi) || mask.channels != 1) {
    NSLog(@"Contour:convertMask inputs must be of same size and mask grayscale.");
  }
  
  uchar* src = mask.roi_data;
  const int src_pad = mask.roi_pad;
  float* dst = confidence.roi_data;
  const int dst_pad = confidence.roi_pad;
  
  const int img_width = mask.roi.width;
  const int img_height = mask.roi.height;
  
  for (int i = 0; i < img_height; ++i, src += src_pad, dst = PtrOffset(dst, dst_pad)) {
    for (int j = 0; j < img_width; ++j, ++src, ++dst) {
      *dst = 1-(int)*src;
    }
  }
}

+(void) getConfidence:(FXMatrix*)confidence 
             atPoints:(vector<FXPoint>*)points
               radius:(int)radius
              results:(vector<float>*)results {
  if (![confidence hasBorderOfAtLeast:radius]) {
    NSLog(@"Contour:getConfidence: confidence needs border of at least radius");
  }
  
  if (results->size() != points->size())
    results->resize(points->size());
  
  // Compute space offsets
  const int diam = 2*radius+1;
  vector<int> space_ofs (diam*diam);
  int space_sz = 0;
  
  for (int i = -radius; i <= radius; ++i) {
    for (int j = -radius; j <= radius; ++j) {
      space_ofs[space_sz++] = i*confidence.width_step + j*sizeof(float);
    }
  }
  
  float val;
  float* sptr;
  float weight = 1.0 / (float)(diam*diam);
  int idx = 0;
  
  for (vector<FXPoint>::const_iterator p = points->begin(); p != points->end(); ++p, ++idx) {
    sptr = [confidence mutableValueAtX:p->x andY:p->y];
    val = 0;
    for (int k = 0; k < space_sz; ++k) {
      val += *PtrOffset(sptr, space_ofs[k]);
    }
    (*results)[idx] = val * weight;
  }
}

+(void) getValidPositionsFromImage:(FXImage*)image 
                           andMask:(FXImage*)mask 
                        maskOffset:(FXPoint)offset
                       patchRadius:(int)radius
                     outerPosition:(vector<FXRect>*)outer
                     innerPositions:(vector<const uchar*>*)inner {
  const int img_width = image.roi.width;
  const int img_height = image.roi.height;
  
  if (![mask hasBorderOfAtLeast:radius]) {
    NSLog(@"Contour:getValidPositionsFromImage: mask must have border of at least radius.");
    return;
  }
  
  FXRect mask_roi = mask.roi;
  mask.roi = FXRect(mask_roi.x - radius, mask_roi.y - radius,
                    mask_roi.width + 2*radius, mask_roi.height + 2*radius);
  offset.x -= radius;
  offset.y -= radius;
  
  const int mask_width = mask.roi.width;
  const int mask_height = mask.roi.height;
  
  outer->clear();
  // Process outer positions
  // Rect above.
  if (offset.y > radius) {
    outer->push_back(FXRect(radius, 
                            radius, 
                            img_width - 2*radius, 
                            std::min(offset.y - radius, img_height - 2*radius)));
  }
  
  // Rect below.
  if (offset.y + mask_height < img_height - 2*radius) {
    // Don't start before radius.
    int start_y = std::max(offset.y + mask_height, radius);
    outer->push_back(FXRect(radius, 
                            start_y,
                            img_width - 2*radius,
                            img_height - radius - start_y));
  }
  
  int start_x = radius;
  int end_x = img_width - radius - 1;
  
  // Rect on left hand side.
  if (offset.x - radius > 10) {
    // Don't start before radius
    int start_y = std::max(offset.y, radius);
    outer->push_back(FXRect(radius,
                            start_y,
                            std::min(offset.x - radius, img_width - 2*radius), 
                            // Height if started at start_y,
                            // but not beyond lower border.
                            std::min(mask_height+offset.y-start_y,
                                     img_height-radius-start_y)));
    // Might be zero because of min.
    if (outer->back().height <= 0)
      outer->pop_back();
    else
      start_x = offset.x;
  }
  
  int inner_rhs_width = img_width - radius - offset.x - mask_width; 
  if (inner_rhs_width > 10) {
    int start_y = std::max(offset.y, radius);
    int start_x = std::max(offset.x + mask_width, radius);
    
    outer->push_back(FXRect(start_x, 
                            start_y, 
                            img_width - radius - start_x, 
                            // Same as above.
                            std::min(mask_height+offset.y-start_y,
                                     img_height-radius-start_y)));
    if (outer->back().height <= 0)
      outer->pop_back();
    else      
      end_x = offset.x + mask_width - 1;
  }
  
  if (start_x < radius || end_x >= img_width - radius) {
    NSLog(@"Contour:getValidPositionsFromImage: logic error in start_x or end_x");
    return;
  }
  
  // Compute space offsets
  const int diam = 2*radius+1;
  vector<int> space_ofs (diam*diam);
  int space_sz = 0;
  
  for (int i = -radius; i <= radius; ++i) {
    for (int j = -radius; j <= radius; ++j) {
      space_ofs[space_sz++] = i*mask.width_step + j;
    }
  }
  
  const int start_y = std::max(radius, offset.y);
  const int len_y = std::min(offset.y + mask_height - start_y,
                             img_height - radius - start_y);
  
  const uchar* mask_ptr = [mask mutableValueAtX:std::max(0,start_x-offset.x) 
                                           andY:start_y-offset.y];
  const uchar* img_ptr = [image mutableValueAtX:start_x andY:start_y];
  
  const int mask_pad = mask.width_step - 
      (std::min(offset.x + mask_width, img_width - radius ) -  // right end of mask
       std::max(offset.x, radius));                            // left start of mask

  if (mask_pad < 0 || mask_pad > mask.width_step) {
    NSLog(@"Contour:getValidPositionsFromImage: logic error in mask_pas");
    return;
  }
  
  const int img_pad = image.width_step - (end_x-start_x+1)*image.channels;
  inner->clear();
  inner->reserve(mask_width*mask_height/4);
    
  for (int i = 0; i < len_y; ++i, mask_ptr += mask_pad, img_ptr += img_pad) {
    int j = start_x;
    
    // Left of mask append
    for (int tmp_end = std::min(offset.x, end_x+1);
         j < tmp_end;
         ++j, img_ptr += image.channels)
        inner->push_back(img_ptr);
    
    for (int tmp_end = std::min(offset.x + mask_width, end_x+1); 
         j < tmp_end; ++j, ++mask_ptr, img_ptr+=image.channels) {
      int sum = 0;
      for (int k = 0; k < space_sz; ++k) {
        sum += (int) *(mask_ptr + space_ofs[k]);
      }
      if (sum == 0) {
        inner->push_back(img_ptr);
      }
    }
    
    // Right of mask append
    for (; j <= end_x; ++j, img_ptr+=image.channels)
      inner->push_back(img_ptr);
  }
  
  mask.roi = mask_roi;
}

+(void) computePatchInformationFromImage:(FXImage*)image
                                 andMask:(FXImage*)mask
                              maskOffset:(FXPoint)offset
                             patchCenter:(FXPoint)center
                             patchRadius:(int)radius
                            spaceOffsets:(vector<int>*)space_ofs
                             colorValues:(vector<uint>*)color_values {
  uchar* mask_ptr = [mask mutableValueAtX:center.x andY:center.y];
  uchar* src_ptr = [image mutableValueAtX:center.x+offset.x andY:center.y+offset.y];
  
  space_ofs->clear();
  color_values->clear();
  space_ofs->reserve(2*radius + 1);
  color_values->reserve(2*radius + 1);
  
  FXRect img_rect = image.roi.shiftOffset(-image.roi.x, -image.roi.y);

  for (int i = -radius; i <= radius; ++i) {
    for (int j = -radius; j <= radius; ++j) {
      if (img_rect.contains(FXPoint(center.x + offset.x + j, center.y + offset.y + i)) &&
          *(mask_ptr + i*mask.width_step + j) == 0) {   // valid location
        space_ofs->push_back(i*image.width_step + j*image.channels);
        color_values->push_back(*reinterpret_cast<uint*>(src_ptr + space_ofs->back()));
      }
    }
  }
}

+(FXPoint) findBestMatchInImage:(FXImage*)image
                        inRects:(vector<FXRect>*)rects
                    atPositions:(vector<const uchar*>*)positions
                   spaceOffsets:(vector<int>*)space_ofs
                    patchValues:(vector<uint>*)color_values {

  int val_r, val_g, val_b, val, sum;
  int min_sum = std::numeric_limits<int>::max();
  const uchar* ptr;
  FXPoint min_loc;
  const int space_sz = space_ofs->size();

  // Process rects first.
  for (vector<FXRect>::const_iterator r = rects->begin(); r != rects->end(); ++r) {
    uchar* src_ptr = [image mutableValueAtX:r->x andY:r->y];
    const int src_pad = image.width_step - r->width*image.channels;
    const int img_ch = image.channels;
    for (int i = r->y, end_y = r->y + r->height; i < end_y; ++i, src_ptr += src_pad) {
      for (int j = r->x, end_x = r->x + r->width; j < end_x; ++j, src_ptr += img_ch) {
        sum = 0;
        int* space_ofs_ptr = &(*space_ofs)[0];
        uint* color_val_ptr = &(*color_values)[0];        
        
        for (int k = 0; k < space_sz; ++k, ++space_ofs_ptr, ++color_val_ptr) {
          ptr = src_ptr + *space_ofs_ptr;
          val = *color_val_ptr;
          
          val_r = (int)*ptr++ - (int)((val >> 0) & 0xff);
          val_g = (int)*ptr++ - (int)((val >> 8)  & 0xff);
          val_b = (int)*ptr   - (int)((val >> 16) & 0xff);
          sum += val_r*val_r + val_g*val_g + val_b*val_b;
        }
        if (sum < min_sum) {
          min_sum = sum;
          min_loc = FXPoint (j,i);
        }
      }
    }
  }
  
  // Process inner postions
  int min_sum_inner = std::numeric_limits<int>::max();
  const uchar* min_loc_inner;
  
  for (vector<const uchar*>::const_iterator i = positions->begin(); i != positions->end(); ++i) {
    sum = 0;
    const uchar* const src_ptr = *i;
    int* space_ofs_ptr = &(*space_ofs)[0];
    uint* color_val_ptr = &(*color_values)[0];
    
    for (int k = 0; k < space_sz; ++k, ++space_ofs_ptr, ++color_val_ptr) {
      ptr = src_ptr + *space_ofs_ptr;
      val = *color_val_ptr;
      
      val_r = (int)*ptr++ - (int)((val >> 0) & 0xff);
      val_g = (int)*ptr++ - (int)((val >> 8)  & 0xff);
      val_b = (int)*ptr   - (int)((val >> 16) & 0xff);
      sum += val_r*val_r + val_g*val_g + val_b*val_b;
    }
    
    if (sum < min_sum_inner) {
      min_sum_inner = sum;
      min_loc_inner = *i;
    }
  }
  
  if (min_sum_inner < min_sum) {
    int off = min_loc_inner - image.roi_data;
    return FXPoint((off % image.width_step) / image.channels,
                   off / image.width_step);
  } else {
    return min_loc;
  }
    
}


@end
