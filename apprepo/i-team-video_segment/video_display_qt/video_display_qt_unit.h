/*
 *  video_display_unit.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/25/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_DISPLAY_QT_UNIT_H__
#define VIDEO_DISPLAY_QT_UNIT_H__

#include "video_unit.h"

class QApplication;
class MainWindow;

namespace VideoFramework {

class VideoDisplayQtUnit : public VideoUnit {

public:
  VideoDisplayQtUnit(const string& stream_name = "VideoStream");
  ~VideoDisplayQtUnit();

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

private:
  int video_stream_idx_;
  int display_unit_id_;
  string stream_name_;

  int frame_width_;
  int frame_height_;

  // Used for automatic arangement of windows.
  static int summed_window_width_;

  // Memory of QApplication is not freed on return;
  static QApplication* app_;
  shared_ptr<MainWindow> main_window_;
};

}  // namespace.

#endif  // VIDEO_DISPLAY_QT_UNIT_H__
