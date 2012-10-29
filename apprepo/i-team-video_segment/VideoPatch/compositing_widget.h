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

#include <QWidget>

struct ImageInfo {
  ImageInfo (QImage* img, const QPointF& p, const string& file) : image(img), pos(p), filename(file) {}
  QImage* image;
  QPointF pos;
  string filename;
};

class CompositingWidget : public QWidget {
  Q_OBJECT
public:
  CompositingWidget(QWidget* parent =0);
  
  const vector<ImageInfo>& getImageInfos() const { return images_; }
  void addImage(const ImageInfo& info);
  
public slots:
  void ZoomIn();
  void ZoomOut();
  
protected:
  void paintEvent(QPaintEvent*);
  
  void dragEnterEvent(QDragEnterEvent* event);
  void dragMoveEvent(QDragMoveEvent* event);
  void dropEvent(QDropEvent* event);
  
  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  
  vector<ImageInfo> images_;
  float scale_;
  QPoint offset_;
  
  int selected_image_idx_;
  QPoint click_pt_;
};

#endif  // COMPOSITING_WIDGET_H__
