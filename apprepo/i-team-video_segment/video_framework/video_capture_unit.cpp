/*
 *  video_capture_unit.cpp
 *  make-test
 *
 *  Created by Matthias Grundmann on 9/16/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_capture_unit.h"

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "assert_log.h"
#include "video_pipeline.h"

namespace VideoFramework {

VideoCaptureUnit::VideoCaptureUnit(const string& video_stream_name,
                                   float downscale_factor,
                                   float fps,
                                   OpenCVArbiter* arbiter)
    : video_stream_name_(video_stream_name),
      downscale_(downscale_factor),
      fps_(fps),
      cv_arbiter_(arbiter) {
  capture_ = cvCaptureFromCAM(0);
}

VideoCaptureUnit::~VideoCaptureUnit() {
  // Crashes for some reason. Commented out.
  // cvReleaseCapture(&capture_);
}

bool VideoCaptureUnit::OpenStreams(StreamSet* set) {
  // Get capture properties.
  frame_width_ = cvGetCaptureProperty(capture_, CV_CAP_PROP_FRAME_WIDTH) / downscale_;
  frame_height_ = cvGetCaptureProperty(capture_, CV_CAP_PROP_FRAME_HEIGHT) / downscale_;

  LOG(INFO) << "Capturing from Camera frames of size "
            << frame_width_ << "x" << frame_height_;

  frame_width_step_ = frame_width_ * 3;
  if (frame_width_step_ % 4) {
    frame_width_step_ += (4 - frame_width_step_ % 4);
  }

  VideoStream* vid_stream = new VideoStream(frame_width_,
                                            frame_height_,
                                            frame_width_step_,
                                            fps_,
                                            0,
                                            PIXEL_FORMAT_BGR24,
                                            video_stream_name_);

  set->push_back(shared_ptr<VideoStream>(vid_stream));

  frame_count_ = 0;
  return true;
}

void VideoCaptureUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  // This is a source, ProcessFrame is never called.
}

void VideoCaptureUnit::PostProcess(list<FrameSetPtr>* append) {
  // Measure elapsed time.
  // All times are in msesc.
  ptime curr_time = boost::posix_time::microsec_clock::local_time();

  if (frame_count_ > 0) {
    // const float target_frame_duration = 1.0f / fps_ * 1e3;
    // const float elapsed_time =
    //     boost::posix_time::time_period(prev_capture_time_,
    //                                    curr_time).length().total_microseconds() * 1e-3;
    // // If not on target fps, wait a bit.
    // int wait_time = (int) ((target_frame_duration - elapsed_time) * 1000);
    // if (wait_time > 0) {
    //   boost::thread::sleep(boost::get_system_time() +
    //                        boost::posix_time::microseconds(wait_time));
    // } else {
    // }

    if (cv_arbiter_) {
      if (cv_arbiter_->GetKey() == 27) {
        LOG(INFO) << "ESC pressed. Abort!";
        return;
      }
    } else {
      if (cvWaitKey(1) == 27) {
        LOG(INFO) << "ESC pressed. Abort!";
        return;
      }
    }
  }

  // Get frame from camera and output.
  FrameSetPtr frame_set(new FrameSet());
  curr_time = boost::posix_time::microsec_clock::local_time();

  VideoFrame* curr_frame = new VideoFrame(frame_width_, frame_height_, 3,
                                          frame_width_step_);

  const IplImage* retrieved_frame;
  if (cv_arbiter_) {
    retrieved_frame = cv_arbiter_->QueryFrame(capture_);
  } else {
    retrieved_frame = cvQueryFrame(capture_);
  }
  IplImage frame_view;
  curr_frame->ImageView(&frame_view);

  cvResize(retrieved_frame, &frame_view);

  frame_set->push_back(shared_ptr<VideoFrame>(curr_frame));
  append->push_back(frame_set);

  prev_capture_time_ = curr_time;
  ++frame_count_;
}

float VideoCaptureUnit::AdjustRateImpl(float fps) {
  // Low pass filtering.
  fps_ = 0.3 * fps_ + 0.7 * fps;
  std::cerr << "\n\nADJSUT RATE : " << fps << "   |   " << fps_ << "\n\n";
  return fps_;
}

}  // namespace VideoFramework.
