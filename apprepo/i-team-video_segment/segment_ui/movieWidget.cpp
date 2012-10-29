#include "movieWidget.h"

//#include "SurfaceCarve.h"
#include <boost/random/uniform_int.hpp>
#include <boost/random/additive_combine.hpp>
#include <boost/random/variate_generator.hpp>

#include <google/protobuf/repeated_field.h>
using google::protobuf::RepeatedPtrField;

#include <iostream>
#include <iomanip>

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <QtGui>

#include "assert_log.h"
#include "image_filter.h"
#include "segmentation_util.h"
#include "mainWindow.h"

// A 4 channel ARGB color generator with support for local hierarchy and added color
// shuffle.
class LocalColorGenerator {
public:
  // Input array holds local level for each region.
  LocalColorGenerator(const vector<int>& local_hierarchy_levels,
                      int color_shuffle,
                      const SegmentationDesc& segmentation,
                      const SegmentationDesc* hierarchy);

  bool operator()(int overseg_region_id, RegionID* mapped_id, uchar* colors) const;
protected:
  const vector<int>& local_hierarchy_levels_;
  int color_shuffle_;
  const SegmentationDesc& segmentation_;
  const SegmentationDesc* hierarchy_;
  const int channels_;
};

LocalColorGenerator::LocalColorGenerator(const vector<int>& local_hierarchy_levels,
                                         int color_shuffle,
                                         const SegmentationDesc& segmentation,
                                         const SegmentationDesc* hierarchy)
    : local_hierarchy_levels_(local_hierarchy_levels),
      color_shuffle_(color_shuffle),
      segmentation_(segmentation),
      hierarchy_(hierarchy),
      channels_(4) {
}

bool LocalColorGenerator::operator()(int overseg_region_id,
                                     RegionID* mapped_id,
                                     uchar* colors) const {
  int region_id = overseg_region_id;
  ASSURE_LOG(region_id < local_hierarchy_levels_.size());
  int level = local_hierarchy_levels_[overseg_region_id];
  if (level > 0) {
    region_id = GetParentId(overseg_region_id, 0, level, hierarchy_->hierarchy());
  }

  *mapped_id = RegionID(region_id, level);

  srand(region_id + color_shuffle_);
  for (int c = 0; c < channels_; ++c) {
    colors[c] = (uchar)(rand() % 255);
  }

  return true;
}

// A 4 channel ARGB color for spefic color rendering.
class LocalSpecifiedColorGenerator {
public:
  LocalSpecifiedColorGenerator(const vector<int>& local_hierarchy_levels,
                               const SegmentationDesc& segmentation,
                               const SegmentationDesc* hierarchy);

  void AddColor(const RegionID& region_id, int color) { color_map_[region_id] = color; }
  void AddColor(const vector<RegionID>& region_ids, int color);
  void SetStandardColor(bool use_standard_color, int color) {
    use_standard_color_ = use_standard_color;
    standard_color_ = color;
  }

  bool operator()(int overseg_region_id, RegionID* mapped_id, uchar* colors) const;
protected:
  const vector<int>& local_hierarchy_levels_;
  const SegmentationDesc& segmentation_;
  const SegmentationDesc* hierarchy_;
  typedef hash_map<RegionID, int, RegionIDHasher> ColorMap;
  ColorMap color_map_;

  bool use_standard_color_;
  int standard_color_;
};

LocalSpecifiedColorGenerator::LocalSpecifiedColorGenerator(
    const vector<int>& local_hierarchy_levels,
    const SegmentationDesc& segmentation,
    const SegmentationDesc* hierarchy) : local_hierarchy_levels_(local_hierarchy_levels),
                                         segmentation_(segmentation),
                                         hierarchy_(hierarchy) {
  use_standard_color_ = false;
}

void LocalSpecifiedColorGenerator::AddColor(const vector<RegionID>& region_ids,
                                            int color) {
  for (vector<RegionID>::const_iterator region_id = region_ids.begin();
       region_id != region_ids.end();
       ++region_id) {
    color_map_[*region_id] = color;
  }
}

bool LocalSpecifiedColorGenerator::operator()(int overseg_region_id, RegionID* mapped_id,
                                              uchar* colors) const {
  int region_id = overseg_region_id;
  int level = local_hierarchy_levels_[overseg_region_id];
  if (level > 0) {
    region_id = GetParentId(overseg_region_id, 0, level, hierarchy_->hierarchy());
  }

  *mapped_id = RegionID(region_id, level);

  // Is mapped_id present?
  int* color_int = (int*)colors;
  ColorMap::const_iterator find_iter = color_map_.find(*mapped_id);
  if (find_iter != color_map_.end()) {
    *color_int = find_iter->second;
    return true;
  } else {
    if (use_standard_color_) {
      *color_int = standard_color_;
      return true;
    }
  }

  return false;
}

MovieWidget::MovieWidget(QWidget* parent, QLabel* width_label) :
    QWidget(parent),
    seg_(0),
    sx_(500),
    sy_(300),
    frame_(0),
    blend_alpha_(0.5),
    color_shuffle_(0),
    hide_unselected_regions_(false),
    draw_borders_(true),
    draw_shape_descriptors_(false),
    visualize_children_(false),
    visualize_neighbors_(false),
    speed_scale_(1.0),
    border_(20),
    resizable_(false),
    sizing_(false),
    carvable_(false),
    change_width_(false),
    anim_(false),
    // carving_(0),
  width_label_(width_label) {
  setMouseTracking (true);
  video_timer_ = new QTimer(this);
  connect(video_timer_, SIGNAL(timeout()), this, SLOT(AnimLoop()));

	movie_pos_ = new QSlider(Qt::Horizontal, this);
	movie_pos_->setTickPosition(QSlider::TicksBelow);
	movie_pos_->show();

	velo_scale_ = new QSlider(Qt::Vertical, this);
	velo_scale_->setTickPosition(QSlider::TicksRight);
	velo_scale_->setMinimum(-4);
	velo_scale_->setMaximum(4);

	velo_scale_->setValue(0);
	velo_scale_->show ();

	velo_label_ = new QLabel("1.0", this);
	velo_label_->show ();

	IniMoviePos ();
  brush_state_ = BRUSH_STATE_NON;

	connect(movie_pos_, SIGNAL(sliderMoved(int)), this, SLOT(SetPositionFromSlider(int)));
	connect(velo_scale_, SIGNAL(sliderMoved(int)), this, SLOT(SetSpeed(int)));
}

MovieWidget::~MovieWidget() {
  // TODO: Why is that?
  delete movie_pos_;
  delete velo_scale_;
  delete velo_label_;
  delete video_timer_;
}

void MovieWidget::AnimLoop() {
  QTime cur_time;

  // Can we animate anything?
  if (video_ && anim_) {
    // Have we taken any time already?
    if (!anim_started_) {
      anim_started_ = true;
      start_time_ = QTime::currentTime();
      base_frame_ = frame_;
    } else {
      cur_time = QTime::currentTime();
      int msecs = start_time_.msecsTo(cur_time);

      int frame = msecs / ( 1000 / video_->GetFPS() / speed_scale_);
      SetToFrame ((base_frame_ + frame) % video_->GetFrames());
    }
  } else {
    anim_started_ = false;
  }
}

void MovieWidget::SetAlpha(int alpha) {
  blend_alpha_ = (float)alpha / 100;
  repaint();
}

void MovieWidget::HideUnselectedRegions() {
  hide_unselected_regions_ = (hide_unselected_regions_ + 1) % 4;
  repaint();
}

void MovieWidget::DrawBorders() {
  draw_borders_ = !draw_borders_;
  repaint();
}

void MovieWidget::DrawShapeDescriptors() {
  draw_shape_descriptors_ = !draw_shape_descriptors_;
  repaint();
}

void MovieWidget::BrushPos() {
  brush_state_ = BRUSH_STATE_POS;
  repaint();
}

void MovieWidget::BrushNeg() {
  brush_state_ = BRUSH_STATE_NEG;
  repaint();
}

void MovieWidget::BrushErase() {
  brush_state_ = BRUSH_STATE_ERASE;
  repaint();
}

void MovieWidget::BrushReset() {
  pos_regions_.clear();
  neg_regions_.clear();
  hide_unselected_regions_ = 0;
  brush_state_ = BRUSH_STATE_NON;
  emit(RegionsDeselected());
  repaint();
}

void MovieWidget::ResetView() {
  if(video_) {
    // first get scale
    float s = (float) sy_ / (float) video_->GetHeight();
    ChangeWidthTo(s * video_->GetWidth());
  }
}

void MovieWidget::AdjustLocalLevel(int l) {
  // Change level of selected region in base layer.
  SegmentationDesc* desc = (*seg_)[0].get();

  if (selected_region_.level == 0) {
    selected_region_.level = l;
  } else {
    if (l <= 0) {
      const CompoundRegion& r = desc->hierarchy(selected_region_.level - 1).
                                      region(selected_region_.id);
      ASSURE_LOG(r.id() == selected_region_.id);

      SetChildrenLevelTo(selected_region_.level + l, r, selected_region_.level - 1);
    } else {
      // Get parent at level and set all the children to selected level.
      int r_id = selected_region_.id;
      for (int k = 0; k < l; ++k) {
        r_id = desc->hierarchy(selected_region_.level + k - 1).region(r_id).parent_id();
      }

      const CompoundRegion& r =
          desc->hierarchy(selected_region_.level + l - 1).region(r_id);

      SetChildrenLevelTo(selected_region_.level + l, r, selected_region_.level + l - 1);
    }
  }

  pos_regions_.clear();
  neg_regions_.clear();
  brush_state_ = BRUSH_STATE_NON;
  repaint();
}


void MovieWidget::SetChildrenLevelTo(int level, const CompoundRegion& cr, int cur_level) {

  SegmentationDesc* desc = (*seg_)[0].get();

  if (cur_level == 0) {
    for (int n = 0, sz = cr.child_id_size(); n < sz; ++ n)
      local_hierarchy_level_[cr.child_id(n)] = level;
  } else {
    for (int n = 0, sz = cr.child_id_size(); n < sz; ++ n)
      SetChildrenLevelTo(level, desc->hierarchy(cur_level - 1).region(cr.child_id(n)),
                         cur_level - 1);
  }
}

void MovieWidget::Toon() {
  return;

  // TODO: Fix this, needs to be updated.
  if (!seg_ || !video_) {
    return;
  }

  int num_regions = 0;

  // TODO: Fix, multiple hierarchies now !!
  if (global_hierarchy_level_ > 0) {
    num_regions = (*seg_)[0]->hierarchy(global_hierarchy_level_ - 1).region_size();
  } else {
    num_regions = (*seg_)[0]->region_size();
  }

  vector<float> region_rgb[3];
  region_rgb[0].resize(num_regions, 0);
  region_rgb[1].resize(num_regions, 0);
  region_rgb[2].resize(num_regions, 0);

  vector<float> region_sizes(num_regions, 1);  // Will be reseted 
                                               // - avoid division by zero.

  vector<float> gamma_lut(256);
  int inv_bins = 500;
  vector<uchar> gamma_inv_lut(inv_bins);

  for (int i = 0; i < 256; ++i) {
    gamma_lut[i] = pow((float)i / 255.0f, 1.0f/2.2f) * inv_bins + 0.5;
  }

  for (int i = 0; i < inv_bins; ++i) {
    gamma_inv_lut[i] = (uchar)(pow((float)i / (float)inv_bins, 2.2) * 255.0);
  }


  for (int frame = 0; frame < video_->GetFrames(); ++frame) {
    const SegmentationDesc& seg = *(*seg_)[frame];
    SegmentationDesc* seg_hier = 0;
    if (global_hierarchy_level_ > 0) {
      // Is a hierarchy present at the current frame?
      if ((*seg_)[frame]->hierarchy_size() != 0)
        seg_hier = (*seg_)[frame].get();
      else
        seg_hier = (*seg_)[0].get();
    }

    const int frame_width = video_->GetWidth();

    // Average each region.
    const RepeatedPtrField<Region2D>& regions = seg.region();
    for(RepeatedPtrField<Region2D>::const_iterator r = regions.begin();
        r != regions.end();
        ++r) {

      // Get region id.
      int region_id = GetParentId(r->id(), 0, global_hierarchy_level_,
                                  seg_hier->hierarchy());
      region_sizes[region_id] =
          GetCompoundRegionFromId(region_id,
                                  seg_hier->hierarchy(global_hierarchy_level_)).size();

      const RepeatedPtrField<ScanInterval>& scan_inter = r->raster().scan_inter();

      for(RepeatedPtrField<ScanInterval>::const_iterator s = scan_inter.begin();
          s != scan_inter.end();
          ++s) {
        const int* src_ptr = RowPtr<int>(video_->GetFrameAt(frame), s->y()) + s->left_x();
        for (int j = 0, len = s->right_x() - s->left_x() + 1; j < len; ++j, ++src_ptr) {
          const uchar* rgb_ptr = reinterpret_cast<const uchar*>(src_ptr);
    //      region_rgb[0][region_id] += (float)gamma_lut[rgb_ptr[0]];
    //      region_rgb[1][region_id] += (float)gamma_lut[rgb_ptr[1]];
    //      region_rgb[2][region_id] += (float)gamma_lut[rgb_ptr[2]];
          region_rgb[0][region_id] += (float)rgb_ptr[0];
          region_rgb[1][region_id] += (float)rgb_ptr[1];
          region_rgb[2][region_id] += (float)rgb_ptr[2];
          ++region_sizes[region_id];
        }
      }
    }
  }

  // Normalize.
  for (int i = 0; i < num_regions; ++i) {
    region_rgb[0][i] /= region_sizes[i];
    region_rgb[1][i] /= region_sizes[i];
    region_rgb[2][i] /= region_sizes[i];

    // cap it.
 //   region_rgb[0][i] = std::max(0.f, std::min(499.f, region_rgb[0][i]));
 //   region_rgb[1][i] = std::max(0.f, std::min(499.f, region_rgb[1][i]));
 //   region_rgb[2][i] = std::max(0.f, std::min(499.f, region_rgb[2][i]));

    // Contrast enhancement hack.
    for (int c = 0; c < 3; ++c) {
      if (region_rgb[c][i] < 100)
        region_rgb[c][i] *= 0.8;
      if (region_rgb[c][i] > 140)
        region_rgb[c][i] *= 1.2;
    }

    region_rgb[0][i] = std::max(0.f, std::min(255.f, region_rgb[0][i]));
    region_rgb[1][i] = std::max(0.f, std::min(255.f, region_rgb[1][i]));
    region_rgb[2][i] = std::max(0.f, std::min(255.f, region_rgb[2][i]));
  }

  // Assign to color map.
  // TODO!
  /*
  colors_.resize(num_regions);
  for (int i = 0; i < num_regions; ++i) {
    uchar* color = reinterpret_cast<uchar*>(&colors_[i]);

 //   color[0] = gamma_inv_lut[(int)(region_rgb[0][i] + 0.5)];
 //   color[1] = gamma_inv_lut[(int)(region_rgb[1][i] + 0.5)];
 //   color[2] = gamma_inv_lut[(int)(region_rgb[2][i] + 0.5)];

    color[0] = (uchar)region_rgb[0][i];
    color[1] = (uchar)region_rgb[1][i];
    color[2] = (uchar)region_rgb[2][i];

    color[3] = 0;
  }
   */
}

void MovieWidget::VisualizeNeighbors(int check_state) {
  visualize_neighbors_ = check_state == Qt::Checked;
  repaint();
}

void MovieWidget::VisualizeChildren(int check_state) {
  visualize_children_ = check_state == Qt::Checked;
  repaint();
}

void MovieWidget::paintEvent(QPaintEvent* pe) {
	QPainter painter(this);

	if (video_ == NULL) {
		painter.setPen(Qt::black);
		painter.setBrush(QBrush(Qt::black));
		painter.drawRect(0, 0, sx_, sy_);
	} else {
    // resize to size
    // determine resizePortion and carving portion
    float scale = (float)video_->GetHeight() / (float)sy_;
   /*
    int seams_todo = sx_ * scale - video_->GetWidth();

    if (carving_ && seams_todo != 0) {
      uchar* carved_img = carving_->getCarvedImg (frame_, seams_todo);

      IppiSize src_sz = { video_->getWidth () + seams_todo, video_->getHeight () };
      IppiSize dst_sz = { sx_, sy_ };
      IppiRect src_rect = { 0, 0, src_sz.width, src_sz.height };

      ippiResize_8u_C4R (carved_img, src_sz, carving_->getStep(), src_rect,
                         (Ipp8u*) scaled_img_.get(), sizeof(int) * dst_sz.width, dst_sz,
                         (double) dst_sz.width / (double) src_sz.width,
                         (double) dst_sz.height / (double) src_sz.height, IPPI_INTER_LINEAR );

    } else {
    */

    //  IppiSize src_sz = { video_->getWidth (), video_->getHeight () };
    //  IppiSize dst_sz = { sx_, sy_ };
    //  IppiRect src_rect = { 0, 0, src_sz.width, src_sz.height };

    if (seg_) {
      RenderRegionFrame(frame_,
                        static_cast<int>(blend_alpha_ * 255) << 24 | 0x00ff0000,
                        static_cast<int>(blend_alpha_ * 255) << 24 | 0x0000ff00);

      if (brush_state_ == BRUSH_STATE_NON) {
        cvAddWeighted(region_rendering_.get(),
                      blend_alpha_,
                      video_->GetFrameAt(frame_),
                      1.0 - blend_alpha_,
                      0,
                      blend_image_.get());
      } else {
        AlphaBlend(region_rendering_.get(),
                   video_->GetFrameAt(frame_),
                   BlendOpOver(),
                   blend_image_.get());
      }
      cvResize(blend_image_.get(), scaled_framebuffer_.get(), CV_INTER_LINEAR);
    } else {
      cvResize(video_->GetFrameAt(frame_), scaled_framebuffer_.get(), CV_INTER_LINEAR);
    }

    QImage img((const uchar*)scaled_framebuffer_->imageData,
               scaled_framebuffer_->width,
               scaled_framebuffer_->height,
               QImage::Format_RGB32);
    painter.drawImage(QPoint(0,0), img);
  }

	painter.setPen(Qt::black);

	painter.drawLine(sx_ + 1, sy_ + 2, sx_ + 3, sy_ +  2);
	painter.drawLine(sx_ + 2, sy_ + 1, sx_ + 2, sy_ +  3);

	painter.drawLine(sx_ + 5, sy_ + 2 , sx_ + border_ - 3, sy_ + 2);
	painter.drawLine(sx_ + 2, sy_ + 5 , sx_ + 2, sy_ + border_ - 3);

  QColor col = /*carving_ ? Qt::black :*/ Qt::darkGray;

	QPen pen (col);
	pen.setWidth(3);
	painter.setPen(pen);

	painter.drawLine(sx_ + border_ / 2, 5, sx_ + border_ / 2, sy_ - 5);
}

void MovieWidget::SetVideo(shared_ptr<VideoData> vid) {
  bool anim_state = anim_;
  anim_ = false;
  anim_started_ = false;
  video_timer_->stop();

  video_ = vid;
	int oldsx = sx_;
	int oldsy = sy_;
	sx_ = vid->GetWidth ();
	sy_ = vid->GetHeight ();

  blend_image_ = cvCreateImageShared(sx_, sy_, IPL_DEPTH_8U, 4);
  region_rendering_ = cvCreateImageShared(sx_, sy_, IPL_DEPTH_8U, 4);

	ReserveScaledImage ();
	frame_ = 0;

	updateGeometry();
	IniMoviePos ();

	// carving_ = 0;

	AdjustParent(oldsx, oldsy);

	repaint ();
	emit(Resized(sx_));

  video_timer_->start(1000 / (vid->GetFPS() * 2) );
  anim_ = anim_state;
}

void MovieWidget::SetToFrame(int frame, bool update_slider) {
	if (frame != frame_) {
    frame_ = frame;

		if (update_slider)
			movie_pos_->setValue (frame_ + 1);

		update();
	}
}

void MovieWidget::SkipFrame(int frames) {
  if (video_) {
    if (frames > 0)
      frame_ = (frame_ + frames) % video_->GetFrames();
    else {
      frame_ = (frame_ + frames + video_->GetFrames()) % video_->GetFrames();
    }

    movie_pos_->setValue(frame_ + 1);

    update();
  }
}


void MovieWidget::SetPositionFromSlider(int frame) {
	if (video_timer_) {
		SetToFrame(frame - 1, false);
	}
}

void MovieWidget::SetSpeed(int s) {
	if (s == 0)
		speed_scale_ = 1.0;
	else if (s < 0) {
		s *= -1;
		speed_scale_ = 0.2 * (5-s);
	}	else {
		speed_scale_ = s+1;
	}

	velo_label_->setText(QString::number(speed_scale_, 'g', 2));
  // Force animation re-initialization.
  anim_started_ = false;
}

void MovieWidget::ScaleSize(int sx) {
  if (sx_ == sx)
    return;

	// Keep aspect ratio.
	float a = (float) sy_ / (float) sx_;
	if (video_)
		a = (float) video_->GetHeight() / (float) video_->GetWidth() ;

	int oldsx = sx_;
	int oldsy = sy_;

	sx_ = sx;
	sy_ = sx * a;

	ReserveScaledImage ();

	updateGeometry ();
	IniMoviePos ();
	resize (sizeHint());

	AdjustParent (oldsx, oldsy);

	repaint ();
	emit(Resized(sx_));
}

void MovieWidget::ChangeWidthTo(int w)
{
  if (sx_ == w)
    return;

	// first get scale
	float s = (float) sy_ / (float) video_->GetHeight ();
	int lower = video_->GetWidth() * s;
  int upper = video_->GetWidth() * s;

  /*
  if (carving_) {
    lower -= carving_->getSeams() * s;
    upper += carving_->getSeams() * s;
  }
   */

	if (w < lower)
		w = lower;
	if (w > upper)
		w = upper;

	sx_ = w;
  width_label_->setText(QString ("Current width: ") +
                        QString::number(100.0 * w / (video_->GetWidth() * s), 'f', 1)  +
                        "%");

	ReserveScaledImage ();

	updateGeometry ();
	IniMoviePos ();
	resize (sizeHint());

	repaint ();
	emit(Resized(sx_));
}

void MovieWidget::TrimVideoTo(int num_frames) {
  video_->TrimTo(num_frames);
  IniMoviePos();
}

void MovieWidget::SetSegmentation(const vector<shared_ptr<SegmentationDesc> >* seg) {
  seg_ = seg;
  if(seg_) {
    SetLevel(0);
  }
}

void MovieWidget::SetLevel(int hierarchy_level) {
  global_hierarchy_level_ = hierarchy_level;
  SegmentationDesc* desc = (*seg_)[0].get();
  BrushReset();

  // Set each regions hierarchy to global one.
  local_hierarchy_level_ = vector<int>(desc->region(desc->region_size() - 1).id(),
                                       global_hierarchy_level_);
  repaint();
}

RegionID MovieWidget::GetRegionIdFromPoint(int x, int y) {
  const SegmentationDesc& seg = *(*seg_)[frame_];
  // Fetch corresponding hierarchy frame.
  SegmentationDesc* seg_hier = (*seg_)[seg.hierarchy_frame_idx()].get();

  int overseg_id = GetOversegmentedRegionIdFromPoint(x,  y, seg);

  int level = local_hierarchy_level_[overseg_id];
  return RegionID(GetParentId(overseg_id, 0, level, seg_hier->hierarchy()), level);
}

void MovieWidget::RenderRegionFrame(int frame, int erase_color, int maintain_color) {
  // Clear image.
  cvZero(region_rendering_.get());
  if (seg_ == NULL) {
    return;
  }

  const SegmentationDesc& seg = *(*seg_)[frame];

  // Fetch corresponding hierarchy frame.
  SegmentationDesc* seg_hier = (*seg_)[seg.hierarchy_frame_idx()].get();
  ASSURE_LOG(seg_hier->hierarchy_size() > 0) << "Hierarchy not present in specified "
                                             << "hierarchy frame.";
  const int max_seg_id = seg.region(seg.region_size() - 1).id() + 1;
  if (local_hierarchy_level_.size() < max_seg_id) {
    // Pad with global hierarchy level.
    local_hierarchy_level_.resize(max_seg_id, global_hierarchy_level_);
  }

  if (brush_state_ == BRUSH_STATE_NON) {
    LocalColorGenerator local_generator(local_hierarchy_level_,
                                        color_shuffle_,
                                        seg,
                                        seg_hier);

    RenderRegions(region_rendering_->imageData,
                  region_rendering_->widthStep,
                  region_rendering_->width,
                  region_rendering_->height,
                  4,
                  draw_borders_,
                  draw_shape_descriptors_,
                  seg,
                  local_generator);
  } else {
    LocalSpecifiedColorGenerator generator(local_hierarchy_level_, seg, seg_hier);
    generator.AddColor(pos_regions_, maintain_color);
    generator.AddColor(neg_regions_, erase_color);
    switch (hide_unselected_regions_) {
      case 1:
        generator.SetStandardColor(true, 0xffffffff);
        break;
      case 2:
        generator.SetStandardColor(true, 0xff000000);
        break;
      case 3:
        generator.SetStandardColor(true, 0xff808080);
        break;
      default:
        break;
    }

    RenderRegions(region_rendering_->imageData,
                  region_rendering_->widthStep,
                  region_rendering_->width,
                  region_rendering_->height,
                  4,
                  draw_borders_,
                  draw_shape_descriptors_,
                  seg,
                  generator);
  }

  // Compound regions in this frame with over-segmented regions.
  Segment::ParentMap parent_map;
  GetParentMap(global_hierarchy_level_, seg, seg_hier->hierarchy(), &parent_map);

  if (brush_state_ == BRUSH_STATE_NON) {
    for (Segment::ParentMap::const_iterator region_iter = parent_map.begin();
         region_iter != parent_map.end();
         ++region_iter) {
      if (visualize_neighbors_) {
        VisualizeNeighbors(GetCompoundRegionFromId(
                               region_iter->first,
                               seg_hier->hierarchy(global_hierarchy_level_)),
                           parent_map,
                           region_rendering_.get());
      }

      if (visualize_children_) {
        VisualizeChildren(GetCompoundRegionFromId(
                              region_iter->first,
                              seg_hier->hierarchy(global_hierarchy_level_)),
                          global_hierarchy_level_,
                          seg_hier->hierarchy(),
                          parent_map,
                          region_rendering_.get());
      }
    }
  } else {
    vector<RegionID> ids_to_render = pos_regions_;
    ids_to_render.insert(ids_to_render.end(), neg_regions_.begin(), neg_regions_.end());
    for (vector<RegionID>::const_iterator region = ids_to_render.begin();
         region != ids_to_render.end();
         ++region) {
      if (region->level != global_hierarchy_level_ ||
          parent_map.find(region->id) == parent_map.end()) {
        continue;
      }

      if (visualize_neighbors_) {
        VisualizeNeighbors(GetCompoundRegionFromId(region->id,
                                                   seg_hier->hierarchy(region->level)),
                           parent_map,
                           region_rendering_.get());
      }

      if (visualize_children_) {
        VisualizeChildren(GetCompoundRegionFromId(region->id,
                                                  seg_hier->hierarchy(region->level)),
                          region->level,
                          seg_hier->hierarchy(),
                          parent_map,
                          region_rendering_.get());
      }
    }
  }
}

void MovieWidget::AdjustParent(int oldsx, int oldsy) {
	MainWindow* parent =
      (MainWindow*)parentWidget()->parentWidget()->parentWidget()->parentWidget();
	if (parent) {
		QSize sz = parent->size ();
		parent->resize ( sz.width() + sx_ - oldsx, sz.height() + sy_ - oldsy);
	}

}

void MovieWidget::BrushHandler(QPoint pos) {
  this->setCursor (Qt::PointingHandCursor);

  if (video_ && seg_) {
    // Convert to window coordinates.
    float scale = (float) video_->GetWidth() / (float) sx_;

    RegionID id = GetRegionIdFromPoint((float)pos.x() * scale, (float)pos.y() * scale);
    if (id.id < 0) {
      return;
    }

    if (brush_state_ == BRUSH_STATE_POS) {
      // Erase from other list.
      vector<RegionID>::iterator i_n = std::lower_bound(neg_regions_.begin(),
                                                        neg_regions_.end(),
                                                        id);
      if (i_n != neg_regions_.end() && *i_n == id)
        neg_regions_.erase(i_n);

      // Add to pos list if not present already.
      vector<RegionID>::iterator i_p = std::lower_bound(pos_regions_.begin(),
                                                        pos_regions_.end(),
                                                        id);
      if (i_p == pos_regions_.end() || *i_p != id)
        pos_regions_.insert(std::lower_bound(pos_regions_.begin(),
                                             pos_regions_.end(),
                                             id), id);

    } else if (brush_state_ == BRUSH_STATE_NEG) {
      vector<RegionID>::iterator i_p = std::lower_bound(pos_regions_.begin(),
                                                        pos_regions_.end(),
                                                        id);
      if (i_p != pos_regions_.end() && *i_p == id)
        pos_regions_.erase(i_p);

      vector<RegionID>::iterator i_n = std::lower_bound(neg_regions_.begin(),
                                                        neg_regions_.end(),
                                                        id);
      if (i_n == neg_regions_.end() || *i_n != id)
        neg_regions_.insert(std::lower_bound(neg_regions_.begin(),
                                             neg_regions_.end(),
                                             id), id);

    } else if (brush_state_ == BRUSH_STATE_ERASE) {
      vector<RegionID>::iterator i_p = std::lower_bound(pos_regions_.begin(),
                                                        pos_regions_.end(),
                                                        id);
      if (i_p != pos_regions_.end() && *i_p == id)
        pos_regions_.erase(i_p);

      vector<RegionID>::iterator i_n = std::lower_bound(neg_regions_.begin(),
                                                        neg_regions_.end(),
                                                        id);
      if (i_n != neg_regions_.end() && *i_n == id)
        neg_regions_.erase(i_n);
    }

    // Count regions. If only one -> remember and enable local hierarchy slider.
    if (pos_regions_.size() + neg_regions_.size() == 1) {
      RegionID r_id;
      if (pos_regions_.size())
        r_id = pos_regions_[0];
      else
        r_id = neg_regions_[0];


      // Save info.
      selected_region_ = r_id;

      emit(LocalHierarchyEnabled(-r_id.level, (*seg_)[0]->hierarchy_size() - r_id.level));
    } else {
      emit(RegionsDeselected());
    }

    repaint();
  }
}

void MovieWidget::mouseMoveEvent(QMouseEvent* me) {
	if (sizing_) {
		QPoint dp = me->pos() - origin_;
		int dx = dp.x();
		ScaleSize (save_dx_ + dx);
	} else if (change_width_) {
		QPoint dp = me->pos() - origin_;
		int dx = dp.x();
		ChangeWidthTo(save_dx_ + dx);
	} else {
		QPoint pos = me->pos();

		if (pos.x() > sx_ &&
        pos.x() < sx_ + border_ &&
        pos.y() < sy_ + border_ &&
        pos.y() > sy_ ) {
			resizable_ = true;
			this->setCursor(Qt::SizeFDiagCursor);
		}/* else if (pos.x() > sx_ + border_ / 2 - 5 &&
               pos.x() < sx_ + border_ / 2 + 5 &&
               pos.y() > 5 &&
               pos.y() < sy_ - 5 &&
               carving_) {
			carvable_ = true;
			this->setCursor(Qt::SizeHorCursor);
		} */ else if (pos.x() < sx_ && pos.y() < sy_) {
      resizable_ = false;
      carvable_ = false;

      if (brush_state_ != BRUSH_STATE_NON)
        // Was mouse pressed ?
        if (me->buttons() & Qt::LeftButton |
            me->buttons() & Qt::RightButton |
            me->buttons() & Qt::MidButton)
        BrushHandler(pos);
      else {
        this->setCursor(Qt::ArrowCursor);
      }
    } else {
			resizable_ = false;
      carvable_ = false;
			this->setCursor(Qt::ArrowCursor);
		}
	}
}

void MovieWidget::IniMoviePos() {
	QSize sz = movie_pos_->sizeHint ();

	movie_pos_->setGeometry(0, sy_ + 2, sx_, sz.height () );
	velo_scale_->setGeometry(sx_ + border_ + 2, 0, velo_scale_->size().width(), sy_);
	velo_label_->move(sx_ + border_ + 7, sy_ + 2);

	if (video_) {
		movie_pos_->setMinimum(1);
		movie_pos_->setMaximum(video_->GetFrames());
		movie_pos_->setValue(frame_);
	}
}

void MovieWidget::mousePressEvent(QMouseEvent* me) {
	if (resizable_) {
		sizing_ = true;
		save_dx_ = sx_;
		origin_ = me->pos ();
	}

	if (carvable_) {
		change_width_ = true;
		save_dx_ = sx_;
		origin_ = me->pos ();
	}

  QPoint pos = me->pos();
  if (pos.x() < sx_ && pos.y() < sy_) {
    if (brush_state_ != BRUSH_STATE_NON)
      BrushHandler(pos);
  }
}

void MovieWidget::mouseReleaseEvent(QMouseEvent* me) {
	sizing_ = false;
	change_width_ = false;
}

void MovieWidget::ReserveScaledImage() {
	scaled_framebuffer_ = cvCreateImageShared(sx_, sy_, IPL_DEPTH_8U, 4);
}

void MovieWidget::VisualizeNeighbors(const SegmentationDesc::CompoundRegion& region,
                                     const Segment::ParentMap& parent_map,
                                     IplImage* output) {
  int region_id = region.id();
  Segment::ParentMap::const_iterator parent_iter = parent_map.find(region_id);
  ASSERT_LOG(parent_iter != parent_map.end());

  Segment::ShapeDescriptor main_shape;
  GetShapeDescriptorFromRegions(parent_iter->second, &main_shape);

  Segment::ShapeDescriptor neighbor_shape;
  for (int n = 0; n < region.neighbor_id_size(); ++n) {
    // 3D region, might not be present in this frame.
    Segment::ParentMap::const_iterator neighbor_iter =
        parent_map.find(region.neighbor_id(n));

    if (neighbor_iter == parent_map.end()) {
      continue;
    }

    GetShapeDescriptorFromRegions(neighbor_iter->second, &neighbor_shape);

    // Draw line between centroids.
    cvLine(output, cvPoint(main_shape.center_x, main_shape.center_y),
           cvPoint(neighbor_shape.center_x, neighbor_shape.center_y),
           cvScalar(255, 255, 255, 255));
  }
}

void MovieWidget::VisualizeChildren(const SegmentationDesc::CompoundRegion& region,
                                    int level,
                                    const Hierarchy& hierarchy,
                                    const Segment::ParentMap& parent_map,
                                    IplImage* output) {
  if (region.parent_id() < 0) {
    return;
  }

  int region_id = region.id();
  Segment::ParentMap::const_iterator parent_iter = parent_map.find(region_id);
  ASSERT_LOG(parent_iter != parent_map.end());

  Segment::ShapeDescriptor main_shape;
  GetShapeDescriptorFromRegions(parent_iter->second, &main_shape);

  Segment::ShapeDescriptor child_shape;
  const SegmentationDesc::CompoundRegion& parent_region =
      GetCompoundRegionFromId(region.parent_id(), hierarchy.Get(level + 1));
  for (int c = 0; c < parent_region.child_id_size(); ++c) {
    // 3D region, might not be present in this frame.
    Segment::ParentMap::const_iterator child_iter =
        parent_map.find(parent_region.child_id(c));

    if (child_iter == parent_map.end()) {
      continue;
    }

    // Only connect if neighbors, to keep visualization in touch with above function.
    const int* neighbor_pos =
        std::lower_bound(region.neighbor_id().begin(), region.neighbor_id().end(),
                         child_iter->first);
    if (neighbor_pos == region.neighbor_id().end() ||
        *neighbor_pos != child_iter->first) {
      continue;
    }

    GetShapeDescriptorFromRegions(child_iter->second, &child_shape);

    // Draw line between centroids.
    cvLine(output, cvPoint(main_shape.center_x, main_shape.center_y),
           cvPoint(child_shape.center_x, child_shape.center_y),
           cvScalar(0, 0, 255, 255), 2, CV_AA);
  }
}
