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

#include "common.h"

#include <QMainWindow>
#include <QtGui>
#include <QObject>

#include "video_reader_unit.h"
#include "video_unit.h"
#include "optical_flow_unit.h"
#include "conversion_units.h"
#include "buffered_image.h"

namespace VideoFramework {
  class OpticalFlowUnit;
  class LuminanceUnit;
  class VideoReaderUnit;
  class VideoUnit;
}

class VideoWidget;

using VideoFramework::FrameSetPtr;
using VideoFramework::StreamSet;

class PlayUnit : public QObject, public VideoFramework::VideoUnit {
  Q_OBJECT
public:
  PlayUnit(const string& vid_stream_name,
           const string& flow_stream_name,
           const string& polygon_stream_name,
           VideoWidget* widget);

  void ResetRect();
  void UpdateOutline(const vector<QPointF>& outline) { outlines_[frame_number_ - 1] = outline; }
  void TogglePlay();
  void EnableRetrack(bool b) { retrack_ = b;  std::cerr << "Retrack: " << b; }

  const vector<vector<QPointF > >& GetOutlines() const { return outlines_; }

public slots:
  void PlayLoop();

signals:
  void PlayingEnded();

protected:
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

  virtual bool SeekImpl(int64_t);

protected:
  string vid_stream_name_;
  string flow_stream_name_;
  string polygon_stream_name_;

  int vid_stream_idx_;
  int flow_stream_idx_;
  bool retrack_;

  int frame_width_;
  int frame_height_;
  int frame_step_;
  int frame_number_;

  vector< vector<QPointF> > outlines_;
  vector<int64_t> frame_map_;

  VideoWidget* widget_;
  shared_ptr<QTimer> play_timer_;
};

class ExportUnit: public QObject, public VideoFramework::VideoUnit {
  Q_OBJECT
public:
  ExportUnit(const string& poly_file,
             const string& export_file,
             const string& vid_stream_name_,
             const vector<vector< QPointF> >& outlines,
             VideoWidget* widget);

  ~ExportUnit();

  void SetCheckerConstraints(int num_row, int num_col, int sz, float space_x, float space_y);
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);

protected:
  string vid_stream_name_;
  int vid_stream_idx_;

  int frame_width_;
  int frame_height_;
  int frame_step_;

  int frame_number_;
  vector< vector<QPointF> > outlines_;
  VideoWidget* widget_;

  int checker_row_;
  int checker_col_;
  int checker_size_;
  float space_x_;
  float space_y_;
  int lines_;

  shared_ptr<IplImage> mask_img_;

  std::ofstream ofs_poly_;
  std::ofstream ofs_checker_;
};

class VideoWidget : public QWidget {
  Q_OBJECT
public:
  VideoWidget(QWidget* parent = 0);
  ~VideoWidget() {}

  void SetPlayUnit(PlayUnit* play_unit) { play_unit_ = play_unit; }

  void SetFrameSize(int width, int height, int width_step);
  uchar* BackBuffer() const { return buffered_image_->get_back(); }
  void SwapBuffer() { buffered_image_->Swap(); }

  void SetOutline(const vector<QPointF>& outline);
  const vector<QPointF>& GetOutline() const { return outline_; }

  int get_checker_row() const { return checker_row_; }
  int get_checker_col() const { return checker_col_; }
  int get_checker_size() const { return checker_size_; }
  float get_space_x() const { return space_x_; }
  float get_space_y() const { return space_y_; }


  void set_checker_row(int row) { checker_row_ = row; }
  void set_checker_col(int col) { checker_col_ = col; }
  void set_checker_size(int sz) { checker_size_ = sz; }
  void set_space_x(float space_x) { space_x_ = space_x; }
  void set_space_y(float space_y) { space_y_ = space_y; }

public slots:
  void CheckerRowNum(int);
  void CheckerColNum(int);
  void CheckerSize(int);
  void CheckerSpaceX(int);
  void CheckerSpaceY(int);

protected:
  void paintEvent(QPaintEvent*);
  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  void resizeEvent(QResizeEvent* event);

  void Rescale();
//  void wheelEvent(QWheelEvent* event);

protected:
  int frame_width_;
  int frame_height_;

  int checker_row_;
  int checker_col_;
  int checker_size_;
  float space_x_;
  float space_y_;

  float scale;
  QPoint offset;
  int pt_selected_;

  shared_ptr<BufferedImage> buffered_image_;
  vector<QPointF> outline_;
  PlayUnit* play_unit_;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow (const string& video_file,
              const string& config_file,
              QWidget* parent =0);
  ~MainWindow ();

public slots:
  void LoadVideo();
  void StepVideo();
  void ResetRect();
  void Retrack(int);
  void ExportResult();
  void SaveInitialization();

signals:
  void ExportingDone();

protected:
  void keyPressEvent(QKeyEvent* event);
  void LoadVideoImpl(const string& video_file);
  void LoadInitialization(const string& file);

protected:
  VideoWidget* video_widget_;

  string video_file_;
  QCheckBox* retrack_;

  shared_ptr<VideoFramework::VideoReaderUnit> video_reader_;
  shared_ptr<VideoFramework::LuminanceUnit> lum_unit_;
  shared_ptr<VideoFramework::OpticalFlowUnit> flow_unit_;

  shared_ptr<PlayUnit> play_unit_;
};


#endif  // MAIN_WINDOW_H__
