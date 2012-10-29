/*
 *  video_display_unit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/25/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_display_qt_unit.h"

#include <sstream>
#include <iostream>

#include "assert_log.h"

#include <QtGui/QApplication>
#include <QtGui/QMainWindow>
#include "main_window.h"

namespace VideoFramework {

QApplication* VideoDisplayQtUnit::app_ = NULL;
int VideoDisplayQtUnit::summed_window_width_ = 50;

VideoDisplayQtUnit::VideoDisplayQtUnit(const string& stream_name)
  : stream_name_(stream_name) {

  int argc = 1;
  char* argv[] = {""};

  if (app_ == NULL) {
    app_ = new QApplication(argc, argv);
  }
  main_window_.reset(new MainWindow(stream_name));
}

VideoDisplayQtUnit::~VideoDisplayQtUnit() {
  if (app_ != NULL) {
    app_->exit();
  }
}

bool VideoDisplayQtUnit::OpenStreams(StreamSet* set) {
  // Find video stream idx.
  video_stream_idx_ = FindStreamIdx(stream_name_, set);

  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  // Get video stream.
  const VideoStream* vid_stream =
      dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();

  main_window_->SetSize(frame_width_, frame_height_);
  main_window_->move(summed_window_width_, 0);
  main_window_->show();
  summed_window_width_ += frame_width_ + 10;  // 10 pix. border

  QApplication::processEvents();
  return true;
}

void VideoDisplayQtUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  const VideoFrame* frame =
      dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  QImage img((const uint8_t*)frame->data(),
             frame->width(),
             frame->height(),
             frame->width_step(),
             QImage::Format_RGB888);

  main_window_->DrawImage(img.rgbSwapped());
  QApplication::processEvents();

  output->push_back(input);
}

void VideoDisplayQtUnit::PostProcess(list<FrameSetPtr>* append) {
}

}
