/*
 *  color_checker.h
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef COLOR_CHECKER_H__
#define COLOR_CHECKER_H__

#include "video_unit.h"
#include <fstream>

namespace VideoFramework {
 
  class ColorCheckerUnit : public VideoUnit {
  public:
    ColorCheckerUnit(const std::string& checker_filename,
                     const std::string& output_file,
                     int min_thresh = 0,
                     int max_thresh = 255,
                     const std::string& video_stream_name = "VideoStream",
                     const std::string& luminance_stream_name = "LuminanceStream") 
        : checker_file_(checker_filename), output_file_(output_file), 
          min_thresh_(min_thresh), max_thresh_(max_thresh),
          vid_stream_name_(video_stream_name),
          luminance_stream_name_(luminance_stream_name) {}
    
    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append) {}
    
  protected:
    std::string vid_stream_name_;
    std::string luminance_stream_name_;
    
    std::string checker_file_;
    std::string output_file_;
    int min_thresh_;
    int max_thresh_;
    
    int video_stream_idx_;
    int lum_stream_idx_;
    
    int frame_width_;
    int frame_height_;
    int frame_num_;
    
    vector<int> checker_ids_;
    
    std::ofstream ofs_;
    
    shared_ptr<IplImage> checker_img_;
  };
  
  class ColorTransform : public VideoUnit {
  public:
    ColorTransform(const std::string& transform_filename,
                   const std::string& video_stream_name = "VideoStream")
        : transform_filename_(transform_filename),
          vid_stream_name_(video_stream_name) {}
    
    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append) {}
    
  protected:
    std::string vid_stream_name_;
    std::string transform_filename_;
    int video_stream_idx_;
    
    int frame_width_;
    int frame_height_;
    int frame_num_;
      
    std::ifstream ifs_;
  };
  
  
}

#endif  // COLOR_CHECKER_H__