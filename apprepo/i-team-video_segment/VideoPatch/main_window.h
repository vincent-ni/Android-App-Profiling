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
#include <utility>
using std::pair;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <QMainWindow>
#include <QImage>

#include "thumbnail_view.h"
#include "compositing_widget.h"

struct Feature {
  Feature(QPointF loc, int _id) : location(loc), feat_id(_id), prev_location(-1, -1) {}
  
  bool operator<(const Feature& rhs) const { return feat_id < rhs.feat_id; }
  bool operator==(const Feature& rhs) const { return feat_id == rhs.feat_id; }
  
  QPointF location;
  int    feat_id;
  QPointF prev_location;
  };


struct ImgRep {
  ImgRep(QImage* img, string file) : image(img), filename(file), offset(0, 0) {}
  shared_ptr<QImage> image;
  string filename;
  vector<Feature> features;
  QPointF offset;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow (QWidget* parent =0);
  ~MainWindow ();
  
  void AlignFromSFMFeatures();
  void AlignFromOpticalFlow();
  
public slots:
  void loadImages();
  void saveConfig();
  void loadConfig();
  void autoAlign();
  
protected:
  ThumbnailView* thumb_;
  CompositingWidget* composit_;
  
  vector<ImgRep> images_;
};
  

#endif  // MAIN_WINDOW_H__