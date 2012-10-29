/*
 *  video_display_unit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/25/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_display_unit.h"

#include <opencv2/highgui/highgui_c.h>
#include <sstream>
#include <iostream>

#include "assert_log.h"
#include "image_util.h"
#include "video_pipeline.h"

namespace VideoFramework {

int VideoDisplayUnit::display_unit_count;

VideoDisplayUnit::VideoDisplayUnit(const string& stream_name,
                                   OpenCVArbiter* arbiter)
    : stream_name_(stream_name), cv_arbiter_(arbiter) {
  display_unit_id_ = display_unit_count++;
}

bool VideoDisplayUnit::OpenStreams(StreamSet* set) {
  // Find video stream idx.
  video_stream_idx_ = FindStreamIdx(stream_name_, set);

  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  // Opend display window.
  std::ostringstream os;
  os << "VideoDisplayUnit_" << display_unit_id_;
  window_name_ = os.str();

  cvNamedWindow(window_name_.c_str());
  cvWaitKey(1);
  return true;
}

void VideoDisplayUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  static int count = 0;
  ++count;

  const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  IplImage image;
  frame->ImageView(&image);

  if (cv_arbiter_) {
    cv_arbiter_->ShowImage(window_name_.c_str(), &image);
  } else {
    cvShowImage(window_name_.c_str(), &image);
    cvWaitKey(1);
  }

  output->push_back(input);
}

void VideoDisplayUnit::PostProcess(list<FrameSetPtr>* append) {
  cvDestroyWindow(window_name_.c_str());
}

}
