/*
 *  buffered_image.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/17/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef BUFFERED_IMAGE_H__
#define BUFFERED_IMAGE_H__

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <QMutex>
#include <QImage>

class BufferedImage {
public:
  BufferedImage(int width, int height, int width_step);
  ~BufferedImage();
  
  uchar* get_front() const {return front_buffer_;}
  uchar* get_back() const {return back_buffer_;}
  
  int get_width() const { return width_; }
  int get_height() const { return height_; }
  int get_width_step() const { return width_step_; }
  
  shared_ptr<QImage> NewFrontView() const;
  
  void Swap();
  void Lock() { mutex_.lock(); }
  void Unlock() { mutex_.unlock(); }
  bool TryLock(int wait_time_msecs = 0) { return mutex_.tryLock(wait_time_msecs); }
  
protected:
  int width_;
  int height_;
  int width_step_;
  
  uchar* front_buffer_;
  uchar* back_buffer_;
  
  QMutex mutex_;
};

#endif // BUFFERED_IMAGE_H__