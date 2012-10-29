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

class QTcpSocket;
class ThumbnailView;

struct ImageInfo {
  ImageInfo (QImage* img, int _img_id) : image(img), img_id(_img_id), scale(1.0), 
    rotation(0), trans_x(0), trans_y(0), locked_by(0) {}
  QImage* image;
  int       img_id;
  float     scale;
  float     rotation;
  float     trans_x;
  float     trans_y;
  QTcpSocket* locked_by;
};

class CompositingWidget : public QWidget {
  Q_OBJECT
public:
  CompositingWidget(QWidget* parent =0);
  vector<ImageInfo>& getImageInfos() { return images_; }

  void updateImgPosition(int id, float scale, float rot, float dx, float dy);
  int AddImage(const QImage& rhs);
  
  bool aquireLock(int img_id, QTcpSocket* conn);
  bool releaseLock(int img_id, QTcpSocket* conn);
  void setThumbView(ThumbnailView* view) { thumb_view_ = view; };
public slots:
  void ZoomIn();
  void ZoomOut();
  
protected:
  void paintEvent(QPaintEvent*);
  
  void dragEnterEvent(QDragEnterEvent* event);
  void dragMoveEvent(QDragMoveEvent* event);
  void dropEvent(QDropEvent* event);
  
  vector<ImageInfo> images_;
  vector<QImage*> image_data_;
  
  float scale_;
  int selected_image_idx_;
  QPoint click_pt_;
  
  int next_img_id_;
  ThumbnailView* thumb_view_;
};

#endif  // COMPOSITING_WIDGET_H__
