
#ifndef MOVIEWIDGET_H
#define MOVIEWIDGET_H

#include <QWidget>
#include <QThread>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QTime>

#include "video_data.h"

class MovieWidget;
// class SurfaceCarve;

#include <vector>
using std::vector;
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <boost/shared_array.hpp>
using boost::shared_array;

#include "segmentation_util.h"
#include "segmentation_render.h"
using namespace Segment;

#include <ext/hash_map>
using __gnu_cxx::hash_map;

//#include <ext/hash_fun.h>
#include <boost/lexical_cast.hpp>

#include <string>
using std::string;

class MovieWidget : public QWidget {
	Q_OBJECT
public:
	MovieWidget (QWidget* parent, QLabel* width_label);
	~MovieWidget ();

	QSizePolicy sizePolicy () const {
    return QSizePolicy ( QSizePolicy::Fixed, QSizePolicy::Fixed );
  }
	QSize sizeHint () const {
    return QSize (sx_ + border_ + velo_scale_->size().width(),
                  sy_ + std::max(border_,
                                 5 + movie_pos_->size().height()));
  }

	void SetVideo(shared_ptr<VideoData> video_data);
	void ScaleSize(int sx);
	void ChangeWidthTo(int sx);

	void Animate(bool b) { anim_ = b; }

	bool IsAnimating() const { return anim_; }
	float GetSpeedScale() const { return speed_scale_; }

  int GetScaledWidth() const { return sx_;}
  int GetScaledHeight() const { return sy_;}

  const IplImage* GetOutputImg() const { return scaled_framebuffer_.get(); }

	const VideoData* GetVideo() { return video_.get(); }
  void TrimVideoTo(int num_frames);

	void SetToFrame(int frame, bool update_slider = true);
	int  CurrentFrame() const { return frame_; }

	// void SetCarving(SurfaceCarve* sf) { carving_ = sf; repaint(); }
  void SetSegmentation(const vector<shared_ptr<SegmentationDesc> >* seg);
  void SetLevel(int hierarchy_level);
  void SetColorShuffle(int n) { color_shuffle_ = n; repaint(); }

  bool IsUserInput() { return pos_regions_.size() || neg_regions_.size(); }
  void RenderRegionFrame(int frame, int erase_color,
                         int maintain_color);

  const IplImage* GetRegionFrame() { return region_rendering_.get(); }

  void SkipFrame(int frames);

public slots:
	void SetPositionFromSlider(int);
	void SetSpeed(int);
  void AnimLoop();

  void SetAlpha(int);
  void HideUnselectedRegions();
  void DrawBorders();
  void DrawShapeDescriptors();

  void BrushPos();
  void BrushNeg();
  void BrushErase();
  void BrushReset();
  void ResetView();

  void AdjustLocalLevel(int);

  void Toon();

  void VisualizeNeighbors(int);
  void VisualizeChildren(int);

signals:
	void Resized (int);
  void RegionsDeselected();
  void LocalHierarchyEnabled(int min, int max);

protected:
	void paintEvent(QPaintEvent*);
	void ReserveScaledImage();

  void SetChildrenLevelTo(int level, const CompoundRegion& cr, int cur_level);

	void mouseMoveEvent(QMouseEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);

	void IniMoviePos();
	void AdjustParent(int,int);

  void BrushHandler(QPoint pos);

  RegionID GetRegionIdFromPoint(int x, int y);

  void VisualizeNeighbors(const SegmentationDesc::CompoundRegion& region,
                          const Segment::ParentMap& parent_map,
                          IplImage* output);

  void VisualizeChildren(const SegmentationDesc::CompoundRegion& region,
                         int level,
                         const Hierarchy& hierarchy,
                         const Segment::ParentMap& parent_map,
                         IplImage* output);
protected:
	shared_ptr<VideoData> video_;
  const vector<shared_ptr<SegmentationDesc> >* seg_;

  vector<RegionID> pos_regions_;
  vector<RegionID> neg_regions_;
  RegionID selected_region_;

  // Hierarchy level for each region in the over-segmentation.
  vector<int> local_hierarchy_level_;

  enum BRUSH_STATE {BRUSH_STATE_NON, BRUSH_STATE_POS, BRUSH_STATE_NEG,
                    BRUSH_STATE_ERASE};
  BRUSH_STATE brush_state_;

  int sx_;
	int sy_;
  int frame_;
  float blend_alpha_;
  int color_shuffle_;
  int hide_unselected_regions_;
  bool draw_borders_;
  bool draw_shape_descriptors_;

  bool visualize_children_;
  bool visualize_neighbors_;

  int base_frame_;
  bool anim_started_;

  shared_ptr<IplImage> scaled_framebuffer_;
  shared_ptr<IplImage> blend_image_;
  shared_ptr<IplImage> region_rendering_;

	int save_dx_;
  int global_hierarchy_level_;

	QPoint origin_;

	float speed_scale_;

	int border_;
	bool resizable_;
	bool sizing_;
	bool carvable_;
	bool change_width_;

  QTimer* video_timer_;
	bool anim_;

	QSlider* movie_pos_;
	QSlider* velo_scale_;

  QLabel* width_label_;
	QLabel*	velo_label_;
  QTime start_time_;

	// SurfaceCarve* carving_;
};

#endif
