/*
 *  video_pool.cpp
 *  video_framework
 *
 *  Created by Matthias Grundmann on 9/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_pool.h"

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include "assert_log.h"
#include "video_writer_unit.h"

namespace VideoFramework {
  
bool VideoPool::OpenStreamsFromSender(StreamSet* set, const VideoUnit* sender) {
  stream_sets_[SenderToId(sender, false)] = *set;
  return true;
}
  
void VideoPool::ProcessFrameFromSender(FrameSetPtr input,
                                       list<FrameSetPtr>* output,
                                       const VideoUnit* sender) {
  // Buffer framesets.
  frame_sets_[SenderToId(sender, true)].push_back(input);
}
  
void VideoPool::PostProcessFromSender(list<FrameSetPtr>* unused, const VideoUnit* sender) {
  input_unit_exhausted_[SenderToId(sender, true)] = true;
  
  // Process all inputs. Output results.
  if (AllInputsExhausted()) {
    LOG(INFO_V1) << "All inputs exhausted. Performing pool operations.";
    // Open Streams.
    OpenPoolStreams(stream_sets_);
    stream_sets_.clear();
    
    // VideoPool acts as sink and source.
    StreamSet stream_set;
    if (!OpenRootStream(&stream_set)) {
      LOG(FATAL) << "Can not open root stream.";
    }
    
    for (list<VideoUnit*>::iterator child = children_.begin(); 
         child != children_.end();
         ++child) {
      if (!(*child)->OpenStreamsImpl(&stream_set, this)) {
        LOG(FATAL) << "Can not open stream on child.";
      }
    }    
    
    // Process all frames.
    ProcessAllPoolFrames(frame_sets_);
    frame_sets_.clear();
    
    // Unit acts as root but uses slightly different functions, replicate Run here. 
    // (Streams we already opened).
    LOG(INFO_V1) << "Acting as source, output frames.";
    while (true) {
      list<FrameSetPtr> append;
      RootPostProcess(&append);
      if (append.empty()) {
        break;
      }
      
      for (list<VideoUnit*>::iterator child = children_.begin();
           child != children_.end(); 
           ++child) {
        (*child)->ProcessFrameImpl(&append, this);    
      }
    }
    
    for (list<VideoUnit*>::iterator child = children_.begin(); child != children_.end(); ++child) {
      (*child)->PostProcessImpl(this);
    }    
  }
}
  
bool VideoPool::OpenStreamsImpl(StreamSet* set, const VideoUnit* sender) {
  if (!OpenStreamsFromSender(set, sender)) {
    return false;
  }
  
  return true;
}
  
void VideoPool::PostProcessImpl(const VideoUnit* sender) {
  // PostProcessFromSender handles calling the children and acts as source
  // once AllInputsExhausted == true.
  list<FrameSetPtr> append;   // Parameter is actually not used.
  PostProcessFromSender(&append, sender);      
}
  
void VideoPool::ImmediatePushToChildren(list<FrameSetPtr>* append) {    
  for (list<VideoUnit*>::iterator child = children_.begin();
       child != children_.end(); 
        ++child) {
    (*child)->ProcessFrameImpl(append, this);    
  }
}
  
int VideoPool::SenderToId(const VideoUnit* sender, bool dont_insert) {
  vector<const VideoUnit*>::const_iterator sender_iter = std::find(video_unit_map_.begin(),
                                                                   video_unit_map_.end(),
                                                                   sender);
  if (sender_iter == video_unit_map_.end()) {
    if (dont_insert) {      
      LOG(FATAL) << "Unit not present in video_unit_map_";
    }
    
    // Insert.
    video_unit_map_.push_back(sender);
    num_input_units_ = video_unit_map_.size();
    
    stream_sets_.resize(num_input_units_);
    frame_sets_.resize(num_input_units_);
    input_unit_exhausted_.resize(num_input_units_, false);
 
    return num_input_units_ - 1;
  } else {
    return sender_iter - video_unit_map_.begin();
  }
}
  
bool VideoPool::AllInputsExhausted() const {
  bool all_inputs_exhausted = true;
  for (vector<bool>::const_iterator unit_exhausted = input_unit_exhausted_.begin();
       unit_exhausted != input_unit_exhausted_.end();
       ++unit_exhausted) {
    if (*unit_exhausted == false) {
      all_inputs_exhausted = false;
    }
  } 
  return all_inputs_exhausted;
}

void MultiSource::AddSource(VideoUnit* unit) {
  root_units_.push_back(unit);
}
  
bool MultiSource::Run() {
  // Prepare all sources.
  for (vector<VideoUnit*>::iterator unit = root_units_.begin();
       unit != root_units_.end();
       ++unit) {
    if ((*unit)->PrepareProcessing() == false) {
      return false;
    }
  }
  
  if (pull_method_ == ROUND_ROBIN) {
    vector<bool> source_exhausted(root_units_.size(), false);
    bool all_sources_exhausted = false;
    while (!all_sources_exhausted) {
      int root_idx = 0;
      all_sources_exhausted = true;
      
      for (vector<VideoUnit*>::iterator unit = root_units_.begin();
           unit != root_units_.end();
           ++unit, ++root_idx) {
        if (!source_exhausted[root_idx]) {
          bool more_frames_available = (*unit)->NextFrame();
          if (more_frames_available) {
            all_sources_exhausted = false;
          } else {
            source_exhausted[root_idx] = true;
          }
        }
      }
    }   // end !all_sources_exhausted.
  } else {
    // Drain each source.
    for (vector<VideoUnit*>::iterator unit = root_units_.begin();
         unit != root_units_.end();
         ++unit) {
      while ((*unit)->NextFrame());
    }
  }
  
  return true;
}
  
}