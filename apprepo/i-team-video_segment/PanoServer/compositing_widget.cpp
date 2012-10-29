/*
 *  compositing_widget.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/3/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "compositing_widget.h"
#include "thumbnail_view.h"

#include <QtGui>
#include <iostream>

const float WIDGET_WIDTH = 320;
const float WIDGET_HEIGHT = 460;

float QPointNorm(const QPoint& lhs, const QPoint& rhs) {
  float dx = lhs.x() - rhs.x();
  float dy = lhs.y() - rhs.y();
  return dx*dx + dy*dy;
}

CompositingWidget::CompositingWidget(QWidget* parent) : QWidget(parent), 
                                                        scale_(1.0),
                                                        selected_image_idx_(-1),
                                                        next_img_id_(0) {
  setMinimumSize(WIDGET_WIDTH, WIDGET_HEIGHT);
  setMaximumSize(WIDGET_WIDTH, WIDGET_HEIGHT);
  resize(WIDGET_WIDTH, WIDGET_HEIGHT);
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

void CompositingWidget::ZoomIn() {
  scale_ *= 2.0;
  repaint();
}

void CompositingWidget::ZoomOut() {
  scale_ *= 0.5;
  repaint();
}

int CompositingWidget::AddImage(const QImage& img) {
  image_data_.push_back(new QImage(img));
  int img_id = next_img_id_++;
  images_.push_back(ImageInfo(image_data_.back(), img_id));
  repaint();
  
  if (thumb_view_) {
    thumb_view_->addImage(image_data_.back(), "");
  }
  
  return img_id;
}

bool CompositingWidget::aquireLock(int img_id, QTcpSocket* conn) {
  for (vector<ImageInfo>::iterator i = images_.begin(); i != images_.end(); ++i) {
    if (i->img_id == img_id) {
      // Already locked?
      if (i->locked_by) {
        return false;
      } else {
        i->locked_by = conn;
        return true;
      }
    }
  }
  return false;
}

bool CompositingWidget::releaseLock(int img_id, QTcpSocket* conn) {
  for (vector<ImageInfo>::iterator i = images_.begin(); i != images_.end(); ++i) {
    if (i->img_id == img_id) {
      // Already locked?
      if (i->locked_by == conn) {
        i->locked_by = 0;
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

void CompositingWidget::paintEvent(QPaintEvent* pe) {
  QPainter painter(this);
  
  painter.setCompositionMode(QPainter::CompositionMode_Plus);
  painter.setRenderHint(QPainter::Antialiasing);  
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  
  // Draw images at pos.
  for (vector<ImageInfo>::const_iterator i = images_.begin(); i != images_.end(); ++i) {
    painter.resetTransform();
    painter.translate(0, WIDGET_HEIGHT);
    
    painter.translate(i->trans_x, -i->trans_y);
    painter.scale(i->scale, i->scale);
    painter.rotate(-i->rotation / M_PI * 180.0);
    painter.drawImage(QPoint(0, -i->image->height() + 1), *i->image);
  }
   
}

void CompositingWidget::updateImgPosition(int id, float scale, float rot, float dx, float dy) {
  for(vector<ImageInfo>::iterator i = images_.begin(); i != images_.end(); ++i) {
    if (i->img_id == id) {
      ImageInfo& ii = *i;
      ii.scale = scale;
      ii.rotation = rot;
      ii.trans_x = dx;
      ii.trans_y = dy;
      repaint();
    }
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
    ImageInfo img_info(img, -1);
    
    images_.push_back(img_info);
    repaint();
    
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else {
    event->ignore();
  }
}
