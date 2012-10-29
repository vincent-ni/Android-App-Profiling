/*
 *  video_display_unit.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/25/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_DISPLAY_UNIT_H__
#define VIDEO_DISPLAY_UNIT_H__

#include "video_unit.h"

namespace VideoFramework {

class OpenCVArbiter;
  
class VideoDisplayUnit : public VideoUnit {
public:
  VideoDisplayUnit(const string& stream_name = "VideoStream",
                   OpenCVArbiter* arbiter = 0);
  ~VideoDisplayUnit() {}
  
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);
private:
  int video_stream_idx_;
  int display_unit_id_;
  string stream_name_;
  
  string window_name_;
  shared_ptr<IplImage> frame_buffer_;
  
  OpenCVArbiter* cv_arbiter_;
  
  static int display_unit_count;
};
  
}

#endif