/*
 *  video_pipeline.cpp
 *  video_framework
 *
 *  Created by Matthias Grundmann on 11/26/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_pipeline.h"

#include <opencv2/highgui/highgui_c.h>

#include "image_util.h"
#include "assert_log.h"

namespace VideoFramework {

VideoPipelineSink::VideoPipelineSink(SINK_POLICY policy) : policy_(policy) {
  source_exhausted_ = false;
  frame_number_ = 0;
}

void VideoPipelineSink::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  frameset_queue_.push(input);

  if (policy_ == ADJUST_RATE_AND_KEEP_DRAINED
      && frame_number_ > 0 && frame_number_ % 6 == 0) {
    if (frameset_queue_.size() > 5) {
      float adjust_rate = CurrentRate() * 0.8;
      AdjustRate(adjust_rate);
    } else if (frameset_queue_.size() == 1) {
      float adjust_rate = CurrentRate() * 1.2;
      AdjustRate(adjust_rate);
    }
  }

  ++frame_number_;
  output->push_back(input);
}

bool VideoPipelineSink::TryFetchingFrameSet(FrameSetPtr* ptr, int timeout) {
  return frameset_queue_.timed_wait_and_pop(ptr, timeout);
}

VideoPipelineSource::VideoPipelineSource(VideoPipelineSink* sink, float target_fps)
    : sink_(sink), target_fps_(target_fps) {
  AttachTo(sink);
  frame_num_ = 0;
}

bool VideoPipelineSource::Run() {
  while (!sink_->IsExhausted()) {
    FrameSetPtr frame_set_ptr;
    float timeout = 0;
    if (target_fps_ > 0) {
      timeout = 1.0f / target_fps_ * 1000;
    }

    if (sink_->TryFetchingFrameSet(&frame_set_ptr, 1)) {
      // Measure time difference.
      ptime curr_time = boost::posix_time::microsec_clock::local_time();

      if (frame_num_ > 0) {
        float micros_passed =
        boost::posix_time::time_period(
            prev_process_time_, curr_time).length().total_microseconds();
        int wait_time = timeout * 1000 - micros_passed;
        if (wait_time > 0) {
          boost::thread::sleep(boost::get_system_time() +
                               boost::posix_time::microseconds(wait_time));
          curr_time = boost::posix_time::microsec_clock::local_time();
        }

        current_rate_buffer_.push_back(micros_passed * 1e-3);
        sink_->ReportAttachedSourceFPS(CurrentRate());
      }

      prev_process_time_ = curr_time;

      list<FrameSetPtr> append;
      append.push_back(frame_set_ptr);

      // Pass to children.
      for (list<VideoUnit*>::iterator child = children_.begin();
           child != children_.end();
           ++child) {
        (*child)->ProcessFrameImpl(&append, this);
      }

      ++frame_num_;
    }
  }

  // TODO(grundman): Block this call from pipepline_sink.
  PostProcessImpl(this);
  return true;
}

namespace  {

struct QueryFrameCommand : public OpenCVCommand {
  QueryFrameCommand(CvCapture* capture) : capture_(capture) { }
  void Execute() { result_ = cvQueryFrame(capture_); cvWaitKey(1); }

  IplImage* result_;
private:
  CvCapture* capture_;
};

struct ShowFrameCommand : public OpenCVCommand {
  ShowFrameCommand(const char* window_name, const IplImage* img, IplImage* buffer)
      : window_name_(window_name), img_(img), buffer_(buffer) { }
  void Execute() {
    cvCopy(img_, buffer_); cvShowImage(window_name_, buffer_); cvWaitKey(1);
  }

private:
  const IplImage* img_;
  const char* window_name_;
  IplImage* buffer_;
};

struct GetKeyCommand : public OpenCVCommand {
  void Execute() {
    key_ = cvWaitKey(1);
  }

  int key_;
};

}  // anon. namespace.

int OpenCVArbiter::GetKey() {
  shared_ptr<GetKeyCommand> cmd(new GetKeyCommand());
  command_queue_.push(cmd);
  boost::unique_lock<boost::mutex> lock(arbiter_mutex_);
  while (cmd->command_executed == false) {
    cmd->processed_condition.wait(lock);
  }

  return cmd->key_;
}

IplImage* OpenCVArbiter::QueryFrame(CvCapture* capture) {
  shared_ptr<QueryFrameCommand> cmd(new QueryFrameCommand(capture));
  command_queue_.push(cmd);
  boost::unique_lock<boost::mutex> lock(arbiter_mutex_);
  while (cmd->command_executed == false) {
    cmd->processed_condition.wait(lock);
  }

  return cmd->result_;
}

void OpenCVArbiter::ShowImage(const char* window_name, const IplImage* image) {
  shared_ptr<IplImage>& frame_buffer = frame_buffers_[window_name];
  if (frame_buffer == NULL ||
      frame_buffer->width != image->width || frame_buffer->height != image->height) {
    frame_buffer = ImageFilter::cvCreateImageShared(image->width,
                                                    image->height,
                                                    IPL_DEPTH_8U,
                                                    image->nChannels);
  }

  shared_ptr<ShowFrameCommand> cmd(new ShowFrameCommand(window_name, image,
                                                        frame_buffer.get()));

  //shared_ptr<ShowFrameCommand> cmd(new ShowFrameCommand(window_name, image));
  command_queue_.push(cmd);

  boost::unique_lock<boost::mutex> lock(arbiter_mutex_);
  while (cmd->command_executed == false) {
    cmd->processed_condition.wait(lock);
  }
}

void OpenCVArbiter::Run() {
  int run_counter = 0;
  while (true) {
    shared_ptr<OpenCVCommand> cmd;
    if (command_queue_.timed_wait_and_pop(&cmd, 1)) {
      // Execute command.
      cmd->Execute();
      {
        boost::lock_guard<boost::mutex> lock(arbiter_mutex_);
        cmd->command_executed = true;
      }
      cmd->processed_condition.notify_one();
    }

    if (run_counter % 100 == 0) {
      bool all_threads_done = true;
      for (vector<boost::thread*>::const_iterator t = threads_->begin();
           t != threads_->end();
           ++t) {
        if (!(*t)->timed_join(boost::posix_time::milliseconds(0))) {
          all_threads_done = false;
        }
      }

      if (all_threads_done) {
        return;
      }
    }

    cvWaitKey(1);
    ++run_counter;
  }
}



}
