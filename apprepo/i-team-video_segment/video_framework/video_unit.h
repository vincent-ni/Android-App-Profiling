/*
 *  video_unit.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/21/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_UNIT_H__
#define VIDEO_UNIT_H__

#include "data_frame_creator.h"

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
using boost::posix_time::ptime;

#include <list>
#include <vector>
#include <string>

#ifdef _WIN32
typedef __int64 int64_t;
#endif

#pragma warning( disable : 654 )

typedef struct _IplImage IplImage;

namespace cv {
  class Mat;
}

namespace VideoFramework {
  typedef unsigned int uint;
  typedef unsigned char uchar;
  using boost::shared_ptr;
  using std::list;
  using std::vector;
  using std::string;

  enum PixelFormat { PIXEL_FORMAT_RGB24, PIXEL_FORMAT_BGR24,
                     PIXEL_FORMAT_ARGB32, PIXEL_FORMAT_ABGR32,
                     PIXEL_FORMAT_RGBA32, PIXEL_FORMAT_BGRA32,
                     PIXEL_FORMAT_YUV422,
                     PIXEL_FORMAT_LUMINANCE };

  int PixelFormatToNumChannels(PixelFormat);

  // Basic Frame container that holds any binary data of a specific size.
  // A derived class can opt to save all its data into the binary data_ member
  // but this is not required.

  // In addition, each derived class have to use the macro
  // DECLARE_DATA_FRAME(CLASS_NAME)
  // within the class scope in the header file and
  // DEFINE_DATA_FRAME(CLASS_NAME)
  // in the implementation file, for factory based intiantiation of Frames.
  // Note: If no default constructor is supplied it has to be at least declared private
  // in order to allow the creation of empty frames.

  class DataFrame : boost::noncopyable {
  public:
    DataFrame(uint size = 0) : size_(size) { data_ = (uchar*) malloc(size_); }
    virtual ~DataFrame() { free(data_); }

    const uchar* data() const { return data_; }
    uchar* mutable_data() { return data_; }

    uint size() const { return size_; }

  protected:
    uchar*  data_;
    uint    size_;

    DECLARE_DATA_FRAME(DataFrame)
  };

  template <class T>
  class ValueFrame : public DataFrame {
  public:
    ValueFrame<T>(const T& t = T()) : DataFrame(sizeof(T)) { t_ = t; }

    const T& value() const { return t_; }
    void set_value(const T& t) { t_ = t; }

  private:
    T t_;
    DECLARE_DATA_FRAME(ValueFrame)
  };

  // Convience class for VideoFrames.
  class VideoFrame : public DataFrame {
  public:
    // If you want to use getImageView, make sure to use a width_step
    // that is a multiple of 4.
    VideoFrame (int width, int height, int channels);
    VideoFrame (int width, int height, int channels, int width_step);
    VideoFrame (int width, int height, int channels, int width_step, double pts);

    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }
    int width_step() const { return width_step_; }
    double pts() const { return pts_; }

    // Initialize an IplImage structure so it can be used with OpenCv.
    // Do NOT call cvRelease on this view!
    void ImageView(IplImage* view) const;
    void MatView(cv::Mat* view) const;

  private:
    int width_;
    int height_;
    int channels_;      // A.K.A. bytes per pixel.
    int width_step_;
    double pts_;

    // Empty
    VideoFrame() {};
    DECLARE_DATA_FRAME(VideoFrame)
  };

  // Basic Stream class.
  class DataStream : boost::noncopyable {
  public:
    DataStream(const string& stream_name) : stream_name_(stream_name) {}
    virtual string stream_name() { return stream_name_; }
    virtual ~DataStream() {};
  protected:
    string stream_name_;
  };

  // Convenience class for VideoStreams,
  class VideoStream : public DataStream {
  public:
    VideoStream(int width,
                int height,
                int frame_width_step,
                float fps,
                int frame_count,
                PixelFormat pixel_format = PIXEL_FORMAT_BGR24,
                const string& stream_name = "VideoStream") :
    DataStream(stream_name), frame_width_(width), frame_height_(height),
    frame_width_step_(frame_width_step), fps_(fps), frame_count_(frame_count),
    pixel_format_(pixel_format) {}

    int frame_width() const { return frame_width_; }
    int frame_height() const { return frame_height_; }
    int width_step() const { return frame_width_step_; }

    float fps() const { return fps_; }
    int frame_count () const { return frame_count_; }

    PixelFormat pixel_format() const { return pixel_format_; }

  private:
    int frame_width_;
    int frame_height_;
    int frame_width_step_;
    float fps_;               // Not necessary exact.
    int frame_count_;         // Not necessary exact.
    PixelFormat pixel_format_;
  };

  typedef vector<shared_ptr<DataFrame> > FrameSet;
  typedef vector<shared_ptr<DataStream> > StreamSet;
  typedef shared_ptr<FrameSet> FrameSetPtr;

  class VideoPool;
  class VideoPipelineSource;

  // Base class for any VideoUnit.
  class VideoUnit {
  public:
    // Common interface for all VideoUnit's. Derive and re-implement as desired.
    // Every unit should at least implement OpenStreams and ProcessFrame or the
    // corresponding *FromSender decorator functions.
    // VideoUnit::Run() is designed to make calls to the decorator functions.

    // OpensStreams gets passed a StreamSet that holds all Streams created by parents of
    // the current unit. Use to determine index of streams you are interested in or
    // to add additional streams.
    virtual bool OpenStreams(StreamSet* set) { return true; }

    // TODO(grundman): Rethink feedback streams. Somewhat unreliable.
    // OpenFeedbackStreams gets passed a StreamSet of all streams that are registered
    // to be fed back into the current unit.
    virtual bool OpenFeedbackStreams(const StreamSet& set) { return true; }

    // ProcessFrame gets the current FrameSet as a shared_ptr in input. Use to
    // do any desired processing and push_back to output when done.
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
      output->push_back(input);
    }

    // Optional variant, that is called instead ProcessFrame if feedback from other units
    // is registered. Calls ProcessFrame by standard.
    virtual void ProcessFrame(FrameSetPtr input,
                              const FrameSet& feedback_input,
                              list<FrameSetPtr>* output) {
      ProcessFrame(input, output);
    }

    // Any PostProcessing goes here. Will be called by parent node as long as
    // FrameSetPtr are added to append.
    virtual void PostProcess(list<FrameSetPtr>* append) {}


    // Decorator functions for explicit caller representation.
    // Simply forward to corresponding function by default.
    virtual bool OpenStreamsFromSender(StreamSet* set, const VideoUnit* sender) {
      return OpenStreams(set);
    }

    virtual bool OpenFeedbackStreamsFromSender(const StreamSet& set,
                                               const VideoUnit* sender) {
      return OpenFeedbackStreams(set);
    }

    virtual void ProcessFrameFromSender(FrameSetPtr input,
                                        list<FrameSetPtr>* output,
                                        const VideoUnit* sender) {
      return ProcessFrame(input, output);
    }

    virtual void ProcessFrameFromSender(FrameSetPtr input,
                                        const FrameSet& feedback_input,
                                        list<FrameSetPtr>* output,
                                        const VideoUnit* sender) {
      return ProcessFrame(input, feedback_input, output);
    }

    virtual void PostProcessFromSender(list<FrameSetPtr>* append,
                                       const VideoUnit* sender) {
      return PostProcess(append);
    }

    // Rate management. Notice that rate management occurs always in the direction
    // of the source, i.e. by standard all units propagate ChangeRate calls up to the
    // source, so it can adjust accordingly. Usually only source units like a reader,
    // source or pool unit implement the virtual impl function, while heavy computation
    // units adjust the Rate by calling this function.
    void AdjustRate(float fps) {
      fps = AdjustRateImpl(fps); if (parent_) parent_->AdjustRate(fps); }

  public:
    void AddChild (VideoUnit*);
    void AttachTo (VideoUnit*);
    void RemoveChild(VideoUnit*);
    void RemoveFrom(VideoUnit*);
    bool HasChild (VideoUnit*);

    VideoUnit* ParentUnit() const { return parent_; }
    VideoUnit* RootUnit();

    // Initiate processing in batch.
    bool PrepareAndRun();
    virtual bool Run();

    // Frame based processing.
    bool PrepareProcessing();
    bool NextFrame();

    // Seeking during Frame-based processing.
    // Returns if position was changed due to the call of Seek.
    bool Seek();
    bool Seek(double time);

    // Overload 
    virtual float AdjustRateImpl(float fps) { return fps; }

    // Feedback functionality.
    // The result of a VideoUnit's produced stream can be fed back as input to a
    // previous unit when the next frame is processed.
    // NOTE: This requires the current unit to output only one FrameSet at a time
    //       durnig the call of ProcessFrame. If multiple FrameSets are outputted,
    //       only the first will be fed back. If none are outputted because some
    //       buffering is used via PostProcess none will be fed back and the functionality
    //       is simple ignored.
    //
    void FeedbackResult(VideoUnit* target, const string& stream_name);

    VideoUnit ();
    virtual ~VideoUnit();

  protected:
    // Returns whether the seek resulted in a change of position.
    // Childrens are only called with SeekImpl if return value is true.
    virtual bool SeekImpl(double seek_time) { return true; }

    // Returns index for stream_name in set, -1 if not found.
    int FindStreamIdx(const string& stream_name, const StreamSet* set);

    // The two next preceding rate calls use a circular buffer of size buffer_size
    // to determine current rate and unit rate. Standard is a buffer of 32 frames,
    // adjust to fit your application.
    void SetRateBufferSize(int buffer_size);

    // Time between successive calls of ProcessFrame in fps.
    float CurrentRate() const;

    // Time spent in this units ProcessFrame in fps.
    float UnitRate() const;

  protected:
    virtual bool OpenStreamsImpl(StreamSet*, const VideoUnit* sender);
    virtual bool OpenFeedbackStreamsImpl(const VideoUnit* sender);

    virtual void ProcessFrameImpl(const list<FrameSetPtr>* frame_set,
                                  const VideoUnit* sender);
    virtual void PostProcessImpl(const VideoUnit* sender);

    // Adds Feedback stream and returns target_idx.
    int AddFeedbackStream(shared_ptr<DataStream> ds);
    void AddFeedbackFrame(shared_ptr<DataFrame> df, int target_idx);

  private:
    list<VideoUnit*> children_;
    VideoUnit* parent_;

    struct FeedbackInfo {
      FeedbackInfo(const string& _stream_name, VideoUnit* _target)
          : stream_name(_stream_name), stream_idx(-1), target_idx(-1), target(_target) {}

      string stream_name;
      int    stream_idx;
      int    target_idx;
      VideoUnit* target;
    };

    list<FeedbackInfo> feedback_info_;

    // Temporarily storage that gets passed into OpenFeedbackStreams,
    // and will be freed afterwards.
    // Do not access directly.
    StreamSet feedback_streams_;

    int stream_sz_;
    int feedback_stream_sz_;

    FrameSet next_feedback_frames_;
    FrameSet current_feedback_frames_;

    int rate_buffer_size_;
    boost::circular_buffer<float> current_rate_buffer_;
    boost::circular_buffer<float> unit_rate_buffer_;
    ptime prev_call_time_;

    friend class VideoPool;
    friend class VideoPipelineSource;
  };

}  // namespace VideoFramework.

#endif // VIDEO_UNIT_H__
