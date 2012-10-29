/*
 *  compositing_widget.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/3/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef COMPOSITING_WIDGET_H__
#define COMPOSITING_WIDGET_H__

#include <vector>
using std::vector;

#include <string>
using std::string;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <QWidget>
#include <QTransform>
#include <QPoint>
#include "buffered_image.h"

class CompositingWidget : public QWidget {
  Q_OBJECT
public:
  CompositingWidget(QWidget* parent =0);
    
  void SetBufferedImage(BufferedImage* img);
  BufferedImage* GetBufferedImage() const { return cur_img_.get(); }
  void DrawBufferedImage(bool draw) { draw_cur_image_ = draw; }
  bool IsDrawBufferedImage() const { return draw_cur_image_; }
  void SetSelectionMask(QImage* mask);
  
  void SetCanvas(QImage* canvas, QPointF canvas_offset);
  QImage* get_canvas() const { return canvas_.get(); }
  QPointF get_canvas_offset() const { return canvas_offset_; }
  
  int get_disp_mode() const { return (int)disp_mode_; }
  void set_disp_mode(int mode) { disp_mode_ = (DisplayMode)mode; }
  
  void SetTransform(const QTransform& trans) { transform_ = trans; }
  void ResetOffset();
  
  void SetTouchUpMode(bool mode);
  
public slots:
  void ZoomIn();
  void ZoomOut();
  void TransformModeChanged(int);
  
signals:
  void FrameSelectedFromPt(QPoint);
  void RegionSelectedFromPt(QPoint);
  
protected:
  void paintEvent(QPaintEvent*);
  
  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  void wheelEvent(QWheelEvent* event);
  void RegionSelectFromPt(const QPoint& ref_pos);
  void VideoScrubFromPt(const QPoint& pos);

protected:
  enum TransformMode { TRANSFORM_FRAME, TRANSFORM_CANVAS };
  enum DisplayMode { DISPLAY_NORMAL = 0, DISPLAY_IMAGE = 1, DISPLAY_CANVAS = 2 };
  
protected:
  shared_ptr<BufferedImage> cur_img_;
  bool draw_cur_image_;
  float cur_img_alpha_;
  bool is_touch_up_mode_;
  shared_ptr<QImage> canvas_;
  QPointF canvas_offset_;

  float scale_;
  QPoint offset_;
  QTransform transform_;
  
  TransformMode transform_mode_;
  QPoint click_pt_;
  
  DisplayMode disp_mode_;
  QImage* region_selection_mask_;

};

#endif  // COMPOSITING_WIDGET_H__
