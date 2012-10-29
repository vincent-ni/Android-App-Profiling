/*
 *  thumbnail_view.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/3/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "thumbnail_view.h"

const float THUMB_RATIO = 9.0 / 16.0;
const float THUMB_WIDTH = 150.0;

#include <QtGui>
#include <QMetaType>

ThumbnailView::ThumbnailView(QWidget* parent) : QListWidget(parent) {
  setDragEnabled(true);
  setViewMode(QListView::IconMode);
  setMinimumWidth(THUMB_WIDTH+30);
  setIconSize(QSize(THUMB_WIDTH, THUMB_WIDTH * THUMB_RATIO));
  setSpacing(10);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  
  QPalette pal = palette();
  pal.setColor(QPalette::Button, Qt::black);
  setAutoFillBackground(true);
  pal.setColor(QPalette::Base, Qt::black);
  setPalette(pal);
  
};

void ThumbnailView::addImage(QImage* img, const QString& filename) {
  
  QPixmap pix_map = QPixmap::fromImage(thumbnailFromImage(img));
  
  QListWidgetItem* new_item = new QListWidgetItem(this);
  new_item->setIcon(QIcon(pix_map));
  new_item->setData(Qt::UserRole, QVariant(pix_map));
  new_item->setData(Qt::UserRole + 1, QVariant::fromValue((QObject*)img));
  new_item->setData(Qt::UserRole + 2, filename);
  new_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
}

QImage ThumbnailView::thumbnailFromImage(const QImage* img) {
  float aspect = (float) img->height() / (float) img->width();
  if (aspect < THUMB_RATIO) {
    // Crop left and right.
    QImage scaled_img = img->scaledToHeight(THUMB_WIDTH * THUMB_RATIO, Qt::SmoothTransformation);
    return scaled_img.copy((scaled_img.width() - THUMB_WIDTH)/2, 0, THUMB_WIDTH, 
                           scaled_img.height());

  } else {
    QImage scaled_img = img->scaledToWidth(THUMB_WIDTH, Qt::SmoothTransformation);
    return scaled_img.copy(0, (scaled_img.height() - THUMB_WIDTH * THUMB_RATIO)/2,
                           scaled_img.width(), THUMB_WIDTH * THUMB_RATIO);
  }
}

void ThumbnailView::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data"))
    event->accept();
  else
    event->ignore();
}

void ThumbnailView::dragMoveEvent(QDragMoveEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data")) {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else
    event->ignore();
}


void ThumbnailView::dropEvent(QDropEvent* event) {
  if (event->mimeData()->hasFormat("image/thumbnail_with_data")) {
  
  }
}


void ThumbnailView::startDrag(Qt::DropActions supportedActions) {
  
  // Initialize serialization.
  QListWidgetItem* item = currentItem();
  QByteArray item_data;
  QDataStream data_stream(&item_data, QIODevice::WriteOnly);
  
  // Write data.
  QPixmap pix_map = item->data(Qt::UserRole).value<QPixmap>();
  QImage* img = (QImage*) item->data(Qt::UserRole + 1).value<QObject*>();
  QString filename = item->data(Qt::UserRole + 2).value<QString>();
  
  data_stream << pix_map;
  data_stream.writeRawData((char*)&img, sizeof(img));
  data_stream << filename;

  // Setup mime-type.
  QMimeData* mime_data = new QMimeData;
  mime_data->setData("image/thumbnail_with_data", item_data);
  
  // Pass drag object.
  QDrag* drag = new QDrag(this);
  drag->setMimeData(mime_data);
  drag->setPixmap(pix_map);
  drag->setHotSpot(QPoint(THUMB_WIDTH * 0.5, THUMB_WIDTH * THUMB_RATIO * 0.5));
  
  
  if (drag->exec() == Qt::MoveAction) {
    delete takeItem(row(item));
  }
}