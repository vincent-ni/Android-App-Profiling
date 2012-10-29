/*
 *  video_pipeline.h
 *  video_framework
 *
 *  Created by Matthias Grundmann on 11/26/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_unit.h"
#include "concurrent_queue.h"

#include <ext/hash_map>
using __gnu_cxx::hash_map;

// Common usage for video pipeline:
// 1. Determine partition of your filter graph that you want to run independently
// 2. Connect disjoint parts by adding a PipelineSink as the last element and a
//    connected PipelineSource (don't use AttachTo) as first element of the next part.
// 3. Call PreprareProcessing on the root
// 4. Place root and each PipelineSource in a separate thread with Run() being
//    the main thread function.
class CvCapture;

namespace VideoFramework {

// Places Frame-Sets into a producer-consumer queue.
class VideoPipelineSource;

enum SINK_POLICY {
  POLICY_NONE,
  // Adjusts rate based on attached_source_fps
  // and increases or decreases rate to keep queue drained.
  ADJUST_RATE_AND_KEEP_DRAINED
};

class VideoPipelineSink : public VideoUnit {
public:
  VideoPipelineSink(SINK_POLICY policy = POLICY_NONE);

  virtual bool OpenStreams(StreamSet* set) { return true; }
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append) { source_exhausted_ = true; }
private:
  // Returns first element from frameset_queue; timeout in msecs.
  bool TryFetchingFrameSet(FrameSetPtr* ptr, int timeout);
  bool IsExhausted() { return source_exhausted_; }
  void ReportAttachedSourceFPS(float fps) { attached_source_fps_ = fps; }

private:
  bool source_exhausted_;
  concurrent_queue<FrameSetPtr> frameset_queue_;

  SINK_POLICY policy_;
  int frame_number_;

  float attached_source_fps_;

  friend class VideoPipelineSource;
};

// Reads Frame-Sets from producer-consumer queue specified by sink
// and passes it to children at a pre-specified rate.
class VideoPipelineSource : public VideoUnit {
public:
  // By standard as fast as possible consumer source.
  VideoPipelineSource(VideoPipelineSink* sink, float target_fps = 0);

  virtual bool OpenStreams(StreamSet* set) { return true; }

  // No input, nothing to do here.
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) { }
  // Dito.
  virtual void PostProcess(list<FrameSetPtr>* append) { }

  // Specialized run. Keeps querying queue until attached sink is exhausted.
  virtual bool Run();

protected:
  VideoPipelineSink* sink_;
  float target_fps_;

  int frame_num_;
  ptime prev_process_time_;
};

// OpenCV allows specific operations only to be called from the main thread,
// eg capturing frames and displaying them.
// This uses a queue of tasks, caller is blocked until result is available.
// (Operations are usually short-while).
struct OpenCVCommand {
  OpenCVCommand() : command_executed(false) { }
  virtual void Execute() = 0;

  boost::condition_variable processed_condition;
  bool command_executed;
};

class OpenCVArbiter {
public:
  // An OpenCVArbiter runs as main thread.
  OpenCVArbiter() { }

  IplImage* QueryFrame(CvCapture* capture);
  void ShowImage(const char* window_name, const IplImage* image);
  int GetKey();

  // It terminates if all passed threads signal readiness to join.
  void SetWaitThreads(vector<boost::thread*>* threads) { threads_ = threads; }
  void Run();

protected:
  vector<boost::thread*>* threads_;
  concurrent_queue<shared_ptr<OpenCVCommand> > command_queue_;

  hash_map<const char*, shared_ptr<IplImage> > frame_buffers_;
  boost::mutex arbiter_mutex_;
};

}  // namespace VideoFramework.
