/*
 *  VideoAnalyzer.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/18/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_ANALYZER_H__
#define VIDEO_ANALYZER_H__

#include <QObject>
#include <QThread>
#include <QTransform>

#include "video_unit.h"
#include "video_reader_unit.h"
using VideoFramework::FrameSetPtr;
using VideoFramework::StreamSet;

#include "canvas_cut.h"

#include <vector>
using std::vector;

#include <list>
using std::list;

#include <string>
using std::string;

#include <boost/shared_array.hpp>
using boost::shared_array;

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <utility>

#include <fstream>
namespace Segment {
  class SegmentationDesc;
}

class FrameInfo {
public: 
  enum SeamDirection { SEAM_45, SEAM_135 };
  enum FrameOrder { FRAME_FIRST, FRAME_SECOND };
  
public:
  FrameInfo() : pts_(0), is_canvas_frame_(false) {}
  const QTransform& get_transform() const { return transform_; }
  void set_transform(const QTransform& t) { transform_ = t; }
  
  const uchar* get_canvas_mask() const { return canvas_mask_.get(); }
  uchar* get_canvas_mask() { return canvas_mask_.get(); }
  void set_canvas_mask(uchar* mask) { canvas_mask_.reset(mask); }
  
  void set_pts(int64_t pts) { pts_ = pts; }
  int64_t get_pts() const { return pts_; }
   
  void set_canvas_frame(bool b) { is_canvas_frame_ = b; }
  bool is_canvas_frame() const { return is_canvas_frame_; }
  
  const vector< std::pair<int, uchar> >& get_region_probs() const { return region_probs_; }
  void set_region_probs(const vector< std::pair<int, uchar> >& region_probs)
      { region_probs_ = region_probs; }
  
  int get_hierarchy_level() const { return hierarchy_level_; }
  void set_hierarchy_level(int l) { hierarchy_level_ = l; }
  
protected:
  // Only translation supported right now.
  QTransform transform_;
  shared_array<uchar> canvas_mask_;
  int64_t pts_;
  bool is_canvas_frame_;   // True, if frame is used to stitch canvas.
  
  vector< std::pair<int, uchar> > region_probs_;
  int hierarchy_level_;
};

std::ostream& operator<<(std::ostream& ofs, const FrameInfo& fi);
std::istream& operator>>(std::istream& ifs, FrameInfo& fi);

class FrameInfoPTSComperator {
public: 
  FrameInfoPTSComperator() {}
  bool operator()(const FrameInfo& lhs, const FrameInfo& rhs) {
    return lhs.get_pts() < rhs.get_pts();
  }
};

class VideoAnalyzer : public QThread, public VideoFramework::VideoUnit {
  Q_OBJECT
public:
  VideoAnalyzer(const QString& video_file, int frame_spacing, float frame_spacing_ratio,
                int seg_level);
  void run();
  
  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  
  // Return new canvas that is big enough to fit all FrameInfo's.
  QImage* ComputeAndAllocateNewCanvas(QPointF*) const;
  
  // Swaps the frame_infos_ member to avoid copying.
  void RetrieveFrameInfos(vector<FrameInfo>* output) { output->swap(frame_infos_); }

public slots:
  void CancelJob();
  
signals:
  void AnalyzeProgress(int);
  
protected:
  enum PassMode { FIRST_PASS, SECOND_PASS };
  
  bool ReadSpacePlayerFeatures();
  
  // Returns the distance between two frames as a fraction of the frame diameter.
  float FrameCenterDist(const QTransform& t1, const QTransform& t2,
                       int frame_width, int frame_height) const;
protected:
  shared_ptr<VideoFramework::VideoReaderUnit> video_reader_;
  QString video_file_;
  QString feat_file_pre_;
  
  int cur_frame_;
  int frame_spacing_;
  float frame_spacing_ratio_;
  bool is_canceled_;
  
  int video_stream_idx_;
  int frame_width_;
  int frame_height_;
  int frame_width_step_;
  
  shared_ptr<CanvasCut> canvas_cut_;
  shared_ptr<IplImage> id_img_;
  shared_array<uchar> foreground_mask_;
  shared_ptr<IplImage> canvas_transformed_;
  
  vector<FrameInfo> frame_infos_;
  PassMode pass_mode_;
  
  shared_ptr<QImage> canvas_;
  QPointF canvas_offset_;
  shared_ptr<IplImage> canvas_foreground_;
  
  QTransform prev_canvas_frame_trans_;
  
  // Segmentation input file stream.
  std::ifstream seg_ifs_;
  bool segmentation_available_;
  int seg_level_;
};

QPointF MinPt(const QPointF& lhs, const QPointF& rhs);
QPointF MaxPt(const QPointF& lhs, const QPointF& rhs);
void CanvasIplImageView(const QImage& canvas, IplImage* view);

void CopyMasked32s(const int* src, int src_width_step,
                   QRectF dst_rect, int* dst, int dst_width_step, int alpha,
                   int* marker, int marker_width_step, int marker_value);

#endif  // VIDEO_ANALYZER_H__