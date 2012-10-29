/*
 *  Util.cpp
 *  visualFX
 *
 *  Created by Matthias Grundmann on 8/22/08.
 *  Copyright 2008 Georgia Institute of Technology. All rights reserved.
 *
 */

#include "Util.h"
#include "CoreGraphics/CGGeometry.h"
#include <cmath>
#include <algorithm>

CGPoint CGPointAdd(CGPoint a, CGPoint b) {
  CGPoint res = {a.x + b.x, a.y + b.y}; 
  return res;
}

CGPoint CGPointSub(CGPoint a, CGPoint b) {
  CGPoint res = {a.x - b.x, a.y - b.y}; 
  return res; 
}

CGPoint CGPointScale(CGPoint a, float s) {
  CGPoint res = {a.x*s, a.y*s};
  return res;
}

CGPoint CGPointRotate(CGPoint a, float r) {
  float c = std::cos(r);
  float s = std::sin(r);
  CGPoint res = {c*a.x - s*a.y, s*a.x + c*a.y};
  return res;
}

CGPoint CGPointAbs(CGPoint a) {
  CGPoint res = {fabs(a.x), fabs(a.y)};
  return res;
}

CGPoint CGPointNormalize(CGPoint a) {
  float norm = 1.0/sqrt(a.x*a.x + a.y*a.y);
  CGPoint res = {a.x*norm, a.y*norm};
  return res;
}

float CGPointDot(CGPoint a, CGPoint b) {
  return a.x*b.x + a.y*b.y;
}

float CGPointNorm(CGPoint a) {
  return sqrt(a.x*a.x + a.y*a.y);
}

CGPoint CGPointMax(CGPoint a, CGPoint b) {
  CGPoint res = {std::max(a.x, b.x), std::max(a.y, b.y)};
  return res;
}

CGPoint CGPointMin(CGPoint a, CGPoint b) {
  CGPoint res = {std::min(a.x, b.x), std::min(a.y, b.y)};
  return res;
}

CGPoint CGPointRound(CGPoint a) {
  CGPoint res = { floor(a.x + 0.5), floor(a.y+0.5) };
  return res;
}