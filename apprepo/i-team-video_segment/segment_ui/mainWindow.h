
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QProgressDialog>

#include <vector>
using std::vector;
#include <boost/scoped_ptr.hpp>
using boost::scoped_ptr;
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include "movieWidget.h"
#include "segmentation.pb.h"
using Segment::SegmentationDesc;
#include "segmentation_io.h"
using Segment::SegmentationReader;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow ();
	~MainWindow ();

public slots:
	void LoadFile ();
  void LoadSaliency();
  void LoadSegmentation();
	void PlayVideo ();
	void CarveVideo ();
	void SetCarveAmount (int);
	void CarveProgress(int);

  void SetForward(int);
  void SetTemporal(int);
  void SetSaliency(int);
  void SetLevel(int);
  void LocalLevelChanged(int);
  void ColorSeedChanged(int);

  void SetNeighborWindow(int);

  void SaveVideo();
  void BlendChanged(int);

  void SetLocalHierarchySlider(int min, int max);
  void DisableHierarchySlider();
protected:
  void FormatWidget(QWidget* widget);
  void FormatButton(QPushButton* widget);
  void keyPressEvent(QKeyEvent * event);

  void LoadSegmentationImpl(const std::string& file);
protected:

	MovieWidget* movie_widget_;
	QPushButton* play_button_;
	QSlider* carve_slider_;
	QLabel*	carve_label_;

	QSlider* surf_length_slider_;
	QLabel*	cur_width_label_;

  QSlider* forward_slider_;
  QSlider* temporal_slider_;
  QSlider* saliency_slider_;

  QLabel* forward_label_;
  QLabel* temporal_label_;
  QLabel* saliency_label_;

  QSlider* level_slider_;
  QLabel* level_label_;
  QLabel* neighbor_label_;

  QSlider* local_level_slider_;
  QLabel* local_level_label_;
  QLabel* color_seed_label_;

  QCheckBox*  blend_mode_check_;

  QSlider* alpha_slider_;
  QPushButton* brush_pos_;
  QPushButton* brush_neg_;
  QPushButton* brush_erase_;
  QPushButton* brush_reset_;
  QPushButton* toon_;
  QCheckBox* load_with_segmentation_;

  QCheckBox* viz_children_;
  QCheckBox* viz_neighbors_;

	QProgressDialog*  carve_progress_dialog_;
  QTime carve_start_time_;

	// scoped_ptr<SurfaceCarve>	carving_;
  shared_ptr<VideoData> saliency_;

  scoped_ptr<SegmentationReader> segment_reader_;
  vector<shared_ptr<SegmentationDesc> > segmentation_;

	int carve_amount_;
	int	surf_length_;
  int hierarchy_level_;
  bool blend_mode_;
  int neighbor_window_;

  float forward_weight_;
  float temporal_weight_;
  float  saliency_weight_;

  const int max_temporal_;
  const int max_saliency_;
  const int max_forward_;
  QString video_file_;
};

#endif
