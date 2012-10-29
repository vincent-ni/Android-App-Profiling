/*
 *  MainWindow.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/2/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <QtGui>
#include <QString>
#include <iostream>
#include <string>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "main_window.h"
#include "compositing_widget.h"

#include <fstream>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  QPalette pal = palette();
  pal.setColor(backgroundRole(), Qt::darkGray);
  setPalette(pal);
  
  QWidget* vertical_sep = new QWidget;
  QVBoxLayout* vertical_layout = new QVBoxLayout;
  vertical_sep->setLayout(vertical_layout);
  
  QWidget* horiz_sep = new QWidget;
  QHBoxLayout* horiz_layout = new QHBoxLayout;
  horiz_sep->setLayout(horiz_layout);
  
  QWidget* header = new QWidget;
  QHBoxLayout* header_layout = new QHBoxLayout;
  header->setLayout(header_layout);
  
  horiz_layout->addWidget(vertical_sep);
  thumb_ = new ThumbnailView(this);
  horiz_layout->addWidget(thumb_);
  
  vertical_layout->addWidget(header);
  composit_ = new CompositingWidget(this);
  vertical_layout->addWidget(composit_);
  
  QPushButton* load_button = new QPushButton("Load Images");
  header_layout->addWidget(load_button);
  connect(load_button, SIGNAL(clicked()), this, SLOT(loadImages()));
  
  QPushButton* save_config = new QPushButton("Save Config");
  header_layout->addWidget(save_config);
  connect(save_config, SIGNAL(clicked()), this, SLOT(saveConfig()));
  
  QPushButton* load_config = new QPushButton("Load Config");
  header_layout->addWidget(load_config);
  connect(load_config, SIGNAL(clicked()), this, SLOT(loadConfig()));
  
  QPushButton* auto_align = new QPushButton("Auto-Align");
  header_layout->addWidget(auto_align);
  connect(auto_align, SIGNAL(clicked()), this, SLOT(autoAlign()));
  
  header_layout->addStretch();
  
  setCentralWidget(horiz_sep);
}

MainWindow::~MainWindow() {

}

void MainWindow::AlignFromSFMFeatures() {
  // Load all features for all images.
  for (vector<ImgRep>::iterator i = images_.begin(); i != images_.end(); ++i) {
    string feature_file = i->filename.substr(0, i->filename.size()-3) + "pnt";
    std::ifstream ifs(feature_file.c_str(), std::ios_base::in);
    if (!ifs) {
      QMessageBox::critical(this, tr("Error opening feature file"), 
                            QString("Can't open") + feature_file.c_str());
      return;
    }
    
    string line;
    std::getline(ifs, line);
    if (line != "#FPOINT V2.0") {
      QMessageBox::critical(this, tr("Error opening feature file"), 
                            QString(feature_file.c_str()) + "is not a valid feature file" );
    }
    
    while(ifs) {
      std::getline(ifs, line);
      
      vector<std::string> line_elems;
      boost::split(line_elems, line, boost::is_any_of("\t"));
      
      if (line_elems.size() < 8)
        continue;
      
      int id;
      if (!line_elems[7].empty())
        id = boost::lexical_cast<int>(line_elems[7]);
      else
        id = 0;
      
      float pos_x = boost::lexical_cast<float>(line_elems[0]);
      float pos_y = boost::lexical_cast<float>(line_elems[1]);
      
      Feature new_feat(QPointF(pos_x, pos_y), id);
      
      bool has_prev_location = false;
      if (line_elems.size() > 10 && !line_elems[9].empty() && !line_elems[10].empty()) {
        new_feat.prev_location.rx() = boost::lexical_cast<float>(line_elems[9]);
        new_feat.prev_location.ry() = boost::lexical_cast<float>(line_elems[10]);
        has_prev_location = true;
      }
      
      // Id == 0 indicates that the feature point 
      // was not registered as 3D point.
      if(id || has_prev_location) {
        vector<Feature>::iterator insert_pos = std::lower_bound(i->features.begin(),
                                                                i->features.end(),
                                                                new_feat);
        i->features.insert(insert_pos, new_feat);
      }
    }
  }
  
  /*
  // Draw feature lines to check.
  for (vector<ImgRep>::const_iterator cur = images_.begin(); cur != images_.end(); ++cur) {
    vector<ImgRep>::const_iterator next = cur + 1;
    if (next != images_.end()) {
      // Traverse features.
      QPainter painter(cur->image.get());
      painter.setPen(QColor(0, 255, 0));
      for (FeatList::const_iterator cur_feat = cur->features.begin();
           cur_feat != cur->features.end();
           ++cur_feat) {
        // Find in adjacent image.
        FeatList::const_iterator next_feat = std::lower_bound(next->features.begin(),
                                                             next->features.end(),
                                                             *cur_feat);
        if (next_feat != next->features.end() && *next_feat == *cur_feat) {
          painter.drawLine(cur_feat->first, next_feat->first);
        }
      }
    }
  }
  */
  
  // TODO: Try to fit homography to matches
  // Right now: Only translation.
  if (!images_.size())
    return;
  
  // Add first image.
  composit_->addImage(ImageInfo(images_.front().image.get(), QPointF(0, 0), 
                                images_.front().filename));
  
  // Align remaining ones.
  for (vector<ImgRep>::iterator cur = images_.begin() + 1; cur != images_.end(); ++cur) {
    float weight_sum = 0;
    float weight;
    
    QPointF offset (0, 0);

    // Traverse features.
    for (vector<Feature>::const_iterator feat = cur->features.begin();
         feat != cur->features.end();
         ++feat) {
      if(feat->prev_location.x() < 0)
        continue;
      // This is a matched 3D point
      if (feat->feat_id)
        weight = 0.8;
      else
        weight = 0.2;
      
      offset -= (feat->location - feat->prev_location) * weight;
      weight_sum += weight;
    }
    
    if(weight_sum == 0) {
      QMessageBox::critical(this, tr("Error auto-aligning images"), 
                            QString("Not enough matches to align image") + cur->filename.c_str());
      continue;
    }
    
    offset /= weight_sum;
    composit_->addImage(ImageInfo(cur->image.get(), offset + (cur-1)->offset, cur->filename));
    cur->offset = offset + (cur-1)->offset;
  }
}

void MainWindow::AlignFromOpticalFlow() {
  
}

void MainWindow::loadImages() {
  QStringList files = QFileDialog::getOpenFileNames(
                          this, tr("Load Images"), "~/Pictures",
                          tr("Images (*.png, *.jpeg, *.jpg, *.tiff, *.bmp, *"));
  
  QStringList files_copy = files;
  for (QStringList::Iterator i = files_copy.begin(); i != files_copy.end(); ++i) {
    // Load files and add to ThumbnailView.
    QImage* img = new QImage(*i);
    std::cout << i->toStdString() << " " << img->width() << " " << img->height() << "\n";
    images_.push_back(ImgRep(img, i->toStdString()));
    thumb_->addImage(img, *i);
  }
}

void MainWindow::saveConfig() {
  
  QString file_name = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                  "", tr("Video Patch File (*.vpf)"));
  if (file_name.count() == 0)
    return;
  
  std::ofstream ofs(file_name.toStdString().c_str(), std::ios_base::trunc);
  
  // Get positions from composit widget.
  const vector<ImageInfo>& image_infos = composit_->getImageInfos();
  for (vector<ImageInfo>::const_iterator i = image_infos.begin(); i != image_infos.end(); ++i) {
    ofs << i->filename << " " << i->pos.x() << " " << i->pos.y()
    << " " <<  i->image->width() << " " << i->image->height();
    if (i+1 != image_infos.end())
      ofs << "\n";
  }
}

void MainWindow::loadConfig() {
  QString file_name = QFileDialog::getOpenFileName(this, tr("Load File"), "");
  if (file_name.count() == 0)
    return;
  
  std::ifstream ifs(file_name.toAscii().data(), std::ios_base::in);
  string img_file;
  float pos_x, pos_y;
  int width, height;
  while (ifs) {
    ifs >> img_file >> pos_x >> pos_y >> width >> height;
    QImage* img = new QImage(img_file.c_str());
    images_.push_back(ImgRep(img, img_file));
    composit_->addImage(ImageInfo(img, QPoint(pos_x, pos_y), img_file));
  }
}

void MainWindow::autoAlign() {
  QMessageBox which_action(this);
  which_action.setText(tr("Auto Align Method"));
  which_action.setInformativeText(tr("Select auto-align method"));
  QPushButton* load_sfm_features = which_action.addButton(tr("Load SFM Features"), 
                                                          QMessageBox::ActionRole);
  QPushButton* compute_from_flow = which_action.addButton(tr("Compute from flow"),
                                                          QMessageBox::ActionRole);
 which_action.addButton(QMessageBox::Cancel);
  which_action.exec();
  
  if (which_action.clickedButton() == load_sfm_features) 
    AlignFromSFMFeatures();
  else if (which_action.clickedButton() == compute_from_flow)
    AlignFromOpticalFlow();
}