/*
 *  VideoAnalyzer.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/18/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_analyzer.h"
#include <QMessageBox>
#include <QFile>
#include <QPainter>

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <utility>
using std::pair;

#include <fstream>
#include <iterator>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <imgproc/imgproc_c.h>

#include <cmath>
#include "assert_log.h"
#include "image_util.h"
using namespace ImageFilter;

#include "warping.h"
#include "bilateral_filter_1D.h"
#include "GCoptimization.h"

#include "segmentation.pb.h"
#include "segmentation_util.h"

using VideoFramework::VideoFrame;
using VideoFramework::VideoStream;

std::ostream& operator<<(std::ostream& ofs, const FrameInfo& fi) {
  QTransform t = fi.get_transform();
  ofs << t.m11() << " " << t.m12() << " " << t.m13() << " ";
  ofs << t.m21() << " " << t.m22() << " " << t.m23() << " ";
  ofs << t.m31() << " " << t.m32() << " " << t.m33() << " ";
  
  ofs << "\n" << fi.get_pts() << " " << fi.is_canvas_frame() << "\n";
  return ofs;
}

std::istream& operator>>(std::istream& ifs, FrameInfo& fi) {
  qreal m[9];
  for (int i = 0; i < 9; ++i)
    ifs >> m[i];
  
  int64_t pts;
  bool canvas_frame;
  ifs >> pts >> canvas_frame;
  
  QTransform t(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
  fi.set_transform(t);
  fi.set_pts(pts);
  fi.set_canvas_mask(0);
  fi.set_canvas_frame(pts);

  return ifs;
}


QPointF MinPt(const QPointF& lhs, const QPointF& rhs) {
  return QPointF(std::min(lhs.x(), rhs.x()), std::min(lhs.y(), rhs.y()));
}

QPointF MaxPt(const QPointF& lhs, const QPointF& rhs) {
  return QPointF(std::max(lhs.x(), rhs.x()), std::max(lhs.y(), rhs.y()));
}

QPoint ToPoint(const QPointF p) {
  return QPoint(floor(p.x() + 0.5), floor(p.y() + 0.5));
}

void CanvasIplImageView(const QImage& canvas, IplImage* view) {
  cvInitImageHeader(view, cvSize(canvas.width(), canvas.height()), IPL_DEPTH_8U, 4);
  cvSetData(view, (void*)canvas.bits(), canvas.bytesPerLine());
}

void CopyMasked32s(const int* src, int src_width_step,
                   QRectF dst_rect, int* dst, int dst_width_step, int alpha,
                   int* marker, int marker_width_step, int marker_value) {
  
  for (int i = 0; i < dst_rect.height(); ++i) {
    const int* src_row = PtrOffset(src, i * src_width_step);
    int* dst_row = PtrOffset(dst, (i + (int)dst_rect.y()) * dst_width_step) + (int)dst_rect.x();
    int* marker_row = PtrOffset(marker, (i + (int)dst_rect.y()) * marker_width_step)
    + (int)dst_rect.x();
    
    for (int j = 0; j < dst_rect.width(); ++j, ++src_row, ++dst_row, ++marker_row) {
      uchar* mask = (uchar*)src_row;
      if (mask[3] > 250) {
        *dst_row = *src_row;
        uchar* tmp = (uchar*) dst_row;
        tmp[3] = alpha;
        
        if (marker)
          marker_row[0] = marker_value;
      } else if (mask[3] > 20) {
        uchar* dst = (uchar*) dst_row;
        uchar* src = (uchar*) src_row;
        dst[0] = ((255 - mask[3]) * dst[0] + mask[3] * src[0]) >> 8;   // divide by 255.
        dst[1] = ((255 - mask[3]) * dst[1] + mask[3] * src[1]) >> 8;   // divide by 255.
        dst[2] = ((255 - mask[3]) * dst[2] + mask[3] * src[2]) >> 8;   // divide by 255.
        dst[3] = alpha;
      }
    }
  }
}


VideoAnalyzer::VideoAnalyzer(const QString& video_file, int frame_spacing, 
                             float frame_spacing_ratio, int seg_level) : 
    video_file_(video_file), frame_spacing_(frame_spacing), 
    frame_spacing_ratio_(frame_spacing_ratio), is_canceled_(false), 
    seg_level_(seg_level) {
  
  video_reader_.reset(new VideoFramework::VideoReaderUnit(video_file_.toStdString()));
  this->AttachTo(video_reader_.get());
      
  if (!video_reader_->PrepareProcessing()) {
    QMessageBox::critical(NULL, "", "VideoAnalyzer:: Error opening Streams");                           
  }
}

void VideoAnalyzer::run() {
  // Support for two pass mode. Only first pass used currently.
  
  int frames_processed_ = 0;
  pass_mode_ = FIRST_PASS;
  
  while (!is_canceled_ && RootUnit()->NextFrame()) {
    ++frames_processed_;
    emit(AnalyzeProgress(frames_processed_));
  }
  
  if (is_canceled_) {
    emit AnalyzeProgress(-2);
    
    // Erase all mask we have allocated so far.
    for (vector<FrameInfo>::iterator i = frame_infos_.begin(); i != frame_infos_.end(); ++i)
      i->set_canvas_mask(0);
    
    return;
  }
  
  // Signal success:
  emit AnalyzeProgress(-1);  // Remove for two pass mode.
  
  /* 
  } else {
    pass_mode_ = SECOND_PASS;
  
    // Reset and analyze again. 
    RootUnit()->Seek();
    cur_frame_ = 0;
   
   while (RootUnit()->NextFrame() && !is_canceled_) {
      ++frames_processed_;
      emit(AnalyzeProgress(frames_processed_ / 2));
    }    
    
    if (is_canceled_) {
      emit AnalyzeProgress(-2);
      return;
    } else {
      emit AnalyzeProgress(-1);
      return;
    }
  }
   */
}

bool VideoAnalyzer::OpenStreams(StreamSet* set) {
  // Get first feature file.
  QString name = video_file_.left(video_file_.lastIndexOf("."));
  int delim = name.lastIndexOf("/");
  QString path = name.left(delim + 1);
  name = name.right(name.length() - delim-1);
  feat_file_pre_ = path + name + "/" + name;
  cur_frame_ = 0;

  frame_infos_.clear();
  
  // Read features.
  if (!ReadSpacePlayerFeatures())
    return false;
  
  // Find video stream idx.
  video_stream_idx_ = FindStreamIdx("VideoStream", set);
  
  if (video_stream_idx_ < 0) {
    QMessageBox::critical(NULL, "", 
                          "VideoAnalyzer::OpenStreams: Could not find Video stream!");
    return false;
  } 
  
  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                                                   set->at(video_stream_idx_).get());
  
  frame_width_ = vid_stream->get_frame_width();
  frame_height_ = vid_stream->get_frame_height();
  frame_width_step_ = vid_stream->get_width_step();
  
  id_img_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_32S, 1);
  foreground_mask_.reset(new uchar[frame_width_ * frame_height_]);
  prev_canvas_frame_trans_.reset();
  
  canvas_transformed_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 4);
  
  // Allocate canvas from features.
  canvas_.reset(ComputeAndAllocateNewCanvas(&canvas_offset_));
  canvas_foreground_ = cvCreateImageShared(canvas_->width(), canvas_->height(),
                                           IPL_DEPTH_8U, 1);
  cvSet(canvas_foreground_.get(), cvScalar(0));
  canvas_cut_.reset(new CanvasCut(frame_width_, frame_height_));
  
  // Segmentation file present?
  string seg_file = (video_file_.left(video_file_.lastIndexOf(".")) + "_segment8.pb").toStdString();
  // Use it?
  if (QFile(seg_file.c_str()).exists()) {
    seg_ifs_.open(seg_file.c_str(), std::ios_base::in | std::ios_base::binary);
    
    // Read offset for each segmentation frame from header.
    int num_seg_frames;
    int64_t seg_header_offset;
    
    seg_ifs_.read(reinterpret_cast<char*>(&num_seg_frames), sizeof(num_seg_frames));
    seg_ifs_.read(reinterpret_cast<char*>(&seg_header_offset), sizeof(seg_header_offset));
    
    segmentation_available_ = true;
  } else {
    segmentation_available_ = false;
  }

   return true;
}

void VideoAnalyzer::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);
  
  if (pass_mode_ == FIRST_PASS) {
    // Transformation already present in frame_infos.
    frame_infos_[cur_frame_].set_pts(frame->get_pts());
        
    // Should frame be used to stitch canvas?
    if(cur_frame_ == 0 ||
      (frame_spacing_ && cur_frame_ % frame_spacing_ == 0) ||
      (!frame_spacing_ && FrameCenterDist(prev_canvas_frame_trans_,
                                          frame_infos_[cur_frame_].get_transform(),
                                          frame_width_, frame_height_) > frame_spacing_ratio_)) {
      frame_infos_[cur_frame_].set_canvas_frame(true);
      prev_canvas_frame_trans_ = frame_infos_[cur_frame_].get_transform();
    }          
          
    // Mark foreground regions.
    if (segmentation_available_) {
      Segment::SegmentationDesc seg;
      int frame_sz;
      seg_ifs_.read(reinterpret_cast<char*>(&frame_sz), sizeof(frame_sz));
      vector<char> frame_data(frame_sz);
      seg_ifs_.read(reinterpret_cast<char*>(&frame_data[0]), frame_sz);
      seg.ParseFromArray(&frame_data[0], frame_sz);
      
      // TODO: Remove!
      // Visualize background.
      shared_array<uchar> foreground_mask(new uchar[frame_width_ * frame_height_]);
      memset(foreground_mask.get(), 0, frame_width_ * frame_height_);
      
      /*
      Segment::RenderRegionsRandomColor(
                                        frame_infos_[cur_frame_].get_region_probs(),
                                        foreground_mask.get(), frame_width_, frame_width_,
                             frame_height_, 1,  
                             0,  // Level 0!
                             seg, 0);
      */
      
      QImage foreground_img(foreground_mask.get(), frame_width_, frame_height_, frame_width_,
                            QImage::Format_Indexed8);
      QImage palette("/grundmann2/SpacePlayer/color.png");
      
      for (int i = 0; i < 256; ++i) {
        QRgb* col = (QRgb*)palette.scanLine(std::max(0, 254 - i));
        foreground_img.setColor(i, *col);
      }
      
      foreground_img.save(QString("forground_mask_%1.png").arg(cur_frame_, 4, 10, 
                                                               QLatin1Char('0')));
      foreground_mask.reset();
      // TODO: End remove.
        
      if(frame_infos_[cur_frame_].is_canvas_frame()) {
        /*
        uchar* foreground_mask = new uchar[frame_width_ * frame_height_];
        memset(foreground_mask, 0, frame_width_ * frame_height_);
              
        Segment::RenderRegions(frame_infos_[cur_frame_].get_region_probs(),
                               foreground_mask, frame_width_, frame_width_,
                               frame_height_, 1,  
                               frame_infos_[cur_frame_].get_hierarchy_level(), seg, 0);
            
        frame_infos_[cur_frame_].set_canvas_mask(foreground_mask);
         */
      }
    }
    
    // Stitch if canvas frame.
    if  (frame_infos_[cur_frame_].is_canvas_frame()) {
      QImage img(frame->get_data(), frame->get_width(), 
                 frame->get_height(), frame->get_width_step(), 
                 QImage::Format_RGB888);
      
      if (cur_frame_ != 0) {
        img = img.convertToFormat(QImage::Format_ARGB32);
        
        // Get underlying canvas content.
        QTransform transform = frame_infos_[cur_frame_].get_transform();
        
        // Concatenate with post transform.
        QTransform post_trans(1, 0, 0, 1, -canvas_offset_.x(), -canvas_offset_.y());
        transform = transform * post_trans;
        
        IplImage canvas_view;
        CanvasIplImageView(*canvas_, &canvas_view);
        float transform_data[6] = { transform.m11(), transform.m21(), transform.m31(),
                                     transform.m12(), transform.m22(), transform.m32() };
        
        // Backward warp.
        ImageFilter::BackwardWarp<uchar>(&canvas_view, transform_data, canvas_transformed_.get());
        
        uchar* mask = frame_infos_[cur_frame_].get_canvas_mask();
        
        if (mask) {
          // Blur and threshold.
          CvMat mask_mat;
          cvInitMatHeader(&mask_mat, frame_height_, frame_width_, CV_8UC1, mask);
          cvSmooth(&mask_mat, &mask_mat, CV_GAUSSIAN, 15, 15);
        }
        
        canvas_cut_->ProcessImage(canvas_transformed_.get(), frame->get_data(),
                                  frame->get_width_step(), mask);
        
        if (!mask) {
          mask = new uchar[frame_width_ * frame_height_];
          frame_infos_[cur_frame_].set_canvas_mask(mask);
          memset(mask, 0, frame_width_ * frame_height_);
        } else {
          // Visualize the foreground.
          QImage foreground_img(mask, frame_width_, frame_height_, frame_width_,
                                QImage::Format_Indexed8);
          QImage palette("/grundmann2/SpacePlayer/color.png");
        
          for (int i = 0; i < 256; ++i) {
            QRgb* col = (QRgb*)palette.scanLine(std::max(0, 254 - i));
            foreground_img.setColor(i, *col);
          }
          
          foreground_img.save(QString("forground_mask_%1.png").arg(cur_frame_, 4, 10, 
                                                                   QLatin1Char('0')));
        }
        
        uchar* mask_ptr = mask;
        
        // Setup label mask and use to paste updated regions onto canvas.
        for (int i = 0, idx = 0; i < frame_height_; ++i) {
      //    uchar* foreground_ptr = RowPtr<uchar>(canvas_foreground_.get(), tl.y() + i) + (int)tl.x();
          for (int j = 0; j < frame_width_; ++j, ++idx, ++mask_ptr) {
            // Use frame pixel?
            if (canvas_cut_->Label(idx) == 0) {
//              if (*mask_ptr > 180)    // was labeled as foreground and copied to canvas.
 //               *foreground_ptr = 1;
              
              *mask_ptr = 0xff;     
            }
            else {
              *mask_ptr = 0;
            }
          }
        }
        
        QRectF frame_rect(0, 0, frame_width_, frame_height_);
        QTransform tf = frame_infos_[cur_frame_].get_transform();
        QRectF canvas_rect = tf.mapRect(frame_rect);
        canvas_rect = canvas_rect.translated(-1.f * canvas_offset_);
        
        // Dump mask into img_data and transform with it.
        for (int i = 0; i < frame_height_; ++i) {
          uchar* dst_ptr = PtrOffset(img.bits(), i * img.bytesPerLine());
          const uchar* mask_ptr = PtrOffset(mask, i * frame_width_);
          
          for (int j = 0; j < frame_width_; ++j, ++mask_ptr, dst_ptr += 4) {
            dst_ptr[3] = mask_ptr[0];
          }
        }
        
        QImage img_trans = img.transformed(tf, Qt::SmoothTransformation);
        
        CopyMasked32s((int*)img_trans.bits(), 
                      img_trans.bytesPerLine(),
                      canvas_rect, 
                      (int*)canvas_->bits(),
                      canvas_->bytesPerLine(),
                      128,  // alpha.
                      0,
                      0,
                      0); 
      } else {
        // cur_frame == 0
        uchar* mask = new uchar[frame_width_ * frame_height_];
        memset(mask, 0xff, frame_width_ * frame_height_);
        frame_infos_[cur_frame_].set_canvas_mask(mask);
        
        // Draw result on canvas.
        QPainter painter(canvas_.get());
        painter.translate(-canvas_offset_);
        painter.drawImage(QPoint(0,0), img);        
      }
      
      frame_infos_[cur_frame_].set_canvas_frame(true);
    }
    ++cur_frame_;
  }
}

QImage* VideoAnalyzer::ComputeAndAllocateNewCanvas(QPointF* canvas_offset) const {
  // Get rectangle of covered frames.
  // TODO: fix to compute optimal overlay ...  
  QPointF frame_top_left(0, 0);
  QPointF frame_bottom_right(frame_width_, frame_height_);
  
  QRectF frame_rect(0, 0, frame_width_, frame_height_);
  
  QPointF canvas_top_left = frame_top_left;
  QPointF canvas_bottom_right = frame_bottom_right;
  
  QRectF rc;
  for (vector<FrameInfo>::const_iterator fi = frame_infos_.begin();
       fi != frame_infos_.end(); ++ fi) {
    
    // Bounding rect.
    rc = fi->get_transform().mapRect(frame_rect);
    canvas_top_left = MinPt(canvas_top_left, rc.topLeft());
    canvas_bottom_right = MaxPt(canvas_bottom_right, rc.bottomRight());
  }
  
  int dx = ceil(canvas_bottom_right.x() - canvas_top_left.x())+1;
  int dy = ceil(canvas_bottom_right.y() - canvas_top_left.y())+1;
  
  QImage* canvas = new QImage(dx, dy, QImage::Format_ARGB32);
  canvas->fill(0);
  
  if (canvas_offset)
    *canvas_offset = canvas_top_left;
  
  return canvas;
}

float VideoAnalyzer::FrameCenterDist(const QTransform& t1, const QTransform& t2,
                                     int frame_width, int frame_height) const {
  
  // Transform frame center by t1 and t2 -> calculate difference.
  QPointF center(frame_width / 2.0, frame_height / 2.0);
  QPointF center1 = center * t1;
  QPointF center2 = center * t2;
  
  QPointF diff = center1 - center2;
  float n = std::sqrt(diff.x() * diff.x () + diff.y() * diff.y());
  
  float diam = std::sqrt(frame_width * frame_width + frame_height * frame_height);
  return n / diam;
}


void VideoAnalyzer::CancelJob() {
  is_canceled_ = true;
}

bool VideoAnalyzer::ReadSpacePlayerFeatures() {
  string feat_file = (video_file_.left(video_file_.lastIndexOf(".")) + ".feat").toStdString();
  std::ifstream ifs(feat_file.c_str(), std::ios_base::in);
  
  string term;
  int frame_number, level;
  float pos_x, pos_y;
  float phi, scale;
  QTransform transform;
  
  while (ifs) {
    
    if (ifs.peek() == std::char_traits<char>::eof())
      break;
    
    ifs >> term >> frame_number >> pos_x >> pos_y;
    bool similarity_model = false;
    
    if (ifs.peek() == ' ') {
      similarity_model = true;
      ifs >> scale >> phi;
    }
    
    if (term != "Frame#")
      return false;
    
    FrameInfo fi;
    
    if (similarity_model) {
      float c = cos(phi);
      float s = sin(phi);
      
      transform = QTransform(c * scale, 
                             s * scale, 
                            -s * scale,
                             c * scale,
                             pos_x,
                             pos_y).inverted();
      fi.set_transform(transform);
      
    } else {
      transform = transform * QTransform::fromTranslate(pos_x, pos_y);
      fi.set_transform(transform);
    }
    
    ifs >> term >> level;
    if (term != "Level#")
      return false;
    
    fi.set_hierarchy_level(level);
    
    ifs >> term;
    if (term != "ForegroundProbs:")
      return false;
    
    string rest;
    std::getline(ifs, rest);
    vector<pair<int, uchar> > region_probs;

    if (!rest.empty()) {
      std::istringstream iss(rest);
      while (iss && iss.peek() != '\n') {
        int region;
        int prob;
        iss >> region >> prob;
        region_probs.push_back(std::make_pair<int, uchar>(region, prob));
      }
    }
    
    fi.set_region_probs(region_probs);
    frame_infos_.push_back(fi);
  }
  
  /*
  // Apply 1D bilateral filter to each.
  ImageFilter::BilateralFilter1D(disp_x, 3, 10, &disp_x);
  ImageFilter::BilateralFilter1D(disp_y, 3, 10, &disp_y);
  
  std::ofstream ofs_model("trans_model.txt", std::ios_base::out);
  for (int i = 0; i < disp_x.size(); ++i) {
    if (i != 0)
      ofs_model << "\n";
    
    ofs_model << disp_x[i] << " " << disp_y[i];
    
    frame_infos_[i].set_transform(QTransform::fromTranslate(disp_x[i], disp_y[i]));
  }
  ofs_model.close();
  */
  
  return true;
}

float min3(float* a, float* b, float* c) {
  return std::min(*a, std::min(*b, *c));
}

int min3idx(float* a, float* b, float* c) {
  if (*a <= *b && *a <= *c)
    return 0;
  else if (*b <= *a && *b <= *c)
    return 1;
  else
    return 2;
}