/*
 *  play_unit.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 5/17/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "play_unit.h"
#include "buffered_image.h"
#include "compositing_widget.h"

#include "region_flow.h"
#include "image_util.h"
using namespace ImageFilter;

#include <QMessageBox>
#include <QApplication>
#include <QProgressDialog>
#include <QPainter>
#include <QFile>
#include <iostream>
#include <cmath>

#include "segmentation.pb.h"
#include "segmentation_util.h"

using VideoFramework::VideoFrame;
using VideoFramework::VideoStream;

namespace {
  // Helper function to replace IPP functionality.
  void SetSingleChannel(uchar value, uchar* ptr, int width_step, int width, int height,
                        int channels) {
    for (int i = 0; i < height; ++i) {
      uchar* row = ptr + i * width_step;
      for (int j = 0; j < width; ++j, row += channels) {
        *row = value;
      }
    }
  }

  
  void CopyMasked32s(const int* src, int src_width_step, int* dst, int dst_width_step,
                     const uchar* mask, int mask_width_step, int width, int height) {
    for (int i = 0; i < height; ++i) {
      const int* src_row = PtrOffset(src, i * src_width_step);
      int* dst_row = PtrOffset(dst, i * dst_width_step);
      const uchar* mask_row = PtrOffset(mask, i * mask_width_step);
      
      for (int j = 0; j < width; ++j, ++src_row, ++dst_row, ++mask_row) {
        if (*mask_row)
          *dst_row = *src_row;
      }
    }
  }
}

PlayUnit::PlayUnit(CompositingWidget* output, VideoUnit* parent, bool show_first_frame,
                   const QString& video_file, const string& video_stream_name) :
    output_(output), video_stream_name_(video_stream_name), video_file_(video_file), 
    draw_canvas_(false), is_caching_frames_(false), frame_infos_(0), seg_level_(-1),
    selection_mode_(0), scrub_mode_(0) {
  
  AttachTo(parent);
  frames_played_ = 0;
  if (!RootUnit()->PrepareProcessing())
    QMessageBox::critical(NULL, "", "PlayUnit:: Error opening streams");
  else if(show_first_frame) {
    RootUnit()->NextFrame();
  }
      
  play_timer_ = new QTimer(this);
  connect(play_timer_, SIGNAL(timeout()), this, SLOT(PlayLoop()));
  connect(output_, SIGNAL(FrameSelectedFromPt(QPoint)), this, SLOT(SetFrameFromPt(QPoint)));
  connect(output_, SIGNAL(RegionSelectedFromPt(QPoint)), this, SLOT(RegionFromPt(QPoint)));
}

PlayUnit::~PlayUnit() {
  output_->SetSelectionMask(0);
  delete play_timer_;
}

void PlayUnit::Play() {
  frames_displayed_ = 0;
  start_time_ = QTime::currentTime();
  play_timer_->start(1000 / (fps_ * 2));
}

void PlayUnit::Pause() {
  play_timer_->stop();
}

bool PlayUnit::OpenStreams(StreamSet* set) {
  // Find video stream idx.
  video_stream_idx_ = FindStreamIdx(video_stream_name_, set);
  
  if (video_stream_idx_ < 0) {
    QMessageBox::critical(NULL, "", 
                          "PlayUnit::OpenStreams: Could not find Video stream!");
    return false;
  } 
  
  // Analysis streams.
  flow_stream_idx_ = FindStreamIdx("RegionFlow", set);
  oflow_stream_idx_ = FindStreamIdx("OpticalFlow", set);
  inlier_stream_idx_ = FindStreamIdx("InlierRegionStream", set);
  
  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
      set->at(video_stream_idx_).get());
  
  frame_width_ = vid_stream->get_frame_width();
  frame_height_ = vid_stream->get_frame_height();
  fps_ = vid_stream->get_fps();
  frame_guess_ = vid_stream->get_frame_count();
  
  BufferedImage* bi = new BufferedImage(frame_width_, frame_height_, vid_stream->get_width_step());
  output_->SetBufferedImage(bi);
  
  string seg_file = (video_file_.left(video_file_.lastIndexOf(".")) + "_segment8.pb").toStdString();

  // Segmentation available?
  if (QFile(seg_file.c_str()).exists()) {
    seg_ifs_.open(seg_file.c_str(), std::ios_base::in | std::ios_base::binary);
    
    // Read offset for each segmentation frame from header.
    int num_seg_frames_;
    int64_t seg_header_offset_;
    
    seg_ifs_.read(reinterpret_cast<char*>(&num_seg_frames_), sizeof(num_seg_frames_));
    seg_ifs_.read(reinterpret_cast<char*>(&seg_header_offset_), sizeof(seg_header_offset_));
    
    int64_t start_pos = seg_ifs_.tellg();
    seg_ifs_.seekg(seg_header_offset_);
    seg_frame_offset_ = vector<int64_t>(num_seg_frames_);
    for (int i = 0; i < num_seg_frames_; ++i) {
      int64_t pos;
      int64_t time_stamp;
      seg_ifs_.read(reinterpret_cast<char*>(&pos), sizeof(pos));
      seg_ifs_.read(reinterpret_cast<char*>(&time_stamp), sizeof(time_stamp));
      seg_frame_offset_[i] = pos;
    }
    
    selection_mask_.reset(new QImage(frame_width_, frame_height_, QImage::Format_ARGB32));
  }
  
  canvas_cut_.reset(new CanvasCut(frame_width_, frame_height_));
  
  return true;
}


void PlayUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  
  if(is_caching_frames_) {
    IplImage video_frame_view;
    frame->ImageView(&video_frame_view);
    
    shared_ptr<IplImage> frame_copy = cvCreateImageShared(video_frame_view.width,
                                                          video_frame_view.height,
                                                          video_frame_view.depth,
                                                          video_frame_view.nChannels);
    cvCopy(&video_frame_view, frame_copy.get());
    cached_images_.push_back(frame_copy);
    ++frames_played_;
    return;
  }
  
  assert(frame);
  ResetSelection();
  
  // Copy to screen's back-buffer.
  memcpy(output_->GetBufferedImage()->get_back(), frame->get_data(), frame->get_size());
  
  if (flow_stream_idx_ >= 0) {
    vector<int> inlier_regions = 
        dynamic_cast<const VideoFramework::ValueFrame<vector<int> >*>(
        input->at(inlier_stream_idx_).get())->get_value();
    
    DrawOpticalFlow(dynamic_cast<const VideoFramework::RegionFlowFrame*>(
                         input->at(flow_stream_idx_).get()), inlier_regions);
//    DrawOpticalFlow(dynamic_cast<const VideoFramework::OpticalFlowFrame*>(
  //                  input->at(oflow_stream_idx_).get()));
  }
  
  if (frame_infos_) {
    // Update Transform.
    output_->SetTransform(frame_infos_->at(frames_played_).get_transform());
    
    // Merge previous frame with canvas based on graph cut mask.
    if (draw_canvas_ && frames_played_ > 0
        && frame_infos_->at(frames_played_-1).is_canvas_frame()) {
      
      // Get frame to draw on canvas.
      shared_ptr<QImage> old_img = output_->GetBufferedImage()->NewFrontView();
      QImage img = old_img->convertToFormat(QImage::Format_ARGB32);
      uchar* img_data = img.bits();

      // Allocate and reset canvas_frame_origin.
      if (canvas_frame_origin_.get() == 0) {
        int canvas_sz = output_->get_canvas()->width() * output_->get_canvas()->height();
        canvas_frame_origin_.reset(new int[canvas_sz]);
        // Initialize to -1.
        memset(canvas_frame_origin_.get(), 0xff, sizeof(int) * canvas_sz);
      }      
      
      const uchar* mask_data = frame_infos_->at(frames_played_-1).get_canvas_mask();
      
      if (mask_data) {        
        // Get canvas area at previous frame.
        QRectF frame_rect(0, 0, frame_width_, frame_height_);
        QTransform tf = frame_infos_->at(frames_played_-1).get_transform();
        QRectF canvas_rect = tf.mapRect(frame_rect);
        
        canvas_rect = canvas_rect.translated(-1.f * output_->get_canvas_offset());
        
        // Dump mask into img_data and transform with it.
        for (int i = 0; i < frame_height_; ++i) {
          uchar* dst_ptr = PtrOffset(img_data, i * img.bytesPerLine());
          const uchar* mask_ptr = PtrOffset(mask_data, i * frame_width_);
          
          for (int j = 0; j < frame_width_; ++j, ++mask_ptr, dst_ptr += 4) {
            dst_ptr[3] = mask_ptr[0]; // 0xff;  // Uncomment to disable graphcut input.
          }
        }
        
        QImage img_trans = img.transformed(tf, Qt::SmoothTransformation);
       
        // Copy masked image part to canvas and set corresponding alpha values to 128.
        // Update canvas_frame_origin to copied frame index.
        CopyMasked32s((int*)img_trans.bits(), 
                      img_trans.bytesPerLine(),
                      canvas_rect, 
                      (int*)output_->get_canvas()->bits(),
                      output_->get_canvas()->bytesPerLine(),
                      128,                    // alpha
                      canvas_frame_origin_.get(),
                      output_->get_canvas()->width() * sizeof(int),
                      frames_played_ - 1);   // image id.
      } else {
        // Simply unhide specific canvas area.
        QRect frame_rect(0, 0, frame_width_, frame_height_);
        QTransform tf = frame_infos_->at(frames_played_-1).get_transform();
        QPolygon canvas_poly = tf.mapToPolygon(frame_rect);
        
        canvas_poly.translate(-output_->get_canvas_offset().x(),
                              -output_->get_canvas_offset().y());
        QRect canvas_rect = canvas_poly.boundingRect();
        
        for (int i = 0; i < canvas_rect.height(); ++i) {
          uchar* canvas_ptr = output_->get_canvas()->scanLine(canvas_rect.y() + i) + 
                              4 * canvas_rect.x();
          const int y = canvas_rect.y() + i;
          int x = canvas_rect.x();
                    
          for (int j = 0; j < canvas_rect.width(); ++j, ++x, canvas_ptr += 4) {
            if (canvas_poly.containsPoint(QPoint(x, y), Qt::OddEvenFill))
              canvas_ptr[3] = 128;
          }
        }
      }
    }
    
    // Do not draw last image but only if canvas is shown.
    if(draw_canvas_)
      output_->DrawBufferedImage(frames_played_ < frame_infos_->size() - 1);
  }

  output_->GetBufferedImage()->Swap();
  output_->update();

  frames_played_++;
}

bool PlayUnit::SeekImpl(int64_t pts) {
  if (pts == 0)
    frames_played_ = 0;
  else {
    // Find pts via binary search.
    if (frame_infos_) {
      FrameInfo fi;
      fi.set_pts(pts);
      vector<FrameInfo>::const_iterator i = 
      std::lower_bound(frame_infos_->begin(), frame_infos_->end(), fi, FrameInfoPTSComperator());
      
      if (i != frame_infos_->end()) {
        frames_played_ = i - frame_infos_->begin();
        
        if (i->get_pts() != pts) {
          // This should normally not happen.
          std::cerr << "PlayUnit::SeekImpl: Could not locate correct frame!";
        }
      } else {
        return false;
      }
    }
  }
    
  return true;
}

void PlayUnit::PassFrameInfos(vector<FrameInfo>* infos, QPointF canvas_offset) {
  frame_infos_ = infos; 
  canvas_offset_ = QPoint(canvas_offset.x(), canvas_offset.y());
  
  // Compute center of each frame.
  frame_center_.clear();
  QPoint center(frame_width_ / 2, frame_height_ / 2);
  QPoint top_left(0, 0);
  
  for (vector<FrameInfo>::const_iterator i = infos->begin(); i != infos->end(); ++i) {
    frame_center_.push_back(center * i->get_transform() - canvas_offset_);
    frame_top_left_.push_back(top_left * i->get_transform() - canvas_offset_);
  }
}
  
void PlayUnit::SetCanvasAlpha(int alpha) {
  if (!output_->get_canvas())
    return;
  
//  IppiSize roi = { output_->get_canvas()->width(), output_->get_canvas()->height() };
  
//  ippiSet_8u_C4CR(alpha, output_->get_canvas()->bits() + 3, 
//                  output_->get_canvas()->bytesPerLine(), roi);
  
  SetSingleChannel(alpha, output_->get_canvas()->bits() + 3, output_->get_canvas()->bytesPerLine(),
                   output_->get_canvas()->width(), output_->get_canvas()->height(), 4);
  
  output_->update();
}

void PlayUnit::SaveCanvasFrameOrigin(const string& file) const {
  int sz = output_->get_canvas()->width() * output_->get_canvas()->height();
  std::ofstream ofs(file.c_str(), std::ios_base::binary);
  ofs.write((char*)&sz, sizeof(int));
  ofs.write((char*)canvas_frame_origin_.get(), sizeof(int) * sz);
}

void PlayUnit::LoadCanvasFrameOrigin(const string& file) {
  std::ifstream ifs(file.c_str(), std::ios_base::binary);
  
  int sz;
  ifs.read((char*)&sz, sizeof(int));
  canvas_frame_origin_.reset(new int[sz]);
  ifs.read((char*)canvas_frame_origin_.get(), sizeof(int) * sz);
}

void PlayUnit::PlayLoop() {
  QTime cur_time = QTime::currentTime();
  int msecs = start_time_.msecsTo(cur_time);
  
  int frame = msecs / ( 1000 / fps_);
  if (frame > frames_displayed_) {
    ++frames_displayed_;
    if(!RootUnit()->NextFrame()) {
      play_timer_->stop();
      emit(Done());
    }
  }
}

void PlayUnit::SetFrameFromPt(QPoint pt) {
  // Do not draw over canvas.
  draw_canvas_ = false;
  emit(DrawCanvasState(false));
  
  output_->DrawBufferedImage(true);
  
  bool restart_timer = false;
  if(play_timer_->isActive()) {
    Pause();
    restart_timer = true;
  }
  
  int64_t target_pts = -1;
  
  if (scrub_mode_ == 0) {
    // Find closest frame center.
    int min_dist = 1 << 30;
    for (vector<QPoint>::const_iterator i = frame_center_.begin();
         i < frame_center_.end();
         ++i) {
      QPoint test_pt = pt - *i;
      int dist = test_pt.x() * test_pt.x() + test_pt.y() * test_pt.y();
      if (dist < min_dist) {
        min_dist = dist;
        target_pts = (*frame_infos_)[i-frame_center_.begin()].get_pts();
      }
    }
  } else if (scrub_mode_ == 1 && canvas_frame_origin_.get()) {
    // Consult canvas_frame_origin_.
    int idx = *(canvas_frame_origin_.get() + output_->get_canvas()->width() * pt.y() + pt.x());
    if (idx >= 0)
      target_pts = (*frame_infos_)[idx].get_pts();
  }
  
  if (target_pts >= 0) {
   if(RootUnit()->Seek(target_pts))
      RootUnit()->NextFrame();
      
    emit (CancelDone());
  }
  
  if (restart_timer)
    Play();
}

void PlayUnit::RegionFromPt(QPoint pt) {
  if (seg_level_ < 0)
    return;
    
  if(play_timer_->isActive())
    Pause();
  
  QRect rc (frame_top_left_[frames_played_-1].x(), frame_top_left_[frames_played_-1].y(),
            frame_width_, frame_height_);
  
  if (rc.contains(pt)) {
    // Convert to local.
    pt = pt - rc.topLeft();
    
    if (frames_played_ - 1 < seg_frame_offset_.size()) {
      if (cur_segmentation_.get() == 0) {
        cur_segmentation_.reset(new Segment::SegmentationDesc);
        seg_ifs_.seekg(seg_frame_offset_[frames_played_ - 1]);
        int frame_sz;
        seg_ifs_.read(reinterpret_cast<char*>(&frame_sz), sizeof(frame_sz));
        vector<char> frame_data(frame_sz);
        seg_ifs_.read(reinterpret_cast<char*>(&frame_data[0]), frame_sz);
        cur_segmentation_->ParseFromArray(&frame_data[0], frame_sz);
      }
      
      int region_id = Segment::GetOversegmentedRegionIdFromPoint(pt.x(), pt.y(),
                                                                 *cur_segmentation_);
      if (seg_level_ != 0) {
        region_id = GetParentId(region_id, 0, seg_level_, cur_segmentation_->hierarchy());
      }
        
      std::cout << "Selected region " << region_id << " in frame " << frames_played_-1 << "\n";
      bool update_selection_mask = false;
      
      switch (selection_mode_) {
        case 0:   // select frame region
        {
          vector<int>::iterator reg_iter = std::lower_bound(selected_frame_regions_.begin(),
                                                            selected_frame_regions_.end(), 
                                                            region_id);
          if (reg_iter == selected_frame_regions_.end() || *reg_iter != region_id) {
            selected_frame_regions_.insert(reg_iter, region_id);
            update_selection_mask = true;
          }
          
          // Remove from canvas_region if needed.
          reg_iter = std::lower_bound(selected_canvas_regions_.begin(),
                                      selected_canvas_regions_.end(), 
                                      region_id);
          if (reg_iter != selected_canvas_regions_.end() && *reg_iter == region_id)
            selected_canvas_regions_.erase(reg_iter);
          
          break;
        }
        case 1:   // select canvas regions
        {
          vector<int>::iterator reg_iter = std::lower_bound(selected_canvas_regions_.begin(),
                                                            selected_canvas_regions_.end(), 
                                                            region_id);
          if (reg_iter == selected_canvas_regions_.end() || *reg_iter != region_id) {
            selected_canvas_regions_.insert(reg_iter, region_id);
            update_selection_mask = true;
          }
          
          reg_iter = std::lower_bound(selected_frame_regions_.begin(),
                                      selected_frame_regions_.end(), 
                                      region_id);
          
          if (reg_iter != selected_frame_regions_.end() && *reg_iter == region_id)
            selected_frame_regions_.erase(reg_iter);
          
          break;
        }
        case 2:   // Eraser
        {
          vector<int>::iterator reg_iter = std::lower_bound(selected_frame_regions_.begin(),
                                                            selected_frame_regions_.end(), 
                                                            region_id);
          if (reg_iter != selected_frame_regions_.end() && *reg_iter == region_id) {
            selected_frame_regions_.erase(reg_iter);
            update_selection_mask = true;
          }
          
          reg_iter = std::lower_bound(selected_canvas_regions_.begin(),
                                      selected_canvas_regions_.end(), 
                                      region_id);
          if (reg_iter != selected_canvas_regions_.end() && *reg_iter == region_id) {
            selected_canvas_regions_.erase(reg_iter);
            update_selection_mask = true;
          }
        }
        default:
          break;
      }
      
      if (update_selection_mask) {
        selection_mask_->fill(0);
        /*
        // Render alpha
        RenderRegions(selected_frame_regions_, 80, selection_mask_->bits()+3,
                      selection_mask_->bytesPerLine(), selection_mask_->width(),
                      selection_mask_->height(), 4, seg_level_, *cur_segmentation_);
        
          // Render color.
        RenderRegions(selected_frame_regions_, 255, selection_mask_->bits()+1,
                      selection_mask_->bytesPerLine(), selection_mask_->width(),
                      selection_mask_->height(), 4, seg_level_, *cur_segmentation_);
        
        // Render alpha
        RenderRegions(selected_canvas_regions_, 80, selection_mask_->bits()+3,
                      selection_mask_->bytesPerLine(), selection_mask_->width(),
                      selection_mask_->height(), 4, seg_level_, *cur_segmentation_);
        
        // Render color.
        RenderRegions(selected_canvas_regions_, 255, selection_mask_->bits()+2,
                      selection_mask_->bytesPerLine(), selection_mask_->width(),
                      selection_mask_->height(), 4, seg_level_, *cur_segmentation_);
*/
        output_->SetSelectionMask(selection_mask_.get());
        output_->update();
      }
    }
  }
}

void PlayUnit::SegLevelChanged(int level) {
  ResetSelection();
  output_->update();
  seg_level_ = level;
}

void PlayUnit::RegionSelectionMode(int mode) {
  selection_mode_ = mode;
}

void PlayUnit::Stitch() {
  /*
  canvas_cut_->set_canvas(output_->get_canvas());
  
  // Obtain mask for graphcut from selection_mask_
  QPoint canvas_offset = frame_top_left_[frames_played_ - 1];
  
  shared_array<uchar> to_canvas(new uchar[frame_width_ * frame_height_]);
  memset(to_canvas.get(), 0, frame_width_ * frame_height_);
  
  for (int i = 0; i < frame_height_; ++i) {
    uchar* sel_ptr = selection_mask_->scanLine(i);
    uchar* mask_ptr = to_canvas.get() + i * frame_width_;
    for (int j = 0; j < frame_width_; ++j, sel_ptr += 4, ++mask_ptr) {
      if (sel_ptr[1] == 255)
        *mask_ptr = 255;
    }
  }
  
  shared_array<uchar> from_canvas(new uchar[frame_width_ * frame_height_]);
  memset(from_canvas.get(), 0, frame_width_ * frame_height_);
  
  for (int i = 0; i < frame_height_; ++i) {
    uchar* sel_ptr = selection_mask_->scanLine(i);
    uchar* mask_ptr = from_canvas.get() + i * frame_width_;
    for (int j = 0; j < frame_width_; ++j, sel_ptr += 4, ++mask_ptr) {
      if (sel_ptr[2] == 255)
        *mask_ptr = 255;
    }
  }
  
  canvas_cut_->ProcessImage(canvas_offset, output_->GetBufferedImage()->get_front(),
                           frame_width_ * 3, 0, to_canvas.get(), from_canvas.get());
  
  // Copy everything with label 0 onto the canvas and update canvas_frame_origin_.
  uchar* canvas_ptr = output_->get_canvas()->scanLine(canvas_offset.y()) + 4 * canvas_offset.x();      
  int* canvas_origin_ptr = canvas_frame_origin_.get() + 
                           canvas_offset.y() * output_->get_canvas()->width() + canvas_offset.x();
  for (int i = 0, idx = 0; i < frame_height_; ++i) {
    uchar* canvas_row = canvas_ptr + i * output_->get_canvas()->bytesPerLine();
    int* canvas_origin_row = canvas_origin_ptr + i * output_->get_canvas()->width();
    uchar* frame_row = output_->GetBufferedImage()->get_front() +
                       output_->GetBufferedImage()->get_width_step() * i;
    
    for (int j = 0;
         j < frame_width_;
         ++j, canvas_row += 4, frame_row += 3, ++idx, ++canvas_origin_row) {
      if (canvas_cut_->Label(idx) == 0) {
        canvas_row[0] = frame_row[2];
        canvas_row[1] = frame_row[1];
        canvas_row[2] = frame_row[0];
        *canvas_origin_row = frames_played_ - 1;
      }
    }
  }
  output_->update();
   */
}

void PlayUnit::ScrubMode(int mode) {
  scrub_mode_ = mode;
}

void PlayUnit::CanvasClean() {
  // Replace each pixel on the canvas with median.
  const int canvas_width = output_->get_canvas()->width();
  const int canvas_height = output_->get_canvas()->height();
  const int frame_radius = 30;
  
  // Retrieve which frames where actually used for canvas stitching.
  vector<int> is_frame_used(frame_infos_->size(), 0);
  for (int i = 0; i < canvas_height; ++i) {
    int* origin_ptr = canvas_frame_origin_.get() + canvas_width * i;
    for (int j = 0; j < canvas_width; ++j, ++origin_ptr) {
      is_frame_used[*origin_ptr] = 1;
    }
  }
  
  vector<int> used_canvas_frames;
  for (int i = 0; i < is_frame_used.size(); ++i)
    if (is_frame_used[i])
      used_canvas_frames.push_back(i);
      
  // Remember pts of next frame to be played.
  int64_t current_pts = frame_infos_->at(frames_played_-1).get_pts();
  is_caching_frames_ = true;
  
  if(play_timer_->isActive())
    Pause();
  
  // Cache the first 2*frame_radius frames.
  RootUnit()->Seek();  
  cached_images_.clear();
  cached_images_.set_capacity(2*frame_radius + 1);
  
  QProgressDialog progress("Cleaning canvas...", "Cancel", 0, used_canvas_frames.size());
  progress.setMinimumDuration(0);
  progress.setWindowModality(Qt::WindowModal);
                           
  for (vector<int>::const_iterator frame_index = used_canvas_frames.begin();
       frame_index != used_canvas_frames.end();
       ++frame_index) {
    
    progress.setValue(frame_index - used_canvas_frames.begin());
    
    if (progress.wasCanceled()) {
      break;
    }
  
    // Do we have to read more frames?
    while (std::min<int>(frame_infos_->size() - 1, *frame_index + frame_radius) >= frames_played_)
      RootUnit()->NextFrame();
    
    const int start_t = std::max(0, *frame_index - frame_radius);
    const int stop_t = std::min<int>(frame_infos_->size() - 1, *frame_index + frame_radius);
    
    // Map this to our circular buffer.
    // Last element at [cached_images_.size() - 1] holds frames_played_ - 1.
    const int frame_diff = stop_t - start_t;
    const int frame_shift = cached_images_.size() - 1 - frame_diff;
    
    // Process all pixels that used current frame_index as frame source.
    for (int i = 0; i < canvas_height; ++i) {
      int* origin_ptr = canvas_frame_origin_.get() + canvas_width * i;
      uchar* canvas_ptr = output_->get_canvas()->scanLine(i);
      vector<int> median[3];
      
      for (int c = 0; c < 3; ++c)
        median[c].reserve(2*frame_radius + 1);
      
      for (int j = 0; j < canvas_width; ++j, ++origin_ptr, canvas_ptr+=4) {
        if (*origin_ptr == *frame_index) {  // Process this pixel.
          for (int c = 0; c < 3; ++c)
            median[c].clear();

          // Get pixel value from each image.          
          for (int t = start_t, img_t = 0; t <= stop_t; ++t, ++img_t) {
            // Get local image coordinate.
            const int x = j - frame_top_left_[t].x();
            const int y = i - frame_top_left_[t].y();
            if (x >= 0 && x < frame_width_ && y >= 0 && y < frame_height_) {
              uchar* pixel_ptr= RowPtr<uchar>(cached_images_[frame_shift + img_t].get(), y) + 3*x;
              median[0].push_back(pixel_ptr[0]);
              median[1].push_back(pixel_ptr[1]);
              median[2].push_back(pixel_ptr[2]);
            }
          }
          
          // Compute median.
          int color[3];
          if (median[0].size() % 2) {   // odd
            const int idx = median[0].size() / 2;
            for (int c = 0; c < 3; ++c) {
              std::sort(median[c].begin(), median[c].end());
              color[c] = median[c][idx];
            }
          } else {
            const int idx = median[0].size() / 2;
            for (int c = 0; c < 3; ++c) {
              std::sort(median[c].begin(), median[c].end());
              color[c] = (median[c][idx] + median[c][idx - 1]) / 2;
            }
          }
          
          // Write to canvas.
          int r = canvas_ptr[0] - color[2];
          int g = canvas_ptr[1] - color[1];
          int b = canvas_ptr[2] - color[0];
          
//          if (std::sqrt(r*r + g*g + b*b)) {
            canvas_ptr[0] = color[2];
            canvas_ptr[1] = color[1];
            canvas_ptr[2] = color[0];
  //        }
        }
      }
    }
  }
    
  progress.setValue(used_canvas_frames.size());
    
  // Reset to original position.
  is_caching_frames_ = false;
  cached_images_.clear();
  RootUnit()->Seek(current_pts);
  RootUnit()->NextFrame();
}

void PlayUnit::ResetSelection() {
  selected_frame_regions_.clear();
  selected_canvas_regions_.clear();
  cur_segmentation_.reset();
  if (selection_mask_)
    selection_mask_->fill(0);
}

void PlayUnit::DrawOpticalFlow(const VideoFramework::OpticalFlowFrame* frame) {
  typedef VideoFramework::OpticalFlowFrame::TrackedFeature TrackedFeature;
  vector<TrackedFeature> flow;
  
  if (frame->MatchedFrames() == 0) 
    return;
  
  // Find last tracked flow frame.
  int idx = 0;
  for (int i = 0; i < frame->MatchedFrames(); ++i) {
    if (frame->NumberOfFeatures(i))
      idx = i;
  }
  
  frame->FillVector(&flow, idx);
  
  BufferedImage* bi = output_->GetBufferedImage();
  QImage img_view(bi->get_back(), bi->get_width(), bi->get_height(), bi->get_width_step(),
                  QImage::Format_RGB888);
  QPainter painter(&img_view);
  painter.setPen(qRgb(255, 0, 0));
  
  for (vector<TrackedFeature>::const_iterator tf = flow.begin(); tf != flow.end(); ++tf) {
    painter.drawLine(tf->loc.x, tf->loc.y, tf->loc.x + tf->vec.x, tf->loc.y + tf->vec.y);
  }
}

void PlayUnit::DrawOpticalFlow(const VideoFramework::RegionFlowFrame* frame,
                               const vector<int>& inlier_regions) {
  typedef VideoFramework::RegionFlowFrame::RegionFlow RegionFlow;
  vector<RegionFlow> region_flow;
  
  if (frame->MatchedFrames() == 0) 
    return;
  
  // Find last tracked flow frame.
  int idx = 0;
//  for (int i = 0; i < frame->MatchedFrames(); ++i) {
//    if (frame->NumberOfFeatures(i))
//      idx = i;
//  }
  
  frame->FillVector(&region_flow, 0); //frame->MatchedFrames()-1);
  
  BufferedImage* bi = output_->GetBufferedImage();
  QImage img_view(bi->get_back(), bi->get_width(), bi->get_height(), bi->get_width_step(),
                  QImage::Format_RGB888);
  QPainter painter(&img_view);
  painter.setRenderHint(QPainter::Antialiasing);
  
  for (vector<RegionFlow>::const_iterator rf = region_flow.begin(); rf != region_flow.end(); ++rf) {
    // Set color based on inlier, outlier.
    vector<int>::const_iterator find_idx = 
      std::find(inlier_regions.begin(), inlier_regions.end(), rf - region_flow.begin());

    if (find_idx == inlier_regions.end()) {
      painter.setPen(QPen(QColor(255, 0, 0, 200), 1.5));
    } else {
      painter.setPen(QPen(QColor(0, 255, 0, 200), 1.5));
    }
    
    
    
    for (VideoFramework::RegionFlowFrame::FeatList::const_iterator f = rf->features.begin();
         f != rf->features.end();
         ++f) {
      painter.drawLine(f->loc.x, f->loc.y, f->loc.x + f->vec.x, f->loc.y + f->vec.y);
    }
  }
  
  // Save file to disk for video.
//  QString filename = QString("analysis_frame_%1.png").arg(frames_played_, 4, 10, QChar('0'));
//  img_view.save(filename);
  
  /*
  frame->FillVector(&region_flow, 0);
  painter.setPen(qRgb(0, 255, 0));
  
  for (vector<RegionFlow>::const_iterator rf = region_flow.begin(); rf != region_flow.end(); ++rf) {
    for (Segment::RegionFlowFrame::FeatList::const_iterator f = rf->features.begin();
         f != rf->features.end();
         ++f) {
      painter.drawLine(f->loc.x, f->loc.y, f->loc.x + f->vec.x, f->loc.y + f->vec.y);
    }
  }
   */
}