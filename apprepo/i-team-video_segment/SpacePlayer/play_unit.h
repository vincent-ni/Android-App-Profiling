/*
 *  play_unit.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/17/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef PLAY_UNIT_H__
#define PLAY_UNIT_H__

#include <QThread>
#include <QTimer>
#include <QTime>
#include <QObject>

#include <video_unit.h>

#include <list>
using std::list;

#include <vector>
using std::vector;

#include <boost/circular_buffer.hpp>
#include "image_util.h"

#include "video_analyzer.h"

using VideoFramework::FrameSetPtr;
using VideoFramework::StreamSet;

class CompositingWidget;

namespace Segment {
  class SegmentationDesc;
}

namespace VideoFramework {
  class OpticalFlowFrame;
  class RegionFlowFrame;
}

class PlayUnit : public QObject, public VideoFramework::VideoUnit {
  Q_OBJECT
public:
  PlayUnit(CompositingWidget* output, VideoUnit* root, bool show_first_frame,
           const QString& video_file, const string& video_stream_name = "VideoStream");
  ~PlayUnit();
  
  void Play();
  void Pause();
  void set_draw_canvas(bool b) {draw_canvas_ = b;}
  
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual bool SeekImpl(int64_t pts);
  
  int get_frame_guess() const { return frame_guess_; }
  int get_frame_width() const { return frame_width_; }
  int get_frame_height() const { return frame_height_; }
  
  void PassFrameInfos(vector<FrameInfo>* infos, QPointF canvas_offset);
  void SetCanvasAlpha(int alpha);
  
  void SaveCanvasFrameOrigin(const string& file) const;
  void LoadCanvasFrameOrigin(const string& file);
  
public slots:
  void PlayLoop();
  void SetFrameFromPt(QPoint);
  void RegionFromPt(QPoint);
  void SegLevelChanged(int);
  void RegionSelectionMode(int);
  void Stitch();
  void ScrubMode(int);
  void CanvasClean();
  
signals:
  void Done();
  void CancelDone();
  void DrawCanvasState(bool);
  
protected:
  void ResetSelection();
  void DrawOpticalFlow(const VideoFramework::RegionFlowFrame* frame,
                       const vector<int>& inlier_regions);
  void DrawOpticalFlow(const VideoFramework::OpticalFlowFrame* frame);
  
protected:
  CompositingWidget* output_;
  string video_stream_name_;
  
  QString video_file_;
  QTimer* play_timer_;
  QTime start_time_;
  int frames_displayed_;
  bool draw_canvas_;
  bool is_caching_frames_;
  
  int video_stream_idx_;
  int flow_stream_idx_;
  int oflow_stream_idx_;
  int inlier_stream_idx_;
  
  int frame_width_;
  int frame_height_;
  float fps_;
  int frame_guess_;
  
  int frames_played_;
  
  vector<FrameInfo>* frame_infos_;
  
  // Holds frame center and top left corner relative to canvas.
  vector<QPoint> frame_center_;
  vector<QPoint> frame_top_left_;
  QPoint canvas_offset_;
  
  std::ifstream seg_ifs_;
  int seg_level_;
  vector<int64_t> seg_frame_offset_;
  vector<int> selected_frame_regions_;
  vector<int> selected_canvas_regions_;
  int selection_mode_;
  int scrub_mode_;
  
  boost::circular_buffer<shared_ptr<IplImage> > cached_images_;
  
  // Hold for each pixel which frame was used.
  shared_array<int> canvas_frame_origin_;
  
  shared_ptr<QImage> selection_mask_;
  shared_ptr<Segment::SegmentationDesc> cur_segmentation_;
  shared_ptr<CanvasCut> canvas_cut_;
};

#endif // PLAY_UNIT_H__