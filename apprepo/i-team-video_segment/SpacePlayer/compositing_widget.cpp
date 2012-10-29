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
#include "main_window.h"

const float WIDGET_WIDTH = 950;
const float WIDGET_HEIGHT = 600;

float QPointNorm(const QPointF& lhs, const QPointF& rhs) {
  float dx = lhs.x() - rhs.x();
  float dy = lhs.y() - rhs.y();
  return dx*dx + dy*dy;
}

CompositingWidget::CompositingWidget(QWidget* parent) : 
    QWidget(parent), scale_(1.0), offset_(0, 0), draw_cur_image_(true), cur_img_alpha_(1.0),
    is_touch_up_mode_(false), disp_mode_(DISPLAY_NORMAL) {
  setMinimumSize(WIDGET_WIDTH, WIDGET_HEIGHT);
  setMaximumSize(WIDGET_WIDTH, WIDGET_HEIGHT);
  resize(WIDGET_WIDTH, WIDGET_HEIGHT);

  transform_mode_ = TRANSFORM_FRAME;
      
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

void CompositingWidget::SetBufferedImage(BufferedImage* img) {
  cur_img_.reset(img);
  offset_ = QPoint((WIDGET_WIDTH - img->get_width()) / 2,
                   (WIDGET_HEIGHT - img->get_height()) / 2);
}

void CompositingWidget::SetSelectionMask(QImage* mask) {
  region_selection_mask_ = mask;
}

void CompositingWidget::SetCanvas(QImage* canvas, QPointF canvas_offset) {
  canvas_.reset(canvas);
  canvas_offset_ = canvas_offset;
  disp_mode_ = DISPLAY_NORMAL;
  
}

void CompositingWidget::ResetOffset() {
  if(cur_img_) 
    offset_ = QPoint((WIDGET_WIDTH - cur_img_->get_width()) / 2,
                     (WIDGET_HEIGHT - cur_img_->get_height()) / 2);
  
}

void CompositingWidget::SetTouchUpMode(bool mode) {
  is_touch_up_mode_ = mode; 
  if (is_touch_up_mode_)
    cur_img_alpha_ = 0.5;
  else
    cur_img_alpha_ = 1.0;
}

void CompositingWidget::ZoomIn() {
  scale_ *= 1.3;
  repaint();
}

void CompositingWidget::ZoomOut() {
  scale_ *= (1.0/1.3);
  repaint();
}

void CompositingWidget::TransformModeChanged(int i) {
  switch (i) {
    case 0:
      transform_mode_ = TRANSFORM_FRAME;
      break;
    case 1:
      transform_mode_ = TRANSFORM_CANVAS;
      break;
  }
}

void CompositingWidget::paintEvent(QPaintEvent* pe) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
 // painter.setCompositionMode(QPainter::CompositionMode_Lighten);
  
  // Set scaling.
  painter.translate(WIDGET_WIDTH / 2 , WIDGET_HEIGHT / 2);
  painter.scale(scale_, scale_);
  painter.translate(-WIDGET_WIDTH / 2, -WIDGET_HEIGHT / 2);
  
  painter.translate(offset_);
  
  if (transform_mode_ == TRANSFORM_FRAME) {
    if (canvas_)
      painter.drawImage(canvas_offset_, *canvas_);
  
    painter.setTransform(transform_, true);
  } else if (transform_mode_ == TRANSFORM_CANVAS) {
    QTransform old_trans = painter.transform();
    painter.setTransform(transform_.inverted(), true);
    
    if (canvas_) {
      painter.drawImage(canvas_offset_, *canvas_);
    }

    painter.setTransform(old_trans, false);
  }
  
  if (cur_img_ && draw_cur_image_ ) {
    cur_img_->Lock();
    shared_ptr<QImage> img = cur_img_->NewFrontView();
    if (!is_touch_up_mode_) {
      painter.drawImage(QPoint(0,0), *img);
    } else {
      if (disp_mode_ == DISPLAY_CANVAS) {
        // Copy from canvas.
        QPointF frame_tl (0, 0);
        QPointF tl = frame_tl * transform_;
        tl -= canvas_offset_;
        
        const uchar* canvas_ptr = canvas_->scanLine((int)tl.y()) + 4 * (int)tl.x();
        const int canvas_step = canvas_->width() * 4;
        QImage canvas_copy(canvas_ptr, img->width(), img->height(), canvas_step, 
                           QImage::Format_RGB32);
        painter.drawImage(QPoint(0, 0), canvas_copy);
      } else {
        if (disp_mode_ == DISPLAY_NORMAL)
          painter.setOpacity(cur_img_alpha_);
        else if (disp_mode_ == DISPLAY_IMAGE)
          painter.setOpacity(1.0);

        painter.drawImage(QPoint(0, 0), *img);
        painter.setOpacity(1.0);
      }
      
      if (region_selection_mask_) {
        painter.drawImage(QPoint(0, 0), *region_selection_mask_);
      }
    }
    
    cur_img_->Unlock();
  }
}
  
void CompositingWidget::mousePressEvent(QMouseEvent* event) {
  // Move canvas mode.
  if (event->button() == Qt::LeftButton) {
    if (is_touch_up_mode_) {
      RegionSelectFromPt(event->pos());
    } else {
      // Check whether point is in any rect.
      click_pt_ = event->pos();
      
      // Convert to coordinate system.
      QPoint pos = event->pos();
      pos -= QPoint(WIDGET_WIDTH / 2, WIDGET_HEIGHT / 2);
      pos = pos / scale_;
      pos += QPoint(WIDGET_WIDTH / 2 , WIDGET_HEIGHT / 2);
      
      pos -= offset_;
    }
    
  } else if (event->button() == Qt::RightButton) {
    // Video scrubbing mode. 
    VideoScrubFromPt(event->pos());
  }
  
  /*
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
   */
}

void CompositingWidget::mouseMoveEvent(QMouseEvent* event) {
  if (event->buttons() & Qt::LeftButton) {
    if (is_touch_up_mode_) {
      RegionSelectFromPt(event->pos());
    } else {
      QPoint diff = event->pos() - click_pt_;
      diff = diff / scale_;
      click_pt_ = event->pos();
      offset_ += diff;
      repaint();
    }
  } else if (event->buttons() & Qt::RightButton) {
    VideoScrubFromPt(event->pos());
  }
}

void CompositingWidget::wheelEvent(QWheelEvent* event) {
  int delta = event->delta() / 8 / 10;
  if (delta > 0) {
    ZoomIn();
  } else if (delta < 0) {
    ZoomOut();
  }
}

void CompositingWidget::RegionSelectFromPt(const QPoint& ref_pos) {
  // Convert to video CS.
  QPoint pos = ref_pos;
  pos -= QPoint(WIDGET_WIDTH / 2.0, WIDGET_HEIGHT / 2.0);
  pos = pos / scale_;
  pos += QPoint(WIDGET_WIDTH / 2.0 , WIDGET_HEIGHT / 2.0);
  
  pos -= offset_;
  pos -= QPoint(canvas_offset_.x(), canvas_offset_.y());
  
  // Select region.
  emit(RegionSelectedFromPt(pos));
}

void CompositingWidget::VideoScrubFromPt(const QPoint& ref_pos) {
  // Convert to video CS.
  QPoint pos = ref_pos;
  pos -= QPoint(WIDGET_WIDTH / 2.0, WIDGET_HEIGHT / 2.0);
  pos = pos / scale_;
  pos += QPoint(WIDGET_WIDTH / 2.0 , WIDGET_HEIGHT / 2.0);
  
  pos -= offset_;
  pos -= QPoint(canvas_offset_.x(), canvas_offset_.y());
  
  // Find closest frame.
  emit(FrameSelectedFromPt(pos));
  
}
