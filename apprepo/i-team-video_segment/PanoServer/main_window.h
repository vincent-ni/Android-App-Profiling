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

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <QMainWindow>
#include <QImage>
#include <QTextEdit>

#include "thumbnail_view.h"
#include "compositing_widget.h"
#include "pano_server_tcp.h"
#include "pano_server_udp.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow (QWidget* parent =0);
  ~MainWindow ();
  
public slots:
  void LoadImages();
  void StartServer();
  
protected:
  ThumbnailView* thumb_;
  CompositingWidget* composit_;
  QTextEdit* output_;
  PanoServerTCP* server_tcp_;
  PanoServerUDP* server_udp_;
  
  vector<shared_ptr<QImage> > images_;
  
};
  

#endif  // MAIN_WINDOW_H__