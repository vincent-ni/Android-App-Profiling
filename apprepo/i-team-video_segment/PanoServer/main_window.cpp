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
  composit_->setThumbView(thumb_);
  vertical_layout->addWidget(composit_);
  
  output_ = new QTextEdit(this);
  output_->setMinimumHeight(120);
  vertical_layout->addWidget(output_);
  output_->setAutoFillBackground(true);
  pal = output_->palette();
  pal.setColor(QPalette::Button, Qt::black);
  pal.setColor(QPalette::Base, Qt::black);
  pal.setColor(QPalette::Text, Qt::gray);
  output_->setPalette(pal);
  output_->setReadOnly(true);
  
  server_udp_ = new PanoServerUDP(34567, output_);
  server_udp_->setView(composit_);
  server_tcp_ = new PanoServerTCP(34568, output_);
  server_tcp_->setView(composit_);
  
  QPushButton* load_button = new QPushButton("Load Images");
  header_layout->addWidget(load_button);
  connect(load_button, SIGNAL(clicked()), this, SLOT(LoadImages()));
  
  QPushButton* start_server = new QPushButton("Start Server");
  header_layout->addWidget(start_server);
  connect(start_server, SIGNAL(clicked()), this, SLOT(StartServer()));
    
  header_layout->addStretch();
  
  setCentralWidget(horiz_sep);
}

MainWindow::~MainWindow() {

}

void MainWindow::LoadImages() {
  images_.clear();
  
  QStringList files = QFileDialog::getOpenFileNames(
                          this, tr("Load Images"), "~/Pictures",
                          tr("Images (*.png, *.jpeg, *.jpg, *.tiff, *.bmp, *"));
  
  QStringList files_copy = files;
  for (QStringList::Iterator i = files_copy.begin(); i != files_copy.end(); ++i) {
    // Load files and add to ThumbnailView.
    QImage* img = new QImage(*i);
    std::cout << i->toStdString() << " " << img->width() << " " << img->height() << "\n";
    images_.push_back(shared_ptr<QImage>(img));
    thumb_->addImage(img, *i);
  }
}

void MainWindow::StartServer() {
  server_tcp_->Restart();
  server_udp_->Restart();
}