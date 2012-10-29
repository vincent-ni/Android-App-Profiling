/*
 *  video_unit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/21/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_unit.h"
#include <algorithm>
#include <functional>
#include <numeric>

#include <opencv2/core/core_c.h>
#include <opencv2/core/core.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include "assert_log.h"

namespace VideoFramework {

  DEFINE_DATA_FRAME(DataFrame)
  DEFINE_DATA_FRAME(VideoFrame)

  int PixelFormatToNumChannels(PixelFormat pix_fmt) {
    switch (pix_fmt) {
      case PIXEL_FORMAT_RGB24:
        return 3;
      case PIXEL_FORMAT_BGR24:
        return 3;
      case PIXEL_FORMAT_RGBA32:
      case PIXEL_FORMAT_ARGB32:
      case PIXEL_FORMAT_BGRA32:
      case PIXEL_FORMAT_ABGR32:
        return 4;
      case PIXEL_FORMAT_YUV422:
        return 2;
      case PIXEL_FORMAT_LUMINANCE:
        return 1;
      default:
        std::cerr << "PixelFormatToNumChannels: unknown pixel format";
        return 0;
    }
  }

  VideoFrame::VideoFrame(int width, int height, int channels) :
      DataFrame(width*height*channels),
      width_(width),
      height_(height),
      channels_(channels),
      width_step_(width * channels),
      pts_(0) {
  }

  VideoFrame::VideoFrame(int width, int height, int channels, int width_step) :
      DataFrame(width_step*height),
      width_(width),
      height_(height),
      channels_(channels),
      width_step_(width_step),
      pts_(0) {
  }

  VideoFrame::VideoFrame(int width,
                         int height,
                         int channels,
                         int width_step,
                         double pts) : DataFrame(width_step*height),
                                       width_(width),
                                       height_(height),
                                       channels_(channels),
                                       width_step_(width_step),
                                       pts_(pts) {
  }

  void VideoFrame::ImageView(IplImage* view) const {
    ASSURE_LOG(view) << "View not set.";
    ASSERT_LOG(width_step_ % 4 == 0) << "IplImages need to have width_step_ "
                                     << "divisable by 4.";
    cvInitImageHeader(view, cvSize(width_, height_), IPL_DEPTH_8U, channels_);
    cvSetData(view, data_, width_step_);
  }

  void VideoFrame::MatView(cv::Mat* view) const {
    ASSURE_LOG(view) << "View not set.";
    *view = cv::Mat(height_, width_, CV_8UC(channels_), (void*)data_, width_step_);
  }

  VideoUnit::VideoUnit() : parent_(0), feedback_stream_sz_(0) {
    rate_buffer_size_ = 32;
    current_rate_buffer_.set_capacity(rate_buffer_size_);
    unit_rate_buffer_.set_capacity(rate_buffer_size_);
  }

  VideoUnit::~VideoUnit() {

  }

  void VideoUnit::AddChild(VideoUnit* v) {
    children_.push_back(v);
    v->parent_ = this;
  }

  void VideoUnit::AttachTo(VideoUnit* v) {
    v->children_.push_back(this);
    parent_ = v;
  }

  void VideoUnit::RemoveChild(VideoUnit* v) {
    list<VideoUnit*>::iterator i = std::find(children_.begin(), children_.end(), v);
    if (i != children_.end()) {
      children_.erase(i);
      (*i)->parent_ = 0;
    }
  }

  void VideoUnit::RemoveFrom(VideoUnit* v) {
    v->RemoveChild(this);
    parent_ = 0;
  }

  bool VideoUnit::HasChild(VideoUnit* v) {
    return std::find(children_.begin(), children_.end(), v) != children_.end();
  }

  VideoUnit* VideoUnit::RootUnit() {
    VideoUnit* current = this;
    while (current->parent_ != 0)
      current = current->parent_;

    return current;
  }

  bool VideoUnit::PrepareAndRun() {
    if (!PrepareProcessing()) {
      return false;
    }

    Run();
    return true;
  }

  bool VideoUnit::Run() {
    // Initial frame_set is empty.
    PostProcessImpl(0);
    return true;
  }

  bool VideoUnit::PrepareProcessing() {
    // Populate Streams.
    StreamSet stream_set;
    if (!OpenStreamsImpl(&stream_set, 0))
      return false;

    // Open Feedback Streams.
    if (!OpenFeedbackStreamsImpl(0))
      return false;

    return true;
  }

  bool VideoUnit::NextFrame() {
    list<FrameSetPtr> append;
    PostProcessFromSender(&append, 0);
    if (append.empty()) {
      for (list<VideoUnit*>::iterator child = children_.begin();
           child != children_.end();
           ++child) {
        (*child)->PostProcessImpl(this);
      }
      return false;
    }

    for (list<VideoUnit*>::iterator child = children_.begin();
         child != children_.end();
         ++child) {
      (*child)->ProcessFrameImpl(&append, this);
    }

    return true;
  }

  bool VideoUnit::Seek() {
    return Seek(0.0);
  }

  bool VideoUnit::Seek(double seek_time) {
    bool changed_pos = this->SeekImpl(seek_time);
    if(changed_pos) {
      std::for_each(children_.begin(), children_.end(),
                    std::bind2nd(
                        std::mem_fun1_t<bool, VideoUnit, double>(&VideoUnit::Seek),
                        seek_time));

      // Empty feedback frames.
      next_feedback_frames_ = FrameSet(feedback_stream_sz_);
    }

    return changed_pos;
  }

  void VideoUnit::FeedbackResult(VideoUnit* target, const string& stream_name) {
    feedback_info_.push_back(FeedbackInfo(stream_name, target));
  }

  int VideoUnit::FindStreamIdx(const string& stream_name, const StreamSet* set) {
    for (StreamSet::const_iterator i = set->begin(); i != set->end(); ++i) {
      if ((*i)->stream_name() == stream_name) {
        return i - set->begin();
      }
    }

    return -1;
  }

  void VideoUnit::SetRateBufferSize(int buffer_size) {
    current_rate_buffer_.set_capacity(buffer_size);
    unit_rate_buffer_.set_capacity(buffer_size);
  }


  float VideoUnit::CurrentRate() const {
    float total_msecs =
        std::accumulate(current_rate_buffer_.begin(),  current_rate_buffer_.end(), 0.f);
    if (total_msecs > 0) {
      return (float)current_rate_buffer_.size() / total_msecs * 1e3;
    } else {
      return -1;
    }

  }

  float VideoUnit::UnitRate() const {
    float total_msecs =
        std::accumulate(unit_rate_buffer_.begin(),  unit_rate_buffer_.end(), 0.f);
    if (total_msecs) {
      return (float)unit_rate_buffer_.size() / total_msecs * 1e3;
    }
  }

  bool VideoUnit::OpenStreamsImpl(StreamSet* set, const VideoUnit* sender) {
    const int prev_stream_sz = set->size();
    if (!OpenStreamsFromSender(set, sender)) {
      return false;
    }

    // Remember stream size to check consistency.
    stream_sz_ = set->size();

    // Check for duplicate names.
    for (int i = prev_stream_sz; i < stream_sz_; ++i) {
      const string curr_stream_name = (*set)[i]->stream_name();
      if (FindStreamIdx(curr_stream_name, set) < i) {
        LOG(ERROR) << "Duplicate stream found: " << curr_stream_name;
        return false;
      }
    }

    // Search for any feedback streams and save their index.
    for(list<FeedbackInfo>::iterator f = feedback_info_.begin();
        f != feedback_info_.end();
        ++f) {
      f->stream_idx = FindStreamIdx(f->stream_name, set);
      if (f->stream_idx < 0) {
        LOG(ERROR) << "Could not find stream " << f->stream_name  << " for feedback.\n";
        return false;
      } else {
        // Notify target that a Feedback stream will be passed to it.
        f->target_idx = f->target->AddFeedbackStream(set->at(f->stream_idx));
      }
    }

    for (list<VideoUnit*>::iterator child = children_.begin();
         child != children_.end();
         ++child) {
      if (!(*child)->OpenStreamsImpl(set, this))
        return false;
    }

    return true;
  }

  bool VideoUnit::OpenFeedbackStreamsImpl(const VideoUnit* sender) {
    feedback_stream_sz_ = feedback_streams_.size();

    if (feedback_stream_sz_ &&
        !OpenFeedbackStreamsFromSender(feedback_streams_, sender)) {
      return false;
    }

    // Set all next feedback frames to zero.
    next_feedback_frames_ = FrameSet(feedback_stream_sz_);

    for (list<VideoUnit*>::iterator child = children_.begin();
         child != children_.end();
         ++child) {
      if (!(*child)->OpenFeedbackStreamsImpl(this))
        return false;
    }

    feedback_streams_.clear();
    return true;
  }

  void VideoUnit::ProcessFrameImpl(const list<FrameSetPtr>* frame_set,
                                   const VideoUnit* sender) {
    if (!frame_set->empty()) {
      if (feedback_stream_sz_ > 0) {
        // Get current feedback frames.
        current_feedback_frames_.swap(next_feedback_frames_);

        // Initialize next_feedback_frames to be empty.
        next_feedback_frames_ = FrameSet(feedback_stream_sz_);
      }

      for (list<FrameSetPtr>::const_iterator i = frame_set->begin();
           i != frame_set->end();
           ++i) {
        list<FrameSetPtr> output;

        // Get current time.
        ptime time_before_call = boost::posix_time::microsec_clock::local_time();

        // Call derived methods.
        if (feedback_stream_sz_ > 0) {
          ProcessFrameFromSender(*i, current_feedback_frames_, &output, sender);
        } else {
          ProcessFrameFromSender(*i, &output, sender);
        }

        // Check that size of each frame set corresponds with size of streamset.
        for (list<FrameSetPtr>::const_iterator frame_set_iter = output.begin();
             frame_set_iter != output.end();
             ++frame_set_iter) {
          ASSURE_LOG((*frame_set_iter)->size() == stream_sz_)
              << "Unit of type: " << typeid(*this).name()
              << "\nNumber of streams set in OpenStreams not consistent "
              << "with returned FrameSet. " << stream_sz_ << " streams for this unit, "
              << "but FrameSet consists of " << (*frame_set_iter)->size();
        }

        // Measure difference.
        ptime time_after_call = boost::posix_time::microsec_clock::local_time();

        float micros_passed =
            boost::posix_time::time_period(time_before_call,
                                           time_after_call).length().total_microseconds();

        // Store in msecs.
        unit_rate_buffer_.push_back(micros_passed * 1e-3);

        // Get unit rate if prev_call_time_ has value from previous call.
        if (unit_rate_buffer_.size() > 1) {
          float micros_passed =
              boost::posix_time::time_period(
                  prev_call_time_, time_after_call).length().total_microseconds();
          current_rate_buffer_.push_back(micros_passed * 1e-3);
        }

        prev_call_time_ = time_after_call;

        if (!output.empty()) {
          // Save any feedback. Only first frame is processed, if multiple present.
          for(list<FeedbackInfo>::iterator f = feedback_info_.begin();
              f != feedback_info_.end();
              ++f) {
            shared_ptr<DataFrame> df(output.front()->at(f->stream_idx));
            f->target->AddFeedbackFrame(df, f->target_idx);
          }
        }

        // Pass to children immediately.
        for (list<VideoUnit*>::iterator child = children_.begin();
             child != children_.end();
             ++child) {
          (*child)->ProcessFrameImpl(&output, this);
        }
      }
    }
  }

  void VideoUnit::PostProcessImpl(const VideoUnit* sender) {
    while (true) {
      list<FrameSetPtr> append;

      // Get current time.
      ptime time_before_call = boost::posix_time::microsec_clock::local_time();

      PostProcessFromSender(&append, sender);
      if (append.empty()) {
        break;
      }

      // Check that size of each frame set corresponds with size of streamset.
      for (list<FrameSetPtr>::const_iterator frame_set_iter = append.begin();
           frame_set_iter != append.end();
           ++frame_set_iter) {
        ASSURE_LOG((*frame_set_iter)->size() == stream_sz_)
            << "Unit of type: " << typeid(*this).name()
            << "\nNumber of streams set in PostProcessImpl not consistent "
            << "with returned FrameSet. " << stream_sz_ << " streams for this unit, "
            << "but FrameSet consists of " << (*frame_set_iter)->size();
      }

      // Measure difference.
      ptime time_after_call = boost::posix_time::microsec_clock::local_time();

      float micros_passed =
          boost::posix_time::time_period(time_before_call,
                                         time_after_call).length().total_microseconds();

      // Store in msecs.
      float inv_append_sz = 1.0f / append.size();
      for (int i = 0; i < append.size(); ++i) {
        unit_rate_buffer_.push_back(micros_passed  * 1e-3 * inv_append_sz);
      }

      // Get unit rate if prev_call_time_ has value from previous call.
      if (unit_rate_buffer_.size() > 1) {
        float micros_passed =
        boost::posix_time::time_period(
            prev_call_time_, time_after_call).length().total_microseconds();
        for (int i = 0; i < append.size(); ++i) {
          current_rate_buffer_.push_back(micros_passed * 1e-3 * inv_append_sz);
        }
      }

      prev_call_time_ = time_after_call;

      for (list<VideoUnit*>::iterator child = children_.begin();
           child != children_.end();
           ++child) {
        (*child)->ProcessFrameImpl(&append, this);
      }
    }

    for (list<VideoUnit*>::iterator child = children_.begin();
         child != children_.end();
         ++child) {
      (*child)->PostProcessImpl(this);
    }
  }

  int VideoUnit::AddFeedbackStream(shared_ptr<DataStream> ds) {
    feedback_streams_.push_back(ds);
    return feedback_streams_.size() - 1;
  }

  void VideoUnit::AddFeedbackFrame(shared_ptr<DataFrame> df, int target_idx) {
    next_feedback_frames_[target_idx] = df;
  }

}  // namespace VideoFramework.
