/*
 *  thumbnail_view.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/3/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef THUMBNAIL_VIEW_H__
#define THUMBNAIL_VIEW_H__

#include <QListWidget>

class ThumbnailView : public QListWidget {
  Q_OBJECT
public:
  ThumbnailView(QWidget* parent = 0);
  void addImage(QImage*, const QString&);
  
protected:
  
  QImage thumbnailFromImage(const QImage*);
  
  void dragEnterEvent(QDragEnterEvent* event);
  void dragMoveEvent(QDragMoveEvent* event);
  void dropEvent(QDropEvent* event);
  void startDrag(Qt::DropActions supportedActions);
};


#endif  // THUMBNAIL_VIEW_H__