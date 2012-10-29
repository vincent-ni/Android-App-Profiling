/*
 *  Util.h
 *  visualFX
 *
 *  Created by Matthias Grundmann on 8/22/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef UTIL_H_
#define UTIL_H_

template <class T>
inline const T* PtrOffset(const T* t, unsigned int offset) {
  return reinterpret_cast<const T*>(reinterpret_cast<const unsigned char*>(t) + offset);
}

template <class T>
inline T* PtrOffset(T* t, unsigned int offset) {
  return reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(t) + offset);
}

struct FXPoint {
  FXPoint() : x(0), y(0) {};
  FXPoint(int _x, int _y) : x(_x), y(_y) {};
  int x;
  int y;
};


struct FXRect {
  FXRect (int _x, int _y, int _width, int _height) : x(_x), y(_y), width(_width), height(_height) {}
  bool isEqualSize(const FXRect& rhs) const { return width == rhs.width && height == rhs.height; }
  FXPoint offset() const { return FXPoint(x,y); }
  FXRect shiftOffset(int _x, int _y) const { return FXRect(x + _x, y + _y, width, height); }
  FXRect shiftOffset(FXPoint offset) const { return shiftOffset(offset.x, offset.y); }
  bool contains(FXPoint p) const { return (p.x >= x &&
                                           p.y >= y &&
                                           p.x < x + width &&
                                           p.y < y + height); }
  int x;
  int y;
  int width;
  int height;
};

struct CGPoint;

CGPoint CGPointAdd(CGPoint a, CGPoint b);
CGPoint CGPointSub(CGPoint a, CGPoint b);
CGPoint CGPointScale(CGPoint a, float s);
CGPoint CGPointRotate(CGPoint a, float r);
CGPoint CGPointAbs(CGPoint a);
CGPoint CGPointNormalize(CGPoint a);
float   CGPointDot(CGPoint a, CGPoint b);
float   CGPointNorm(CGPoint a);

CGPoint CGPointMax(CGPoint a, CGPoint b);
CGPoint CGPointMin(CGPoint a, CGPoint b);
CGPoint CGPointRound(CGPoint a);

enum BorderMode {BORDER_BOTH, BORDER_HORIZ, BORDER_VERT};

#endif // UTIL_H_