/*
 *  buffered_image.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/17/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "buffered_image.h"

BufferedImage::BufferedImage(int width, int height, int width_step) :
  width_(width), height_(height), width_step_(width_step) {
  front_buffer_ = new uchar[height * width_step];
  back_buffer_ = new uchar[height * width_step];
  memset(front_buffer_, 0, height * width_step);
  memset(back_buffer_, 0, height * width_step);
}

BufferedImage::~BufferedImage() {
  delete [] front_buffer_;
  delete [] back_buffer_;
}

shared_ptr<QImage> BufferedImage::NewFrontView() const {
  int border = 0;
  shared_ptr<QImage> img(new QImage(front_buffer_,
                                    width_-border,
                                    height_-border,
                                    width_step_,
                                    QImage::Format_RGB888));
  return img;
}

void BufferedImage::Swap() {
  mutex_.lock();
  std::swap(front_buffer_, back_buffer_);
  mutex_.unlock();
}

