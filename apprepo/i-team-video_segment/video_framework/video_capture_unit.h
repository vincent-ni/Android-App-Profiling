/*
 *  video_capture_unit.h
 *  make-test
 *
 *  Created by Matthias Grundmann on 9/16/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_CAPTURE_UNIT_H__
#define VIDEO_CAPTURE_UNIT_H__

#include "video_unit.h"

class CvCapture;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

namespace VideoFramework {
  
class OpenCVArbiter;

class VideoCaptureUnit : public VideoUnit {
public:
  // If arbiter is passed cvQueryFrame calls will be handled through arbiter.
  VideoCaptureUnit(const string& video_stream_name = "VideoStream",
                   float down_scale_factor = 4,
                   float fps = 5,
                   OpenCVArbiter* cv_arbiter = 0);
  
  ~VideoCaptureUnit();
  
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);
  
protected:
  float AdjustRateImpl(float fps);

private:
  string video_stream_name_;    
  float downscale_;
  
  CvCapture* capture_;
  int frame_width_;
  int frame_height_;
  float fps_;
  int frame_width_step_;
  
  int frame_count_;
  
  // Used to capture w.r.t. specified fps.
  ptime prev_capture_time_;
  
  OpenCVArbiter* cv_arbiter_;
};
  
}

#endif