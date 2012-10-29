/*
 *  MainWindow.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/2/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef MAIN_WINDOW_H__
#define MAIN_WINDOW_H__

#include <vector>
using std::vector;

#include <string>
using std::string;
#include <utility>
using std::pair;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <QMainWindow>
#include <QImage>
#include <QPushButton>
#include <QTime>
#include <QProgressDialog>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSlider>

#include "video_reader_unit.h"
using VideoFramework::VideoReaderUnit;

#include "video_analyzer.h"
#include "compositing_widget.h"
#include "play_unit.h"
#include "canvas_cut.h"

namespace  VideoFramework {
  class OpticalFlowUnit;
  class RegionFlowUnit;
  class LuminanceUnit;
  class FlipBGRUnit;
}

namespace Segment {
  class SegmentationReaderUnit;
  class SegmentationRenderUnit;
  class SegmentationUnit;
  class SegmentationWriterUnit;
}

class ForegroundTracker;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow (QWidget* parent =0);
  ~MainWindow ();
  
public slots:
  void LoadVideo();
  void AnalyzeVideo();
  void PlayVideo();
  void StepVideo();
  void StopVideo();
  void TraceToggled(int);
  void SegLevelChanged(int);
  void FrameSpacingChanged(int);
  void FrameSpacingRatioChanged(int);
  void TouchUp();
  
  void PlayEnded();
  void CancelPlayEnded();
  
  void AnalyzeProgress(int);
  void DrawCanvasStateChanged(bool);
  void SaveProject();
  void LoadProject();
  void HideCanvas();
  
  void CanvasBrush();
  void Stitch();
  
protected:
  void LoadVideoImpl(const QString& video_file);
  void InitSegmentationSlider();
  void keyPressEvent(QKeyEvent * event);
  
protected:
  CompositingWidget* composit_;
  QPushButton* play_button_;
  QPushButton* analyse_button_;
  QPushButton* stop_button_;
  QPushButton* step_button_;
  QButtonGroup* transform_mode_;
  QCheckBox* trace_canvas_;
  QSlider* seg_level_slider_;
  QLabel* seg_level_label_;
  
  QPushButton* touch_up_;
  QPushButton* stitch_;
  QPushButton* canvas_brush_;
  QPushButton* canvas_clean_;
  
  QButtonGroup* selection_mode_;
  QButtonGroup* scrub_mode_;
  
  QPushButton* save_project_;
  QPushButton* hide_canvas_;
  QPushButton* load_project_;
  
  QLabel* frame_spacing_label_;
  QSlider* frame_spacing_slider_;
  
  QLabel* frame_spacing_ratio_label_;
  QSlider* frame_spacing_ratio_slider_;
  
  QProgressDialog* analyze_pd_;
  QTime analyze_time_;
  
  QString video_file_;
  shared_ptr<VideoReaderUnit> video_reader_;
  shared_ptr<PlayUnit> play_unit_;
  
  shared_ptr<VideoFramework::LuminanceUnit> lum_unit_;
  shared_ptr<VideoFramework::OpticalFlowUnit> flow_unit_;
  shared_ptr<VideoFramework::FlipBGRUnit> flip_bgr_unit_;
  shared_ptr<Segment::SegmentationReaderUnit> seg_reader_unit_;
  shared_ptr<Segment::SegmentationRenderUnit> seg_render_unit_;
  shared_ptr<Segment::SegmentationWriterUnit> seg_writer_unit_;
  shared_ptr<Segment::SegmentationUnit> seg_unit_;

  shared_ptr<VideoFramework::RegionFlowUnit> region_flow_unit_;
  shared_ptr<ForegroundTracker> foreground_tracker_;
  
  shared_ptr<VideoAnalyzer> video_analyzer_;
  vector<FrameInfo> frame_infos_;
  
  bool video_restart_;

};
  

#endif  // MAIN_WINDOW_H__