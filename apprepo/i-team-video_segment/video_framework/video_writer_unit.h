/*
 *  video_writer_unit.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/27/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef VIDEO_WRITER_UNIT_H__
#define VIDEO_WRITER_UNIT_H__

#include "video_unit.h"

typedef unsigned char uint8_t;

struct AVCodec;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVStream;
struct AVOutputFormat;
struct SwsContext;

namespace VideoFramework {

  struct VideoWriterOptions {
    VideoWriterOptions() : bit_rate(2000000),
                           fps(0),
                           scale(1.0),
                           scale_max_dim(0),
                           scale_min_dim(0),
                           fraction(4),
                           output_format() {}
    // Codec settings.
    // Bit-rate default is 2000 kbps
    int bit_rate;

    // Use video streams fps as default.
    float fps;

    // Scale factor of output. Overrides all other scale settings.
    float scale;

    // width and height will be rounded to multiple of fraction.
    int fraction;

    // If > 0, maximum dimension is scaled to fit that value. Can not both be set.
    int scale_max_dim;
    int scale_min_dim;

    // Guess format from format string as default.
    // Call "ffmpeg -formats" to get a list of valid formats.
    string output_format;
  };

  class VideoWriterUnit : public VideoUnit {
  public:
    VideoWriterUnit(const string& video_file,
                    const string& stream_name = "VideoStream",
                    const VideoWriterOptions& options = VideoWriterOptions());

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

    int LineSize() const;
  private:
    string video_file_;
    int video_stream_idx_;
    string stream_name_;

    int frame_width_;
    int frame_height_;

    int output_width_;
    int output_height_;

    AVCodec*  codec_;
    AVCodecContext* codec_context_;
    AVFormatContext* format_context_;
    AVOutputFormat* output_format_;
    AVStream* video_stream_;
    SwsContext* sws_context_;

    AVFrame* frame_encode_;
    AVFrame* frame_bgr_;
    uint8_t* video_buffer_;
    int video_buffer_size_;

    VideoWriterOptions options_;
    int frame_num_;

    static bool ffmpeg_initialized_;
  };

}

#endif // VIDEO_WRITER_UNIT_H__
