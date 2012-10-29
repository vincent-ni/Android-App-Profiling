/*
 *  compositing_widget.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/3/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "compositing_widget.h"

#include <QtGui>
#include <iostream>

const float WIDGET_SIZE = 800;

float QPointNorm(const QPointF& lhs, const QPointF& rhs) {
  float dx = lhs.x() - rhs.x();
  float dy = lhs.y() - rhs.y();
  return dx*dx + dy*dy;
}

CompositingWidget::CompositingWidget(QWidget* parent) : QWidget(parent), 
                                                        scale_(1.0),
                                                        offset_(0, 0),
                                                        selected_image_idx_(-1) {
  setMinimumSize(WIDGET_SIZE, WIDGET_SIZE);
  setMaximumSize(WIDGET_SIZE, WIDGET_SIZE);
  resize(WIDGET_SIZE, WIDGET_SIZE);
  QPalette pal = palette();
  pal.setColor(backgroundRole(), Qt::black);
  setPalette(pal);
  setAutoFillBackground(true);
  
  setAcceptDrops(true);
  
  QPushButton* zoom_in = new QPushButton("+", this);
  QPushButton* zoom_out = new QPushButton("-", this);
  
  zoom_in->show();
  zoom_out->show();

  zoom_out->setGeometry(10, height() - 15 - zoom_out->height() * 2,
                        zoom_out->width(), zoom_out->height());   
  zoom_in->setGeometry(10, height() - 10 - zoom_in->height(), zoom_in->width(), zoom_in->height());
  
  connect(zoom_in, SIGNAL(clicked()), this, SLOT(ZoomIn()));
  connect(zoom_out, SIGNAL(clicked()), this, SLOT(ZoomOut()));
}

void CompositingWidget::addImage(const ImageInfo& info) {
  images_.push_back(info);
  repaint();
}

void CompositingWidget::ZoomIn() {
  scale_ *= 2.0;
  repaint();
}

void CompositingWidget::ZoomOut() {
  scale_ *= 0.5;
  repaint();
}

void CompositingWidget::paintEvent(QPaintEvent* pe) {
  QPainter painter(this);
  painter.setCompositionMode(QPainter::CompositionMode_Lighten);
  
  // Set scaling.
  painter.translate(WIDGET_SIZE / 2 , WIDGET_SIZE / 2);
  painter.scale(scale_, scale_);
  painter.translate(-WIDGET_SIZE / 2, -WIDGET_SIZE / 2);
  
  painter.translate(offset_);
  
  // Draw images at pos.
  for (vector<ImageInfo>::const_iterator i = images_.begin(); i != images_.end(); ++i) {
    painter.drawImage(i->pos, *i->image);
  }
}

void CompositingWidget::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data"))
    event->accept();
  else
    event->ignore();  
}

void CompositingWidget::dragMoveEvent(QDragMoveEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data")) {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else
    event->ignore();
  
}

void CompositingWidget::dropEvent(QDropEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data")) {
    QByteArray item_data = event->mimeData()->data("image/thumbnail_with_data");
    QDataStream data_stream(&item_data, QIODevice::ReadOnly);
    
    QPixmap pix_map;
    QImage* img;
    QString filename;
    
    data_stream >> pix_map;
    data_stream.readRawData((char*)&img, sizeof(img));
    data_stream >> filename;
    
    // Convert to coordinate system.
    QPoint pos = event->pos();

    pos -= QPoint(WIDGET_SIZE / 2, WIDGET_SIZE / 2);
    pos = pos / scale_;
    pos += QPoint(WIDGET_SIZE / 2 , WIDGET_SIZE / 2);
    
    pos -= offset_;
    
    images_.push_back(ImageInfo(img, pos - QPoint(img->width()/2, img->height()/2), 
                                filename.toStdString())); 
    repaint();
    
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else {
    event->ignore();
  }
}
  
void CompositingWidget::mousePressEvent(QMouseEvent* event) {
  // Check whether point is in any rect.
  click_pt_ = event->pos();
  
  vector<int> rect_ids;
  
  // Convert to coordinate system.
  QPoint pos = event->pos();
  pos -= QPoint(WIDGET_SIZE / 2, WIDGET_SIZE / 2);
  pos = pos / scale_;
  pos += QPoint(WIDGET_SIZE / 2 , WIDGET_SIZE / 2);
  
  pos -= offset_;
  
  for (vector<ImageInfo>::const_iterator i = images_.begin(); i != images_.end(); ++i) {
    if (pos.x() >= i->pos.x() && pos.x() < i->pos.x() + i->image->width() &&
        pos.y() >= i->pos.y() && pos.y() < i->pos.y() + i->image->height())
      rect_ids.push_back(i - images_.begin());
  }
  
  if (!rect_ids.empty() && event->button() == Qt::LeftButton) {
    if (rect_ids.size() == 1) {
      selected_image_idx_ = rect_ids[0];
    } else {
      // Find the one closest to centroid.
      int min_idx = 0;
      int min_val = QPointNorm(pos, images_[rect_ids[0]].pos +
                               QPointF((float)images_[rect_ids[0]].image->width() * 0.5,
                                       (float)images_[rect_ids[0]].image->height() * 0.5));
      
      for (int i = 1; i < rect_ids.size(); ++i) {
        float norm = QPointNorm(pos, images_[rect_ids[i]].pos +
                                QPointF((float)images_[rect_ids[i]].image->width() * 0.5,
                                        (float)images_[rect_ids[i]].image->height() *0.5));
        if (norm < min_val) {
          min_val = norm;
          min_idx = i;
        }
      }
      
      selected_image_idx_ = rect_ids[min_idx];
    }
  } else {
    selected_image_idx_ = -1;
  }
}

void CompositingWidget::mouseMoveEvent(QMouseEvent* event) {
  QPoint diff = event->pos() - click_pt_;
  diff = diff / scale_;
  click_pt_ = event->pos();
  
  if (selected_image_idx_ >= 0) {
    images_[selected_image_idx_].pos += diff;
    repaint();
  } else {
    offset_ += diff;
    repaint();
  }
}
