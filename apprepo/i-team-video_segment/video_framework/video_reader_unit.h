/*
 *  VideoReaderUnit.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/24/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_READER_UNIT_H__
#define VIDEO_READER_UNIT_H__

#include "video_unit.h"

struct AVCodec;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace VideoFramework {

class VideoReaderUnit : public VideoUnit {
 public:
  VideoReaderUnit(const string& video_file,
                  int trim_frames = 0,
                  const string& stream_name = "VideoStream",
                  PixelFormat pixel_format = PIXEL_FORMAT_BGR24);
  ~VideoReaderUnit();

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

  virtual bool SeekImpl(double time);
  bool PrevFrame();

  // Can be positive or negative.
  bool SkipFrames(int frame_offset);

 private:
  // Returns allocated VideoFrame (ownership passed to caller).
  // Returns NULL if end of file is reached.
  VideoFrame* ReadNextFrame();

 private:
  string video_file_;
  int trim_frames_;
  int video_stream_idx_;

  string stream_name_;
  PixelFormat pixel_format_;
  int bytes_per_pixel_;

  int frame_num_;
  int frame_width_;
  int frame_height_;
  int frame_width_step_;
  double time_base_;

  AVCodec*  codec_;
  AVCodecContext* codec_context_;
  AVFormatContext* format_context_;
  SwsContext* sws_context_;

  AVFrame* frame_yuv_;
  AVFrame* frame_bgr_;
  // This is used for seeking to a specific frame.
  // If next_packet_ is set, it is read instead of decode being called.
  shared_ptr<AVPacket> next_packet_;

  double current_pos_;
  double fps_;

  bool used_as_root_;
  static bool ffmpeg_initialized_;
};

}

#endif
