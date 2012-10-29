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
#include "video_analyzer.h"
#include "conversion_units.h"
#include "optical_flow_unit.h"
#include "region_flow.h"

#include <fstream>
#include "segmentation.pb.h"
#include "segmentation_unit.h"
#include "foreground_tracker.h"

#define FRAME_SPACING 5

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), analyze_pd_(0) {
  setWindowTitle("SpacePlayer - Do not distribute!");
  
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
  
  vertical_layout->addWidget(header);
  composit_ = new CompositingWidget(this);
  vertical_layout->addWidget(composit_);
  
  QPushButton* load_button = new QPushButton("Load video");
  header_layout->addWidget(load_button);
  connect(load_button, SIGNAL(clicked()), this, SLOT(LoadVideo()));
    
  analyse_button_ = new QPushButton("Analyze");
  header_layout->addWidget(analyse_button_);
  connect(analyse_button_, SIGNAL(clicked()), this, SLOT(AnalyzeVideo()));
  analyse_button_->setEnabled(false);
  
  play_button_ = new QPushButton("Play");
  header_layout->addWidget(play_button_);
  connect(play_button_, SIGNAL(clicked()), this, SLOT(PlayVideo()));
  play_button_->setEnabled(false);
  
  stop_button_ = new QPushButton("Stop");
  header_layout->addWidget(stop_button_);
  connect(stop_button_, SIGNAL(clicked()), this, SLOT(StopVideo()));
  stop_button_->setEnabled(false);
  
  step_button_ = new QPushButton("Step");
  header_layout->addWidget(step_button_);
  connect(step_button_, SIGNAL(clicked()), this, SLOT(StepVideo()));
  step_button_->setEnabled(false);  
  
  QTabWidget* pref_tabs = new QTabWidget(this);
  header_layout->addWidget(pref_tabs);
  
  // General Tab.
  QWidget* general_prefs = new QWidget(this);
  QHBoxLayout* general_layout = new QHBoxLayout;
  general_prefs->setLayout(general_layout);
  
  general_layout->addWidget(new QLabel("Transform: "));
  transform_mode_ = new QButtonGroup(this);
  
  QRadioButton* frame_mode = new QRadioButton("Frame", this);
  QRadioButton* canvas_mode = new QRadioButton("Canvas", this);
  
  general_layout->addWidget(frame_mode);
  general_layout->addWidget(canvas_mode);
  
  transform_mode_->addButton(frame_mode, 0);
  transform_mode_->addButton(canvas_mode, 1);
  
  connect(transform_mode_, SIGNAL(buttonClicked(int)), composit_, SLOT(TransformModeChanged(int)));
  
  frame_mode->setChecked(true);
  
  trace_canvas_ = new QCheckBox("Trace");
  general_layout->addWidget(trace_canvas_);
  connect(trace_canvas_, SIGNAL(stateChanged(int)), this, SLOT(TraceToggled(int)));
  
  QGroupBox* seg_level_box = new QGroupBox;
  QHBoxLayout* seg_level_layout = new QHBoxLayout;
  seg_level_layout->setContentsMargins(11, 5, 11, 0);
  seg_level_box->setLayout(seg_level_layout);
  
  seg_level_layout->addWidget(new QLabel("SegLevel:"));

  seg_level_label_ = new QLabel("None");
  seg_level_label_->setMinimumWidth(50);
  seg_level_layout->addWidget(seg_level_label_);

  seg_level_slider_ = new QSlider(Qt::Horizontal, this);
  seg_level_slider_->setEnabled(false);
  seg_level_layout->addWidget(seg_level_slider_);
  connect(seg_level_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SegLevelChanged(int)));
  general_layout->addWidget(seg_level_box);
    
  general_layout->addStretch();
  general_layout->setContentsMargins(11, 0, 11, 5);
  pref_tabs->addTab(general_prefs, "General");
  
  // FrameSpacing Tab.
  QWidget* frame_spacing_prefs = new QWidget(this);
  QHBoxLayout* frame_spacing_pref_layout = new QHBoxLayout;
  frame_spacing_prefs->setLayout(frame_spacing_pref_layout);
  
  QGroupBox* frame_spacing_box = new QGroupBox;
  QHBoxLayout* frame_spacing_layout = new QHBoxLayout;
  frame_spacing_layout->setContentsMargins(11, 5, 11, 0);
  frame_spacing_box->setLayout(frame_spacing_layout);
  
  frame_spacing_layout->addWidget(new QLabel("FrameSpacing:"));
  
  frame_spacing_label_ = new QLabel("Auto");
  frame_spacing_label_->setMinimumWidth(30);
  frame_spacing_layout->addWidget(frame_spacing_label_);
  
  frame_spacing_slider_ = new QSlider(Qt::Horizontal, this);
  frame_spacing_slider_->setMinimum(0);
  frame_spacing_slider_->setMaximum(30);
  frame_spacing_slider_->setValue(0);
  connect(frame_spacing_slider_, SIGNAL(sliderMoved(int)), this, SLOT(FrameSpacingChanged(int)));
  frame_spacing_layout->addWidget(frame_spacing_slider_);
  
  frame_spacing_pref_layout->addWidget(frame_spacing_box);
  
  QGroupBox* frame_spacing_ratio_box = new QGroupBox;
  QHBoxLayout* frame_spacing_ratio_layout = new QHBoxLayout;
  frame_spacing_ratio_layout->setContentsMargins(11, 5, 11, 0);
  frame_spacing_ratio_box->setLayout(frame_spacing_ratio_layout);
  
  
  frame_spacing_ratio_layout->addWidget(new QLabel("FrameRatio:"));
  
  frame_spacing_ratio_label_ = new QLabel("0.1");
  frame_spacing_ratio_label_->setMinimumWidth(30);
  frame_spacing_ratio_layout->addWidget(frame_spacing_ratio_label_);
  
  frame_spacing_ratio_slider_ = new QSlider(Qt::Horizontal, this);
  frame_spacing_ratio_slider_->setMinimum(0);
  frame_spacing_ratio_slider_->setMaximum(30);
  frame_spacing_ratio_slider_->setValue(10);
  
  connect(frame_spacing_ratio_slider_, SIGNAL(sliderMoved(int)),
          this, SLOT(FrameSpacingRatioChanged(int)));
  
  frame_spacing_ratio_layout->addWidget(frame_spacing_ratio_slider_);
  frame_spacing_pref_layout->addWidget(frame_spacing_ratio_box);
  
  frame_spacing_pref_layout->setContentsMargins(11, 0, 11, 5);
  pref_tabs->addTab(frame_spacing_prefs, "FrameSpacing");
  
  // TouchUp Tab.
  QWidget* touch_up_prefs = new QWidget(this);
  QHBoxLayout* touch_up_layout = new QHBoxLayout;
  touch_up_prefs->setLayout(touch_up_layout);
  
  touch_up_ = new QPushButton("(T)ouch up");
  touch_up_layout->addWidget(touch_up_);
  connect(touch_up_, SIGNAL(clicked()), this, SLOT(TouchUp()));
  touch_up_->setEnabled(false);
  
  stitch_ = new QPushButton("(S)titch");
  touch_up_layout->addWidget(stitch_);
  stitch_->setEnabled(false);
  connect(stitch_, SIGNAL(clicked()), this, SLOT(Stitch()));
  
  canvas_brush_ = new QPushButton("Canvas (B)rush");
 // touch_up_layout->addWidget(canvas_brush_);
  canvas_brush_->setEnabled(false);
  connect(canvas_brush_, SIGNAL(clicked()), this, SLOT(CanvasBrush()));
  
  selection_mode_ = new QButtonGroup(this);
  QRadioButton* frame_button = new QRadioButton("(F)rame", this);
  QRadioButton* canvas_button = new QRadioButton("(C)anvas", this);
  QRadioButton* erase_button = new QRadioButton("(E)rase", this);
  
  selection_mode_->addButton(frame_button, 0);
  selection_mode_->addButton(canvas_button, 1);
  selection_mode_->addButton(erase_button, 2);
  frame_button->setChecked(true);
  
  touch_up_layout->addWidget(new QLabel("Brush mode:"));
  touch_up_layout->addWidget(frame_button);
  touch_up_layout->addWidget(canvas_button);
  touch_up_layout->addWidget(erase_button);
  
  touch_up_layout->addStretch();
  touch_up_layout->setContentsMargins(11, 0, 11, 5);
  pref_tabs->addTab(touch_up_prefs, "TouchUp");
  
  // Canvas Tab.
  QWidget* canvas_prefs = new QWidget(this);
  QHBoxLayout* canvas_prefs_layout = new QHBoxLayout;
  canvas_prefs->setLayout(canvas_prefs_layout);
  
  scrub_mode_ = new QButtonGroup(this);
  QRadioButton* frame_center = new QRadioButton("Frame Center", this);
  QRadioButton* frame_source = new QRadioButton("Frame Source", this);
  
  scrub_mode_->addButton(frame_center, 0);
  scrub_mode_->addButton(frame_source, 1);
  
  canvas_prefs_layout->addWidget(new QLabel("ScrubMode:"));
  canvas_prefs_layout->addWidget(frame_center);
  canvas_prefs_layout->addWidget(frame_source);
  
  frame_center->setChecked(true);
  
  canvas_clean_ = new QPushButton("Clean", this);
  canvas_prefs_layout->addWidget(canvas_clean_);
  canvas_clean_->setEnabled(false);
  
  hide_canvas_ = new QPushButton("(H)ide Canvas", this);
  canvas_prefs_layout->addWidget(hide_canvas_);
  hide_canvas_->setEnabled(false);
  connect(hide_canvas_, SIGNAL(clicked()), this, SLOT(HideCanvas()));
  
  canvas_prefs_layout->addStretch();
  canvas_prefs_layout->setContentsMargins(11, 0, 11, 5);
  pref_tabs->addTab(canvas_prefs, "Canvas");
  
  // Project tab.
  
  QWidget* project_prefs = new QWidget(this);
  QHBoxLayout* project_layout = new QHBoxLayout;
  project_prefs->setLayout(project_layout);
  
  save_project_ = new QPushButton("Save Project", this);
  project_layout->addWidget(save_project_);
  connect(save_project_, SIGNAL(clicked()), this, SLOT(SaveProject()));
    
  load_project_ = new QPushButton("Load Project", this);
  project_layout->addWidget(load_project_);
  connect(load_project_, SIGNAL(clicked()), this, SLOT(LoadProject()));
  
  project_layout->addStretch();
  project_layout->setContentsMargins(11, 0, 11, 5);
  pref_tabs->addTab(project_prefs, "Project");
  
  header_layout->addStretch();
  header_layout->setContentsMargins(11, 0, 11, 0);
  vertical_layout->addStretch();
  vertical_layout->setContentsMargins(11, 0, 11, 5);
  setCentralWidget(horiz_sep);
}

MainWindow::~MainWindow() {

}

void MainWindow::InitSegmentationSlider() {
  string seg_file = (video_file_.left(video_file_.lastIndexOf(".")) + "_segment8.pb").toStdString();
  // Use it?
  if (QFile(seg_file.c_str()).exists()) {
    std::ifstream ifs (seg_file.c_str(), std::ios_base::in | std::ios_base::binary);
    
    int num_frames;
    int64_t header_offset;
    
    ifs.read(reinterpret_cast<char*>(&num_frames), sizeof(num_frames));
    ifs.read(reinterpret_cast<char*>(&header_offset), sizeof(header_offset));    
    
    Segment::SegmentationDesc seg;
    int frame_sz;
    ifs.read(reinterpret_cast<char*>(&frame_sz), sizeof(frame_sz));
    vector<char> frame_data(frame_sz);
    ifs.read(reinterpret_cast<char*>(&frame_data[0]), frame_sz);
    seg.ParseFromArray(&frame_data[0], frame_sz);
    
    seg_level_slider_->setMinimum(-1);
    seg_level_slider_->setMaximum(seg.hierarchy_size());
    seg_level_slider_->setValue(-1);
    seg_level_slider_->setSingleStep(1);
    
    seg_level_slider_->setEnabled(true);
    return;
  } else {
    seg_level_slider_->setEnabled(false); 
    seg_level_slider_->setValue(-1);
  }
  SegLevelChanged(-1);
}

void MainWindow::LoadVideo() {
  video_file_ = QFileDialog::getOpenFileName(this, tr("Load Video"), "");
  
  if (video_file_.isEmpty())
    return;
  
  LoadVideoImpl(video_file_);
}

void MainWindow::LoadVideoImpl(const QString& video_file) {
  if (play_unit_)
    play_unit_->Pause();
  
  composit_->SetCanvas(0, QPointF());
  composit_->SetTransform(QTransform());
  composit_->SetTouchUpMode(false);
  composit_->DrawBufferedImage(true);
  
  frame_infos_.clear();
  video_reader_.reset(new VideoReaderUnit(video_file.toStdString(), 
                                          0,
                                          "VideoStream", 
                                          VideoFramework::PIXEL_FORMAT_RGB24));
  
  int bundle_max_frames = 50;
  float bundle_frame_area_ratio = 0.5;
  bool use_keyframe_tracking = false;
  
  if (QMessageBox::question(this, "", "Attach additional analysis units to video stream "
                            "and use SpacePlayer in camera tracker mode?", 
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    lum_unit_.reset(new VideoFramework::LuminanceUnit());
    lum_unit_->AttachTo(video_reader_.get());
    
    if (use_keyframe_tracking) {
      flow_unit_.reset(new VideoFramework::OpticalFlowUnit(0.3,  // feature density. = 0.3
                                                           bundle_max_frames,  // frames tracked.
                                                           "LuminanceStream",
                                                           "OpticalFlow",
                                                           "SegmentationStream",
                                                           "RegionFlow",
                                                           "KeyFrameStream"));
    } else {
      flow_unit_.reset(new VideoFramework::OpticalFlowUnit(0.3,
                                                           1,
                                                           "LuminanceStream",
                                                           "OpticalFlow"));
    }

    flow_unit_->AttachTo(lum_unit_.get());
    
    string seg_file = (video_file_.left(video_file.lastIndexOf(".")) + "_segment8.pb")
                          .toStdString();
    
    VideoFramework::VideoUnit* seg_supply_unit = 0;
    if (!QFile(seg_file.c_str()).exists()) {
      if (QMessageBox::question(this, "", "No Segmentation file found! You need a valid "
                                          "segmentation file at the location of the video. "
                                          "Should one be created while camera is tracked and "
                                          "written to file?", 
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::Yes) == QMessageBox::Yes) {
        
        flip_bgr_unit_.reset(new VideoFramework::FlipBGRUnit("VideoStream", "VideoStreamBGR24"));
        flip_bgr_unit_->AttachTo(lum_unit_.get());
        // TODO: fix!
        /*
        Segment::SegmentationOptions seg_options;
        seg_options.set_window_x_spacing(10000); // 100
        seg_options.set_window_y_spacing(10000); // 100
        seg_options.set_window_t_spacing(15);  // 15
        seg_options.set_parameter_k(0.02);
        seg_options.set_min_region_size(100); //100
        seg_options.set_domain(Segment::SegmentationOptions::SEGMENTATION_DOMAIN_2D);
        seg_options.set_segmentation_mode(
            Segment::SegmentationOptions::SEGMENTATION_MODE_HIERARCHY);
        seg_options.set_presmoothing(Segment::SegmentationOptions::PRESMOOTH_BILATERAL);
        seg_options.set_min_region_num(20);
        seg_options.set_max_region_num(300);
        
        std::string seg_option_string;
        seg_options.SerializeToString(&seg_option_string);
        seg_unit_.reset(new Segment::SegmentationUnit(seg_option_string, "VideoStreamBGR24"));
        seg_unit_->AttachTo(flip_bgr_unit_.get());

        seg_writer_unit_.reset (new Segment::SegmentationWriterUnit(seg_file));
        seg_writer_unit_->AttachTo(seg_unit_.get());      
        
        seg_supply_unit = seg_unit_.get();
         */
      }
    } else {
      seg_reader_unit_.reset(new Segment::SegmentationReaderUnit(seg_file));
      seg_reader_unit_->AttachTo(flow_unit_.get());
      seg_supply_unit = seg_reader_unit_.get();
    }
    
    // Use it?
    if (seg_supply_unit) {
      seg_render_unit_.reset(new Segment::SegmentationRenderUnit(0.1));
      seg_render_unit_->AttachTo(seg_supply_unit);
      
      region_flow_unit_.reset(new VideoFramework::RegionFlowUnit);
      region_flow_unit_->AttachTo(seg_render_unit_.get());
      
      // Feedback RegionFlow results into the optical flow unit.
      if (use_keyframe_tracking) {
        region_flow_unit_->FeedbackResult(flow_unit_.get(), "SegmentationStream");
        region_flow_unit_->FeedbackResult(flow_unit_.get(), "RegionFlow");
      }
      
      string feat_file = (video_file_.left(video_file.lastIndexOf(".")) + ".feat").toStdString();
      foreground_tracker_.reset(new ForegroundTracker(feat_file,
                                                      use_keyframe_tracking,
                                                      bundle_max_frames,
                                                      bundle_frame_area_ratio));
      
      foreground_tracker_->AttachTo(region_flow_unit_.get());
      if (use_keyframe_tracking) {
        foreground_tracker_->FeedbackResult(flow_unit_.get(), "KeyFrameStream");
      }
                                                      
      play_unit_.reset(new PlayUnit(composit_, region_flow_unit_.get(), true, video_file,
                       "RenderedRegionStream"));
    } else {
      play_unit_.reset(new PlayUnit(composit_, flow_unit_.get(), true, video_file));
    }
    
    analyse_button_->setEnabled(false);    
  } else {
    play_unit_.reset(new PlayUnit(composit_, video_reader_.get(), true, video_file));
    analyse_button_->setEnabled(true);
  }
  
  video_analyzer_.reset();
  
  connect(play_unit_.get(), SIGNAL(Done()), this, SLOT(PlayEnded()));
  connect(play_unit_.get(), SIGNAL(CancelDone()), this, SLOT(CancelPlayEnded()));
  connect(play_unit_.get(), SIGNAL(DrawCanvasState(bool)), this, 
          SLOT(DrawCanvasStateChanged(bool)));
  connect(seg_level_slider_, SIGNAL(sliderMoved(int)), play_unit_.get(), 
          SLOT(SegLevelChanged(int)));
  connect(selection_mode_, SIGNAL(buttonClicked(int)), play_unit_.get(),
          SLOT(RegionSelectionMode(int)));
  connect(scrub_mode_, SIGNAL(buttonClicked(int)), play_unit_.get(),
          SLOT(ScrubMode(int)));
  connect(canvas_clean_, SIGNAL(clicked()), play_unit_.get(), SLOT(CanvasClean()));
  
  video_restart_ = false;
  
  // Get segmentation levels.
  InitSegmentationSlider();  
  
  play_button_->setEnabled(true);
  stop_button_->setEnabled(true);
  step_button_->setEnabled(true);
  trace_canvas_->setChecked(true);
  touch_up_->setEnabled(false);
  canvas_brush_->setEnabled(false);
  canvas_clean_->setEnabled(false);
  hide_canvas_->setEnabled(true);  
}

void MainWindow::AnalyzeVideo() {
  video_analyzer_.reset(new VideoAnalyzer(video_file_, frame_spacing_slider_->value(),
                                          frame_spacing_ratio_slider_->value() / 100.0,
                                          seg_level_slider_->value()));

  connect(video_analyzer_.get(), SIGNAL(AnalyzeProgress(int)), this, SLOT(AnalyzeProgress(int)));
  
  analyze_pd_ = new QProgressDialog("Analyzing video ... ", 
		                        "Cancel", 0, play_unit_->get_frame_guess());
  analyze_pd_->setWindowModality(Qt::WindowModal);
  analyze_pd_->setMinimumDuration(0);
  
  connect(analyze_pd_, SIGNAL(canceled()), video_analyzer_.get(), SLOT(CancelJob()));
  
  analyse_button_->setEnabled(false);
  analyze_time_.start();
  video_analyzer_->start();
}

void MainWindow::PlayVideo() {      
  play_unit_->set_draw_canvas(trace_canvas_->checkState() == Qt::Checked);
  if (play_button_->text() == "Play") {
    if (video_restart_) {
      video_reader_->Seek();
      video_restart_ = false;
    }
    
    play_unit_->Play();
    play_button_->setText("Pause");
  } else {
    play_unit_->Pause();
    play_button_->setText("Play"); 
  }
}

void MainWindow::StepVideo() {
  if (!video_reader_->NextFrame())
    StopVideo();
}

void MainWindow::StopVideo() {
  // Reset video.
  play_unit_->Pause();
  play_button_->setText("Play"); 
  play_unit_->set_draw_canvas(trace_canvas_->checkState() == Qt::Checked);
  composit_->ResetOffset();
  composit_->DrawBufferedImage(true);

  if (video_reader_->Seek()) {
    // Pull first frame.
    video_reader_->NextFrame();
  } else {
    // Propagate Reset offset from above.
    composit_->update();
  }
  
}

void MainWindow::TraceToggled(int state) {
  if (play_unit_)
    play_unit_->set_draw_canvas(state == Qt::Checked);
}

void MainWindow::SegLevelChanged(int value) {
  if (value == -1) {
    seg_level_label_->setText("None");
  }
  else {
    seg_level_label_->setText(QString::number(value));
  }
  
  if (seg_render_unit_)
    seg_render_unit_->SetHierarchyLevel(std::max(0, value));
  if (region_flow_unit_)
    region_flow_unit_->SetHierarchyLevel(std::max(0, value));
}

void MainWindow::FrameSpacingChanged(int value) {
  if (value == 0) {
    frame_spacing_label_->setText("Auto");
    frame_spacing_ratio_label_->setEnabled(true);
    frame_spacing_ratio_slider_->setEnabled(true);
  }
  else {
    frame_spacing_label_->setText(QString::number(value));
    frame_spacing_ratio_label_->setEnabled(false);
    frame_spacing_ratio_slider_->setEnabled(false);
  }
}

void MainWindow::FrameSpacingRatioChanged(int value) {
    frame_spacing_ratio_label_->setText(QString::number(value / 100.0));
}

void MainWindow::TouchUp() {
  if (touch_up_->text() == "(T)ouch up") {
    composit_->SetTouchUpMode(true);
    composit_->update();
    stitch_->setEnabled(true);
    touch_up_->setText("Done re(t)ouching");
  } else {
    composit_->SetTouchUpMode(false);
    composit_->update();
    stitch_->setEnabled(false);
    touch_up_->setText("(T)ouch up");
  }
}

void MainWindow::PlayEnded() {
  play_button_->setText("Play"); 
  video_restart_ = true;
}

void MainWindow::CancelPlayEnded() {
  video_restart_ = false; 
}

void MainWindow::AnalyzeProgress(int frame) {
  static QString remaining_string;
    
  if (frame < 0) {
    // Done.
    analyze_pd_->setValue (analyze_pd_->maximum());
    
    // Return values are:
    // -1: success
    // -2: Tracking done. Graphcut got canceled.
    
    if (frame >= -2) { // Signals success or intermediate result.
      QPointF canvas_offset;
      QImage* canvas = video_analyzer_->ComputeAndAllocateNewCanvas(&canvas_offset);
      video_analyzer_->RetrieveFrameInfos(&frame_infos_);
      play_unit_->PassFrameInfos(&frame_infos_, canvas_offset);
      composit_->SetCanvas(canvas, canvas_offset);
      touch_up_->setEnabled(true);
      canvas_brush_->setEnabled(true);
      canvas_clean_->setEnabled(true);
    }
    
    StopVideo();
    analyse_button_->setEnabled(true);
    trace_canvas_->setChecked(true);
    TraceToggled(Qt::Checked);
    video_analyzer_.reset();
    return;
  }
  
  if (frame >= analyze_pd_->maximum())
    analyze_pd_->setMaximum(frame+1);
  
  analyze_pd_->setValue (frame);
  
  int msecs = analyze_time_.elapsed();
  if(frame >= 1) {
    int elapsed_s = msecs / 1000;
    int remaining_s = (msecs * analyze_pd_->maximum()  / frame) / 1000 - elapsed_s;
    QString elapsed_string;
    
    if (elapsed_s > 60)
      elapsed_string = QString::number(elapsed_s / 60) + ":"  + 
      QString::number(elapsed_s % 60).rightJustified(2,'0') + " min";
    else
      elapsed_string = QString::number(elapsed_s) + " s";

    // Don't update the predicted time too often.
    if (elapsed_s  <= 3) {
      remaining_string = "---";
    } else if (elapsed_s > 3 && frame % 20 == 0) {
      if (remaining_s > 60)
        remaining_string = QString::number(remaining_s / 60) + ":"  + 
        QString::number(remaining_s % 60).rightJustified(2,'0') + " min";
      else
        remaining_string = QString::number(remaining_s) + " s";
    } 
    
    QString standard = "Graphcut optimization...\nCancel now if not needed.";
    analyze_pd_->setLabelText(standard + "\nElapsed time: " + elapsed_string +
                              "\nApprox. remaining time: " + remaining_string);
  }
}

void MainWindow::DrawCanvasStateChanged(bool b) {
  trace_canvas_->setChecked(b);
}

void MainWindow::SaveProject() {
  if (composit_->get_canvas() == 0) {
    QMessageBox::critical(this, "", "There is no canvas to save");
    return;
  }
  
  QString save_filename = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                       tr(""),
                                                       tr("Project files (*.spf)"));
  
  if (save_filename.indexOf(".spf") < 0)
    save_filename += ".spf";
  
  QString img_file = save_filename.left(save_filename.lastIndexOf(".spf")) + ".png";  
  composit_->get_canvas()->convertToFormat(QImage::Format_RGB888).save(img_file, 0, 90);
  
  QString map_file = save_filename.left(save_filename.lastIndexOf(".spf")) + ".map";
  play_unit_->SaveCanvasFrameOrigin(map_file.toStdString());
  
  // Save Project file.
  std::ofstream proj_ofs (save_filename.toStdString().c_str(),
                         std::ios_base::out | std::ios_base::trunc);
  proj_ofs << video_file_.toStdString() << "\n";
  proj_ofs << composit_->get_canvas_offset().x() << " " << composit_->get_canvas_offset().y() 
           << "\n";
  proj_ofs << frame_spacing_slider_->value() << "\n";
  
  proj_ofs << frame_infos_.size() << "\n";
  for (int i = 0; i < frame_infos_.size(); ++i) 
    proj_ofs << frame_infos_[i];
}

void MainWindow::LoadProject() {
  QString load_filename = QFileDialog::getOpenFileName(this, tr("Load File"),
                                                       tr(""),   tr("Project files (*.spf)"));
  
  QString img_file = load_filename.left(load_filename.lastIndexOf(".spf")) + ".png";  
  
  QImage canvas_img_raw(img_file);
  QImage* canvas_img = new QImage(canvas_img_raw.convertToFormat(QImage::Format_ARGB32));

  // Load Project file.
  std::ifstream proj_ifs (load_filename.toStdString().c_str());
  string video_file;
  std::getline(proj_ifs, video_file);
  video_file_ = QString::fromStdString(video_file);
  
  QPointF offset;
  proj_ifs >> offset.rx() >> offset.ry();
  
  int frame_spacing;
  proj_ifs >> frame_spacing;
  frame_spacing_slider_->setValue(frame_spacing);
  FrameSpacingChanged(frame_spacing);
  
  LoadVideoImpl(video_file_);
  composit_->SetCanvas(canvas_img, offset);
  play_unit_->SetCanvasAlpha(0);
  
  int frame_info_num;
  FrameInfo fi;
  
  proj_ifs >> frame_info_num;
  frame_infos_.resize(frame_info_num);
  
  for (int i = 0; i < frame_info_num; ++i) {
    proj_ifs >> fi;
    frame_infos_[i] = fi;
  }
  
  play_unit_->PassFrameInfos(&frame_infos_, offset);
  QString map_file = load_filename.left(load_filename.lastIndexOf(".spf")) + ".map";
  play_unit_->LoadCanvasFrameOrigin(map_file.toStdString());
  
  composit_->update();
  
 // analyse_button_->setEnabled(false);
  touch_up_->setEnabled(true);
  canvas_brush_->setEnabled(true);
  canvas_clean_->setEnabled(true);  
}

void MainWindow::HideCanvas() {
  if (hide_canvas_->text() == "(H)ide Canvas") {
    hide_canvas_->setText("Un(h)ide Canvas");
    play_unit_->SetCanvasAlpha(0);
  } else {
    hide_canvas_->setText("(H)ide Canvas");
    play_unit_->SetCanvasAlpha(128);
  }
}



void MainWindow::CanvasBrush() {
  if (canvas_brush_->text() == "Canvas (B)rush") {
    canvas_brush_->setText("Done (B)rusing");
    stitch_->setText("(R)emove");
    stitch_->setEnabled(true);
  } else {
    canvas_brush_->setText("Canvas (B)rush");    
    stitch_->setText("(S)titch");
    stitch_->setEnabled(false);
  }
}

void MainWindow::Stitch() {
  if (stitch_->text() == "(S)titch") {
    play_unit_->Stitch();
  } else if (stitch_->text() == "(R)emove") {
    
  }
}

void MainWindow::keyPressEvent(QKeyEvent * event) {
  switch(event->key()) {
    case Qt::Key_Right:
      if (video_reader_)
        video_reader_->NextFrame();
      break;
      
    case Qt::Key_Left:
      if (video_reader_) 
        video_reader_->PrevFrame();
      break;
      
    case Qt::Key_Up:
      if (video_reader_)
        video_reader_->SkipFrames(10);
      break;
      
    case Qt::Key_Space:
      if (video_reader_)
        PlayVideo();
        break;
      
    case Qt::Key_Down:
      if (video_reader_) 
        video_reader_->SkipFrames(-10);
      break;
      
    case Qt::Key_Tab:
      composit_->set_disp_mode((composit_->get_disp_mode() + 1) % 3);
      composit_->update();
      break;
      
    case Qt::Key_A:
      if (video_reader_)
        AnalyzeVideo();
      break;
      
    case Qt::Key_F:
      selection_mode_->button(0)->animateClick();
      break;
    
    case Qt::Key_C:
      selection_mode_->button(1)->animateClick();
      break;
      
    case Qt::Key_E:
      selection_mode_->button(2)->animateClick();
      break;
      
    case Qt::Key_T:
      if (touch_up_->isEnabled())
        touch_up_->animateClick();
      break;
      
    case Qt::Key_S:
      if (stitch_->isEnabled()) 
        stitch_->animateClick();
      break;
      
    case Qt::Key_B:
      if (canvas_brush_->isEnabled())
        canvas_brush_->animateClick();
      break;
      
    case Qt::Key_H:
      if (hide_canvas_->isEnabled())
        hide_canvas_->animateClick();
      break;
      
    case Qt::Key_D:
      composit_->DrawBufferedImage(!composit_->IsDrawBufferedImage());
      composit_->update();
      break;
      
    default:
      QMainWindow::keyPressEvent(event);
  }
}