/*
 *  video_pool.h
 *  video_framework
 *
 *  Created by Matthias Grundmann on 9/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_POOL_H__
#define VIDEO_POOL_H__

#include "video_unit.h"

namespace VideoFramework {

// Joins several VideoUnits as input, by buffering incoming frames until input sources are
// exhausted and calls processing functions on the pool of frames.
// It acts as a sink for multiple incoming units, does joint processing of all frames and 
// act as new output source.
// Derive implementation units from it, implementing OpenPoolStreams and ProcessAllPoolFrames.
  
// TODO: Reject all feedback streams for this unit! Implement if needed.
class VideoPool : public VideoUnit {
 public:
  VideoPool() : num_input_units_(0) {
  }
  
  // Interface to for deriving classes.
  typedef vector< vector<FrameSetPtr> > FrameSetPtrPool;
  virtual void OpenPoolStreams(const vector<StreamSet>& stream_sets) = 0;
  virtual void ProcessAllPoolFrames(const FrameSetPtrPool& frame_sets) = 0;
  
  // Same as for VideoUnit but renamed to act additionally as root. 
  // Invoked after OpenPoolStreams.
  virtual bool OpenRootStream(StreamSet* set) {
    return true;
  }
  
  virtual void RootPostProcess(list<FrameSetPtr>* append) { }
  
    
  virtual bool OpenStreamsFromSender(StreamSet* set, const VideoUnit* sender);

  virtual void ProcessFrameFromSender(FrameSetPtr input,
                                      list<FrameSetPtr>* output,
                                      const VideoUnit* sender);
  
  virtual void PostProcessFromSender(list<FrameSetPtr>* append, const VideoUnit* sender);
  
  int InputUnits() const { return num_input_units_; }
  
protected:
  // Overrides default behavior, does not open streams for children, as it acts as a sink.
  virtual bool OpenStreamsImpl(StreamSet* set, const VideoUnit* sender);
  virtual void PostProcessImpl(const VideoUnit* sender);
  // Use if you don't want to buffer output in ProcessAllPoolFrames in output on 
  // RootPostProcess but push output immediately to children.
  virtual void ImmediatePushToChildren(list<FrameSetPtr>* append);
  
private:
  // Maps sender pointer to index in StreamSet via the map declared below.
  // Resizes vectors for StreamSet and FrameSets accordingly.
  int SenderToId(const VideoUnit* sender, bool dont_insert);
  // Returns true if input_unit_exhausted_ is set to true for all indices.
  bool AllInputsExhausted() const;
  
  vector<const VideoUnit*> video_unit_map_;
  int num_input_units_;
  
protected:
  vector<StreamSet> stream_sets_;
  vector< vector<FrameSetPtr> > frame_sets_;
  vector<bool> input_unit_exhausted_;
};

// Can act as root unit for several input units, e.g. if a VideoPool is present in a 
// Filter tree.
class MultiSource {
public:
  // Specifies how frames are fetched. 
  enum PullMethod { 
    ROUND_ROBIN,       // One frame at a time from each unit
    DRAIN_SOURCE       // Process all frames from one unit until it is drained, then go
  };                   // to next one.
  
  MultiSource(PullMethod pull_method) : pull_method_(pull_method) {
  }
  
  void AddSource(VideoUnit* unit);
  bool Run();
  
private:
  bool pull_method_;
  vector<VideoUnit*> root_units_;
};
  
}

#endif  // VIDEO_POOL_H__