/*
 *  color_checker.cpp
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "color_checker.h"
#include <highgui.h>
#include "assert_log.h"
#include "image_util.h"

using namespace ImageFilter;

namespace VideoFramework {

  bool ColorCheckerUnit::OpenStreams(StreamSet* set) {
    
    // Open color checker image. (in gray-scale)
    checker_img_ = cvImageToSharedPtr(cvLoadImage(checker_file_.c_str(), 0));
    
    // Get id's contained in the image.
    for (int i = 0; i < checker_img_->height; ++i) {
      uchar* row_ptr = RowPtr<uchar>(checker_img_, i);
      for (int j = 0; j < checker_img_->width; ++j, ++row_ptr) {
        int id= *row_ptr;
        vector<int>::iterator iter = std::lower_bound(checker_ids_.begin(),
                                                      checker_ids_.end(),
                                                      id);
        if (iter == checker_ids_.end() ||
            *iter != id) {
          checker_ids_.insert(iter, id);
        }
      }
    }
    
    video_stream_idx_ = FindStreamIdx(vid_stream_name_, set);
    
    if (video_stream_idx_ < 0) {
      std::cerr << "ColorCheckerUnit::OpenStreams: " 
                << "Could not find Video stream!\n";
      return false;
    }
    
    const VideoStream* vid_stream = 
        dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

    ASSERT_LOG(vid_stream);
    
    frame_width_ = vid_stream->get_frame_width();
    frame_height_ = vid_stream->get_frame_height();    
    
    lum_stream_idx_ = FindStreamIdx(luminance_stream_name_, set);
    if (lum_stream_idx_ < 0) {
      std::cerr << "ColorCheckerUnit::OpenStreams: Luminance stream expected!\n";
      return false;
    }
    
    if (checker_img_->width != frame_width_ ||
        checker_img_->height != frame_height_) {
      std::cerr << "Checker image of different size than video stream!\n";
      return false;
    }
    
    ofs_.open(output_file_.c_str(), std::ios_base::out);
    frame_num_ = 0;
    
    return true;
  }
  
  void ColorCheckerUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);
    
    const VideoFrame* lum_frame = dynamic_cast<const VideoFrame*>(input->at(lum_stream_idx_).get());
    ASSERT_LOG(lum_frame);
    
    IplImage lum_image;
    lum_frame->ImageView(&lum_image);
    
    // Sum according to checker image.
    vector<CvPoint3D32f> colors(256);
    vector<float> pixel_num(256);
    
    for (int i = 0; i < frame_height_; ++i) {
      const uchar* mask_ptr = RowPtr<const uchar>(checker_img_, i);
      uchar* color_ptr = RowPtr<uchar>(&image, i);
      const uchar* lum_ptr = RowPtr<const uchar>(&lum_image, i);
      
      for (int j = 0; j < frame_width_; ++j, ++mask_ptr, color_ptr +=3, ++lum_ptr) {
        int idx = mask_ptr[0];
        if (idx == 0)     // Don't sample background
          continue;
        
        // Is pixel within range?
        int min_val = std::min(std::min(color_ptr[0], color_ptr[1]), color_ptr[2]);
        int max_val = std::max(std::max(color_ptr[0], color_ptr[1]), color_ptr[2]);
        
        if (min_val >= min_thresh_ && max_val <= max_thresh_) {
//        if (lum_ptr[0] >= min_thresh_ && lum_ptr[0] <= max_thresh_) {
          ++pixel_num[idx];
          colors[idx] = colors[idx] + cvPoint3D32f(color_ptr[0],
                                                   color_ptr[1],
                                                   color_ptr[2]);
          // Mark pixel as processed!
          color_ptr[0] ^= 0xff;
          color_ptr[1] ^= 0xff;
          color_ptr[2] ^= 0xff;
        }
      }
    }
    
    if (frame_num_ != 0)
      ofs_ << "\n";
    
    // Normalize and output.
    int processed_colors = 0;
    for (vector<int>::const_iterator c = checker_ids_.begin(); c != checker_ids_.end(); ++c) {
      if (pixel_num[*c] > 0) {
        ++processed_colors;
        colors[*c] = colors[*c] * (1.0 / pixel_num[*c]);
      }
      
      ofs_ << *c << " " << pixel_num[*c] << " " << colors[*c].x << " " 
           << colors[*c].y << " " << colors[*c].z << " ";
    }
    std::cout << "Frame " << frame_num_ << ": Processed colors " << processed_colors << "\n";
    
    ++frame_num_;
    output->push_back(input);
  }
  
  bool ColorTransform::OpenStreams(StreamSet* set) {
    video_stream_idx_ = FindStreamIdx(vid_stream_name_, set);
    
    if (video_stream_idx_ < 0) {
      std::cerr << "ColorCheckerUnit::OpenStreams: " 
      << "Could not find Video stream!\n";
      return false;
    }
    
    const VideoStream* vid_stream = 
    dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());
    
    ASSERT_LOG(vid_stream);
    
    frame_width_ = vid_stream->get_frame_width();
    frame_height_ = vid_stream->get_frame_height();        
    
    ifs_.open(transform_filename_.c_str(), std::ios_base::in);
    
    return true;
  }
  
  float clamp_uchar(float c) {
    return std::max(0.f, std::min(255.f, c));
  }
  
  void ColorTransform::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);
    
    IplImage image;
    frame->ImageView(&image);

    // Read transform weights.
    float weights[3];
    float shift;
    ifs_ >> weights[0] >> weights[1] >> weights[2] >> shift;
    
    for (int i = 0; i < frame_height_; ++i) {
      uchar* color_ptr = RowPtr<uchar>(&image, i);
      
      for (int j = 0; j < frame_width_; ++j, color_ptr +=3 ) {
        color_ptr[0] = (uchar) clamp_uchar((float)color_ptr[0] * weights[0] * shift);
        color_ptr[1] = (uchar) clamp_uchar((float)color_ptr[1] * weights[1] * shift);
        color_ptr[2] = (uchar) clamp_uchar((float)color_ptr[2] * weights[2] * shift);
      }
    }
    
    output->push_back(input);
  }
  
}