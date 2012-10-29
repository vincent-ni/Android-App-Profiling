#include "mainWindow.h"

#include <QtGui>
#include <QString>
#include <iostream>
#include <fstream>

#include "assert_log.h"
#include "video_data.h"
// #include "SurfaceCarve.h"

#include <cmath>

MainWindow::MainWindow() : carve_progress_dialog_(0),
                           // carving_(0),
                           max_temporal_(200),
                           max_saliency_(200),
                           max_forward_(200) {
	setWindowTitle("Video Segmentation UI - for academic use only!");

  // Main layout.
  QHBoxLayout* main_hbox = new QHBoxLayout;
  QWidget* main = new QWidget ();
  main->setLayout(main_hbox);
  setCentralWidget(main);

  QVBoxLayout* main_vbox = new QVBoxLayout;
  QWidget* main_vbox_widget = new QWidget;
  main_vbox_widget->setLayout(main_vbox);
  main_hbox->addWidget(main_vbox_widget);

  // Header.
	QPushButton* exit = new QPushButton(tr("Exit") );
  FormatButton(exit);
	QPushButton* load = new QPushButton(tr("Load Video"));
  FormatButton(load);
  QPushButton* load_segmentation = new QPushButton(tr("Load Seg."));
  FormatButton(load_segmentation);
  QPushButton* load_saliency = new QPushButton(tr("Load Saliency"));
  FormatButton(load_saliency);

	play_button_ = new QPushButton(tr("Play"));
  FormatButton(play_button_);
	QPushButton* carve = new QPushButton (tr("Carve"));
  FormatButton(carve);
  carve->setEnabled(false);

	QGridLayout* header_layout = new QGridLayout;
	header_layout->addWidget(load, 0, 0);
  header_layout->addWidget(load_saliency, 1, 0);
  header_layout->addWidget(load_segmentation, 1, 1);
	header_layout->addWidget(play_button_, 0, 1);
	header_layout->addWidget(carve, 0, 2);
	header_layout->addWidget(exit, 0, 4);

  QLabel* alpha_label = new QLabel("Alpha:");
  FormatWidget(alpha_label);
  header_layout->addWidget(alpha_label, 1, 3);

  QPushButton* save_vid = new QPushButton("Save");
  FormatButton(save_vid);
  header_layout->addWidget(save_vid, 0, 3);

  alpha_slider_ = new QSlider(Qt::Horizontal);
  alpha_slider_->setMinimum(0);
  alpha_slider_->setMaximum(100);
  alpha_slider_->setTickPosition(QSlider::TicksBelow);
  alpha_slider_->setValue(50);
  header_layout->addWidget(alpha_slider_, 1, 4, 1, 2);
  header_layout->setColumnStretch(5, 10);

  connect(save_vid, SIGNAL(clicked()), this, SLOT(SaveVideo()));

	QGridLayout* grid_layout = new QGridLayout;

	cur_width_label_ = new QLabel ("Width: 100%");
  FormatWidget(cur_width_label_);
  header_layout->addWidget(cur_width_label_, 0, 5);

  QPushButton* reset_view = new QPushButton("Reset View");
  FormatButton(reset_view);
  header_layout->addWidget(reset_view, 1, 2);

	QWidget* header_grid = new QWidget;
	header_grid->setLayout(grid_layout);

	QWidget* header = new QWidget;
	header->setLayout(header_layout);
  main_vbox->addWidget(header);

  connect(exit, SIGNAL(clicked()), qApp, SLOT(quit()));
	connect(load, SIGNAL(clicked()), this, SLOT(LoadFile()));
  connect(load_saliency, SIGNAL(clicked()), this, SLOT(LoadSaliency()));
	connect(play_button_, SIGNAL(clicked()), this, SLOT(PlayVideo()));
	connect(carve, SIGNAL(clicked()), this, SLOT(CarveVideo()));
  connect(load_segmentation, SIGNAL(clicked()), this, SLOT(LoadSegmentation()));

  // Movie widget.
  movie_widget_ = new MovieWidget(this, cur_width_label_);
  QWidget* mid = new QWidget;
  QHBoxLayout* mid_hbox = new QHBoxLayout;
  mid->setLayout(mid_hbox);
  main_vbox->addWidget(mid);
  mid_hbox->addWidget(movie_widget_);
  mid_hbox->addStretch();

  connect(reset_view, SIGNAL(clicked()), movie_widget_, SLOT(ResetView()));
  connect(alpha_slider_, SIGNAL(sliderMoved(int)), movie_widget_, SLOT(SetAlpha(int)));

  // Bottom part.
  QHBoxLayout* bottom_hbox = new QHBoxLayout;
  QWidget* bottom = new QWidget;
  bottom->setLayout(bottom_hbox);

  bottom_hbox->addStretch();
	main_vbox->addStretch ();

  // Preference Tab.
  QTabWidget* pref_tabs = new QTabWidget(this);
  main_hbox->addStretch();
  main_hbox->addWidget(pref_tabs);

  // Segmentation Tab.
  QWidget* seg_prefs = new QWidget;
  QVBoxLayout* seg_pref_vbox = new QVBoxLayout;
  seg_pref_vbox->setContentsMargins(11, 5, 11, 0);
  seg_prefs->setLayout(seg_pref_vbox);

  pref_tabs->addTab(seg_prefs, "Segment");
  pref_tabs->setTabEnabled(1, true);

  // Segmentation Level.
  QGroupBox* seg_level_part = new QGroupBox("Overall Hierarchy Level:");
  FormatWidget(seg_level_part);
  QHBoxLayout* seg_level_hbox = new QHBoxLayout;
  seg_level_hbox->setContentsMargins(11, 5, 11, 5);
  seg_level_part->setLayout(seg_level_hbox);
  seg_pref_vbox->addWidget(seg_level_part);

  level_label_ = new QLabel("0");
  FormatWidget(level_label_);
  level_label_->setFont(QFont("Monaco",12));
  seg_level_hbox->addWidget(level_label_);

  level_slider_ = new QSlider(Qt::Horizontal);
  level_slider_->setMinimum(0);
  level_slider_->setMaximum(0);
  level_slider_->setTickPosition(QSlider::TicksBelow);
  seg_level_hbox->addWidget(level_slider_);

  connect(level_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SetLevel(int)));

  // Brushing.
  QGridLayout* seg_brush_grid = new QGridLayout;
  QGroupBox* seg_brush_part = new QGroupBox("Region brushing:");
  FormatWidget(seg_brush_part);
  seg_brush_part->setLayout(seg_brush_grid);
  seg_brush_grid->setContentsMargins(11, 5, 11, 5);

  seg_pref_vbox->addWidget(seg_brush_part);

  brush_pos_ = new QPushButton("Brush +");
  FormatButton(brush_pos_);
  brush_neg_ = new QPushButton("Brush -");
  FormatButton(brush_neg_);
  brush_erase_ = new QPushButton("Eraser");
  FormatButton(brush_erase_);
  brush_reset_ = new QPushButton("Reset");
  FormatButton(brush_reset_);

  seg_brush_grid->addWidget(brush_pos_, 0, 0);
  seg_brush_grid->addWidget(brush_neg_, 0, 1);
  seg_brush_grid->addWidget(brush_erase_, 1, 0);
  seg_brush_grid->addWidget(brush_reset_, 1, 1);

  connect(brush_pos_, SIGNAL(clicked()), movie_widget_, SLOT(BrushPos()));
  connect(brush_neg_, SIGNAL(clicked()), movie_widget_, SLOT(BrushNeg()));
  connect(brush_erase_, SIGNAL(clicked()), movie_widget_, SLOT(BrushErase()));
  connect(brush_reset_, SIGNAL(clicked()), movie_widget_, SLOT(BrushReset()));

  // Local Hierarchy selection.
  QGroupBox* seg_local_level_part = new QGroupBox("Local Level:");
  FormatWidget(seg_local_level_part);
  QVBoxLayout* seg_local_level_vbox = new QVBoxLayout;
  seg_local_level_vbox->setContentsMargins(11, 5, 11, 5);
  seg_local_level_part->setLayout(seg_local_level_vbox);
  seg_pref_vbox->addWidget(seg_local_level_part);

  local_level_slider_ = new QSlider(Qt::Horizontal);
  local_level_slider_->setMinimum(-10);
  local_level_slider_->setMaximum(10);
  local_level_slider_->setTickPosition(QSlider::TicksBelow);
  local_level_slider_->setValue(0);
  local_level_slider_->setEnabled(false);
  seg_local_level_vbox->addWidget(local_level_slider_);

  connect(movie_widget_, SIGNAL(LocalHierarchyEnabled(int, int)), this,
          SLOT(SetLocalHierarchySlider(int, int)));

  connect(movie_widget_, SIGNAL(RegionsDeselected()), this,
          SLOT(DisableHierarchySlider()));
  connect(local_level_slider_, SIGNAL(sliderMoved(int)),
          movie_widget_, SLOT(AdjustLocalLevel(int)));
  connect(local_level_slider_, SIGNAL(sliderMoved(int)),
          this, SLOT(LocalLevelChanged(int)));

  local_level_label_ = new QLabel("Level adjust: n/a");
  seg_local_level_vbox->addWidget(local_level_label_);

  // Color seed.
  QGroupBox* seg_color_seed_part = new QGroupBox("Color Seed:");
  FormatWidget(seg_color_seed_part);
  QVBoxLayout* seg_color_seed_vbox = new QVBoxLayout;
  seg_color_seed_vbox->setContentsMargins(11, 5, 11, 5);
  seg_color_seed_part->setLayout(seg_color_seed_vbox);
  seg_pref_vbox->addWidget(seg_color_seed_part);

  QSlider* color_seed_slider = new QSlider(Qt::Horizontal);
  color_seed_slider->setMinimum(0);
  color_seed_slider->setMaximum(50);
  color_seed_slider->setTickPosition(QSlider::TicksBelow);
  color_seed_slider->setValue(0);
  seg_color_seed_vbox->addWidget(color_seed_slider);

  connect(color_seed_slider, SIGNAL(sliderMoved(int)),
          this, SLOT(ColorSeedChanged(int)));

  color_seed_label_ = new QLabel("Seed: 0");
  seg_color_seed_vbox->addWidget(color_seed_label_);

  // Tooning.
  QGroupBox* seg_misc_part = new QGroupBox("Misc:");
  FormatWidget(seg_misc_part);
  QVBoxLayout* seg_misc_vbox = new QVBoxLayout;
  seg_misc_vbox->setContentsMargins(11, 5, 11, 5);
  seg_misc_part->setLayout(seg_misc_vbox);


  toon_ = new QPushButton("Toon");
  FormatButton(toon_);
  seg_misc_vbox->addWidget(toon_);
  connect(toon_, SIGNAL(clicked()), movie_widget_, SLOT(Toon()));

  load_with_segmentation_ = new QCheckBox("Load with segmentation");
  load_with_segmentation_->setCheckState(Qt::Checked);
  seg_misc_vbox->addWidget(load_with_segmentation_);

  seg_pref_vbox->addWidget(seg_misc_part);
  seg_pref_vbox->addStretch();

  // Segmentation insights controls.
  QWidget* insight_prefs = new QWidget;
  QVBoxLayout* insight_pref_vbox = new QVBoxLayout;
  insight_pref_vbox->setContentsMargins(11, 5, 11, 0);
  insight_prefs->setLayout(insight_pref_vbox);

  pref_tabs->addTab(insight_prefs, "Insights");

  // Hierarchy visualization
  QGroupBox* hierarchy_viz_part = new QGroupBox("Hierarchy visualization:");
  FormatWidget(hierarchy_viz_part);
  QVBoxLayout* hierarchy_viz_vbox = new QVBoxLayout;
  hierarchy_viz_vbox->setContentsMargins(11, 5, 11, 5);
  hierarchy_viz_part->setLayout(hierarchy_viz_vbox);
  insight_pref_vbox->addWidget(hierarchy_viz_part);

  viz_neighbors_ = new QCheckBox("Visualize neighbors");
  viz_neighbors_->setCheckState(Qt::Unchecked);
  hierarchy_viz_vbox->addWidget(viz_neighbors_);

  viz_children_ = new QCheckBox("Visualize children");
  viz_children_->setCheckState(Qt::Unchecked);
  hierarchy_viz_vbox->addWidget(viz_children_);

  connect(viz_neighbors_, SIGNAL(stateChanged(int)), movie_widget_,
          SLOT(VisualizeNeighbors(int)));
  connect(viz_children_, SIGNAL(stateChanged(int)), movie_widget_,
          SLOT(VisualizeChildren(int)));

  // Descriptors insights.
  QGroupBox* descriptor_viz_part = new QGroupBox("Descriptor visualization:");
  FormatWidget(descriptor_viz_part);
  QVBoxLayout* descriptor_viz_vbox = new QVBoxLayout;
  descriptor_viz_vbox->setContentsMargins(11, 5, 11, 5);
  descriptor_viz_part->setLayout(descriptor_viz_vbox);
  insight_pref_vbox->addWidget(descriptor_viz_part);

  QPushButton* plot_app_1 = new QPushButton("Plot appearance +");
  descriptor_viz_vbox->addWidget(plot_app_1);

  QPushButton* plot_app_2 = new QPushButton("Plot appearance -");
  descriptor_viz_vbox->addWidget(plot_app_2);

  insight_pref_vbox->addStretch();

  // Seam carving preferences.
  QWidget* seam_prefs = new QWidget;
  QVBoxLayout* seam_pref_vbox = new QVBoxLayout;
  seam_pref_vbox->setContentsMargins(11, 5, 11, 0);
  seam_prefs->setLayout(seam_pref_vbox);

  // pref_tabs->addTab(seam_prefs, "Carving");
  seam_prefs->setEnabled(false);

  // Seam percentage.
  QGroupBox* seam_percentage_part = new QGroupBox("Seams:");
  FormatWidget(seam_percentage_part);
  QHBoxLayout* seam_percentage_hbox = new QHBoxLayout;
  seam_percentage_hbox->setContentsMargins(11, 5, 11, 5);
  seam_percentage_part->setLayout(seam_percentage_hbox);
  seam_pref_vbox->addWidget(seam_percentage_part);

  carve_label_ = new QLabel("30 %");
  FormatWidget(carve_label_);
  carve_label_->setFont(QFont("Monaco", 12));
	carve_amount_ = 30;
  seam_percentage_hbox->addWidget(carve_label_);

	carve_slider_ = new QSlider (Qt::Horizontal);
	carve_slider_->setTickPosition ( QSlider::TicksBelow );
	carve_slider_->setMinimum(1);
	carve_slider_->setMaximum(50);
	carve_slider_->setValue (carve_amount_);

	connect(carve_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SetCarveAmount(int)));
  seam_percentage_hbox->addWidget(carve_slider_);

  // Seam Parameters.
  QGridLayout* seam_param_grid = new QGridLayout;
  QGroupBox* seam_param_part = new QGroupBox("Weights");
  FormatWidget(seam_param_part);
  seam_param_part->setLayout(seam_param_grid);
  seam_param_grid->setContentsMargins(11, 5, 11, 5);

  seam_pref_vbox->addWidget(seam_param_part);

  forward_slider_ = new QSlider(Qt::Vertical);
  forward_slider_->setMinimum(0);
  forward_slider_->setMaximum(max_forward_);

  temporal_slider_ = new QSlider(Qt::Vertical);
  temporal_slider_->setMinimum(0);
  temporal_slider_->setMaximum(max_temporal_);

  saliency_slider_ = new QSlider(Qt::Vertical);
  saliency_slider_->setMinimum(0);
  saliency_slider_->setMaximum(max_saliency_);

  seam_param_grid->addWidget(forward_slider_, 0, 0, Qt::AlignHCenter);
  seam_param_grid->addWidget(temporal_slider_, 0, 1, Qt::AlignHCenter);
  seam_param_grid->addWidget(saliency_slider_, 0, 2, Qt::AlignHCenter);

  forward_label_ = new QLabel();
  temporal_label_ = new QLabel();
  saliency_label_ = new QLabel();
  forward_label_->setFont(QFont("Monaco",12));
  FormatWidget(forward_label_);
  saliency_label_->setFont(QFont("Monaco",12));
  FormatWidget(saliency_label_);
  temporal_label_->setFont(QFont("Monaco",12));
  FormatWidget(temporal_label_);

  seam_param_grid->addWidget(forward_label_, 1, 0);
  seam_param_grid->addWidget(temporal_label_, 1, 1);
  seam_param_grid->addWidget(saliency_label_, 1, 2);

  connect(forward_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SetForward(int)));
  connect(temporal_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SetTemporal(int)));
  connect(saliency_slider_, SIGNAL(sliderMoved(int)), this, SLOT(SetSaliency(int)));

  int forward_pos = pow(1.0 / 200.0, 1.0 / 5.0) * max_forward_ + 0.5;
  forward_slider_->setValue(forward_pos);
  SetForward(forward_pos);

  int temporal_pos = pow(1.0 / 200.0, 1.0 / 5.0) * max_temporal_ + 0.5;
  temporal_slider_->setValue(temporal_pos);
  SetTemporal(temporal_pos);

  int saliency_pos = pow(1.0 / 200.0, 1.0 / 5.0) * max_saliency_ + 0.5;
  saliency_slider_->setValue(saliency_pos);
  SetSaliency(saliency_pos);

  // Neighbors.
  QGroupBox* seam_neighbor_part = new QGroupBox("Neighbors:");
  FormatWidget(seam_neighbor_part);
  QHBoxLayout* seam_neighbor_hbox = new QHBoxLayout;
  seam_neighbor_hbox->setContentsMargins(11, 5, 11, 5);
  seam_neighbor_part->setLayout(seam_neighbor_hbox);
  seam_pref_vbox->addWidget(seam_neighbor_part);

  QSlider* neighbor_slider = new QSlider(Qt::Horizontal);
  neighbor_label_ = new QLabel(QString::number(3).rightJustified(3));
  FormatWidget(neighbor_label_);
  neighbor_label_->setFont(QFont("Monaco",12));
  neighbor_window_ = 3;

  neighbor_slider->setValue(0);
  neighbor_slider->setMinimum(0);
  neighbor_slider->setMaximum(30);

  seam_neighbor_hbox->addWidget(neighbor_slider);
  seam_neighbor_hbox->addWidget(neighbor_label_);

  connect(neighbor_slider, SIGNAL(sliderMoved(int)), this, SLOT(SetNeighborWindow(int)));

  // Misc.
  QGroupBox* seam_misc_part = new QGroupBox("Misc:");
  FormatWidget(seam_misc_part);
  QVBoxLayout* seam_misc_vbox = new QVBoxLayout;
  seam_misc_vbox->setContentsMargins(11, 5, 11, 5);
  seam_misc_part->setLayout(seam_misc_vbox);
  seam_pref_vbox->addWidget(seam_misc_part);

  blend_mode_check_ = new QCheckBox("Blend Seams");
  FormatWidget(blend_mode_check_);
  blend_mode_ = false;
  blend_mode_check_->setCheckState(Qt::Unchecked);
  seam_misc_vbox->addWidget(blend_mode_check_);
  connect(blend_mode_check_, SIGNAL(stateChanged(int)), this, SLOT(BlendChanged(int)));

  seam_pref_vbox->addStretch();
}

MainWindow::~MainWindow () {
}

void MainWindow::LoadFile() {
	QString file_name = QFileDialog::getOpenFileName(this,
                                                   tr("Load Video"), "videos",
                                                   tr("Videos (*.avi *.wmv *.flv *.mpeg "
                                                      "*.mpg *.mov)"));

	if (file_name.length() != 0) {
		std::string file = file_name.toLocal8Bit().data();
		LOG(INFO) << "Loading video file " << file;

		shared_ptr<VideoData> data(new VideoData ());
		if (!data->LoadFromFile(file)) {
      QMessageBox::critical (this, this->windowTitle(),
                             "Could not load file. Check console for error.");
			return;
		}

    segmentation_.clear();
    movie_widget_->SetSegmentation(0);
    level_slider_->setMaximum(0);
    level_slider_->setValue(0);

		// Display first image.
		movie_widget_->SetVideo(data);
    video_file_ = file_name;

    if (load_with_segmentation_->checkState() == Qt::Checked) {
      std::string file_base = file.substr(0, file.find_last_of("."));
      std::string segment_file = file_base + "_segment.pb";
      LoadSegmentationImpl(segment_file);
    }
  }
}

void MainWindow::LoadSaliency() {
  if (!movie_widget_->GetVideo()) {
		QMessageBox::information (this, windowTitle(), "Which video?");
		return;
	}

  QString file_name = QFileDialog::getOpenFileName(this,
                                                   tr("Load Saliency"),
                                                   "videos",
                                                   tr("Videos (*.avi *.wmv *.mpeg"
                                                      "*.mpg *.mov)"));

	if (file_name.length() != 0 ) {
		std::string file = file_name.toLocal8Bit().data();
		LOG(INFO) << "Loading saliency file " << file << std::endl;

		shared_ptr<VideoData> data(new VideoData());
		if (!data->LoadFromFile(file)) {
			QMessageBox::critical (this, this->windowTitle(),
                             "Could not load file. Check console for error.");
			return;
		}

    saliency_ = data;
  }
}

void MainWindow::LoadSegmentation() {
  if (!movie_widget_->GetVideo()) {
		QMessageBox::information (this, windowTitle(), "Which video?" );
		return;
	}

  QString fileName = QFileDialog::getOpenFileName(this, tr("Load Video Segmentation"),
                                                  "videos",
                                                  tr("Segmentation Protobuffer (*.pb)"));

	if ( fileName.length() != 0 ) {
		std::string file = fileName.toLocal8Bit().constData();
    LoadSegmentationImpl(file);
  }
}

void MainWindow::LoadSegmentationImpl(const std::string& file) {
  std::cout << "Loading segmentation " << file << std::endl;

  segmentation_.clear();
  segment_reader_.reset(new SegmentationReader(file));
  segment_reader_->OpenFileAndReadHeaders();

  if (segment_reader_->NumFrames() > movie_widget_->GetVideo()->GetFrames()) {
    QMessageBox::critical(this, this->windowTitle(),
                          QString("Segmentation file is not compatible with video.\n") +
                          QString::number(movie_widget_->GetVideo()->GetFrames()) +
                          " vs. " +
                          QString::number(segment_reader_->NumFrames()));
    return;
  }

  if (segment_reader_->NumFrames() < movie_widget_->GetVideo()->GetFrames()) {
    if (QMessageBox::question(this, this->windowTitle(),
                              QString("Video too long for segmentation. Trim it?"),
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
      movie_widget_->TrimVideoTo(segment_reader_->NumFrames());
    } else {
      return;
    }
  }

  vector<uchar> frame_data;
  for (int i = 0; i < segment_reader_->NumFrames(); ++i) {
    const int frame_sz = segment_reader_->ReadNextFrameSize();
    frame_data.resize(frame_sz);
    segment_reader_->ReadNextFrame(&frame_data[0]);

    shared_ptr<SegmentationDesc> seg_ptr(new SegmentationDesc);
    seg_ptr->ParseFromArray(&frame_data[0], frame_sz);
    segmentation_.push_back(seg_ptr);
  }

  level_slider_->setMaximum(segmentation_[0]->hierarchy_size() - 1);
  movie_widget_->SetSegmentation(&segmentation_);
  SetLevel(segmentation_[0]->hierarchy_size() * 0.5);
}

void MainWindow::PlayVideo () {
	if ( play_button_->text () == "Play") {
		movie_widget_->Animate(true);
		play_button_->setText("Pause");
	} else {
		movie_widget_->Animate(false);
		play_button_->setText ("Play");
	}
}

void MainWindow::SetCarveAmount (int num)
{
	carve_amount_ = num;
	carve_label_->setText(QString::number(num).leftJustified(2) + " %" );
}

void MainWindow::SetForward(int n) {
  // Should be between 0 and 2.
  forward_weight_ = std::pow((float)n / max_forward_, 5) * 200;
  forward_label_->setText(QString("SptalC\n") +
                          QString::number(forward_weight_, 'f',  2).rightJustified(6));
}

void MainWindow::SetTemporal(int n) {
  // Should be between 0 and 5.
  temporal_weight_ = std::pow((float)n / max_temporal_, 5) * 200;
  temporal_label_->setText(QString("TemplC\n") +
                           QString::number(temporal_weight_, 'f',  2).rightJustified(6));
}

void MainWindow::SetSaliency(int n) {
  saliency_weight_ = std::pow((float)n / max_saliency_, 5) * 200;
  saliency_label_->setText(QString("Salncy\n") +
                           QString::number(saliency_weight_, 'f', 2).rightJustified(6));
}

void MainWindow::SetLevel(int n) {
  level_label_->setText(QString::number(n).leftJustified(3));
  hierarchy_level_ = n;
  level_slider_->setValue(hierarchy_level_);
  movie_widget_->SetLevel(hierarchy_level_);
}

void MainWindow::LocalLevelChanged(int l) {
  local_level_label_->setText(QString("Level adjust: ") + QString::number(l));
}

void MainWindow::ColorSeedChanged(int s) {
  color_seed_label_->setText(QString("Seed: ") + QString::number(s));
  movie_widget_->SetColorShuffle(s);
}

void MainWindow::SaveVideo() {
  if (movie_widget_->GetVideo()) {

    // Animate MovieWidget, grab frame and save it to file
    movie_widget_->Animate(false);

    int frame_width = movie_widget_->GetScaledWidth();
    int frame_height = movie_widget_->GetScaledHeight();
    int frames = movie_widget_->GetVideo()->GetFrames();

    QProgressDialog progress("Saving files...", "Cancel", 0, frames, this);
    progress.setWindowModality(Qt::WindowModal);

    for (int t = 0; t < frames; ++t) {
      progress.setValue(t);
      if (progress.wasCanceled())
        break;

      movie_widget_->SetToFrame(t);
      movie_widget_->repaint();
      movie_widget_->update();
      qApp->processEvents();
      const IplImage* output_img = movie_widget_->GetOutputImg();
      QImage img((const uchar*)output_img->imageData,
                 output_img->width,
                 output_img->height,
                 QImage::Format_RGB32);
      img.save(QString("out_frame") + QString::number(t).rightJustified(4, '0') + ".png",
               "png");
    }

    progress.setValue(frames);
  }
}

void MainWindow::SetNeighborWindow(int n) {
  neighbor_window_ = 2 + 2*n+1;
  neighbor_label_->setText(QString::number(neighbor_window_).rightJustified(3));
  movie_widget_->repaint();
}

void MainWindow::BlendChanged(int state) {
  blend_mode_ = state == Qt::Checked;
}

void MainWindow::SetLocalHierarchySlider(int min, int max) {
  local_level_slider_->setMinimum(min);
  local_level_slider_->setMaximum(max);
  local_level_slider_->setValue(0);
  local_level_slider_->setEnabled(true);
  local_level_label_->setText("Level adjust: 0");
}

void MainWindow::DisableHierarchySlider() {
  local_level_slider_->setMinimum(0);
  local_level_slider_->setMaximum(0);
  local_level_slider_->setValue(0);
  local_level_slider_->setEnabled(false);
  local_level_label_->setText("Level adjust: n/a");
}

void MainWindow::CarveVideo () {
  /*
	if (!movie_widget_->GetVideo())	{
		QMessageBox::information (this, windowTitle(), "Which video?" );
		return;
	}

  movie_widget_->SetCarving(0);

  // Does flow file exists?
  QString flow_file = video_file_.left(video_file_.lastIndexOf(".")) + ".flow";

  if(!QFile(flow_file).exists())
    flow_file = "";
  else if (QMessageBox::question(this, "", "Optical flow available. Use it?",
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
    flow_file = "";
  }

	carving_ = new SurfaceCarve (movie_widget_->GetVideo(), carve_amount_, temporal_weight_,
                               forward_weight_, saliency_weight_, blend_mode_, neighbor_window_,
                               flow_file);

	carve_progress_dialog_ = new QProgressDialog("Seam Carving in progress ... ",
		                        "Cancel", 0, movie_widget_->GetVideo()->getFrames());

  carve_progress_dialog_->setWindowModality(Qt::WindowModal);
  carve_progress_dialog_->setMinimumDuration(0);

  connect(carve_progress_dialog_, SIGNAL(canceled()), carving_, SLOT(CancelJob()));
  connect(carving_, SIGNAL(done(int)), this, SLOT(CarveProgress(int)));

  if (saliency_) {
    carving_->SetSaliencyMap(saliency_);
    movie_widget_->BrushReset();
  } else {
    // Do we have user input?
    if (movie_widget_->IsUserInput()) {
      carving_->SetUserInput(movie_widget_);
    }
  }

  carve_start_time_.start();
	carving_->start();
   */
}

void MainWindow::CarveProgress(int p)
{
  const int frames = movie_widget_->GetVideo()->GetFrames();
	if (carve_progress_dialog_) {
    int msecs = carve_start_time_.elapsed();
		carve_progress_dialog_->setValue (p);

    if(p >= 1) {
      int elapsed_s = msecs / 1000;
      int remaining_s = msecs / 1000 * frames / p - elapsed_s;
      QString elapsed_string;
      QString remaining_string;

      if (elapsed_s > 60)
        elapsed_string = QString::number(elapsed_s / 60) + ":"  +
                         QString::number(elapsed_s % 60).rightJustified(2,'0') + " min";
      else
        elapsed_string = QString::number(elapsed_s) + " s";

      if (remaining_s > 60)
        remaining_string = QString::number(remaining_s / 60) + ":"  +
                           QString::number(remaining_s % 60).rightJustified(2,'0') + " min";
      else
        remaining_string = QString::number(remaining_s) + " s";


      QString standard = "Seam Carving in progress ... Please do not interact with software.";
      carve_progress_dialog_->setLabelText(standard + "\nElapsed time: " + elapsed_string +
                                           "\nApprox. remaining time: " + remaining_string);
    }
	}
/*
	if (carving_ && p == frames) {
    // Signal carving object is ready to use.
		movie_widget_->SetCarving (carving_);
	}
*/
}

void MainWindow::FormatWidget(QWidget* widget) {
  return;
  QPalette pal = widget->palette();
  pal.setColor(QPalette::Button, qRgb(80, 80, 80));
  pal.setColor(QPalette::Background, qRgb(80, 80, 80));
  pal.setColor(QPalette::Foreground, qRgb(255, 255, 255));
  pal.setColor(QPalette::ButtonText, qRgb(255, 255, 255));
  pal.setColor(QPalette::Text, qRgb(255, 255, 255));

  widget->setPalette(pal);
}

void MainWindow::FormatButton(QPushButton* button) {
  return;
  button->setStyleSheet("QPushButton { "
                        "border: 1px solid #a0a0a0;"
                        "border-radius: 8px;"
                        "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
                        "stop: 0 #606060, stop: 1 #505050);"
                        "min-width: 100px;"
                        "margin-left: .5ex;"
                        "margin-right: .5ex;"
                        "color: #ffffff;"
                        "}"

                        "QPushButton:pressed {"
                        "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
                        "                                   stop: 0 #dadbde, stop: 1 #f6f7fa);"
                        "  color: #000000;"
                        "}"
                        );
}

void MainWindow::keyPressEvent(QKeyEvent * event) {
  switch(event->key()) {
    case Qt::Key_Space:
      PlayVideo();
      break;
    case Qt::Key_H:
      movie_widget_->HideUnselectedRegions();
      break;
    case Qt::Key_B:
      movie_widget_->DrawBorders();
      break;
    case Qt::Key_S:
      movie_widget_->DrawShapeDescriptors();
      break;
    case Qt::Key_Left:
      movie_widget_->SkipFrame(-1);
      break;
    case Qt::Key_Right:
      movie_widget_->SkipFrame(1);
      break;
  }
}
