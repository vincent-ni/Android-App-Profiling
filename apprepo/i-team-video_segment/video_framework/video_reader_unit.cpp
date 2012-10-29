/*
 *  VideoReaderUnit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/24/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include "video_reader_unit.h"

#include "assert_log.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

namespace VideoFramework {

  bool VideoReaderUnit::ffmpeg_initialized_ = false;

  VideoReaderUnit::VideoReaderUnit(const string& video_file,
                                   int trim_frames,
                                   const string& stream_name,
                                   PixelFormat pixel_format)
      : video_file_(video_file),
        trim_frames_(trim_frames),
        stream_name_(stream_name),
        pixel_format_(pixel_format) {
    codec_context_ = 0;
    format_context_ = 0;
    frame_bgr_ = 0;
    frame_yuv_ = 0;
  }

  VideoReaderUnit::~VideoReaderUnit() {
    if (frame_bgr_) {
      av_free(frame_bgr_->data[0]);
      av_free(frame_bgr_);
    }

    if (frame_yuv_)
      av_free(frame_yuv_);

    if (codec_context_)
      avcodec_close(codec_context_);
    if (format_context_)
      av_close_input_file(format_context_);
  }

  bool VideoReaderUnit::OpenStreams(StreamSet* set) {
    // Setup FFMPEG.
    if (!ffmpeg_initialized_) {
      ffmpeg_initialized_ = true;
      av_register_all();
    }

    // Open video file.
    AVFormatContext* format_context;
    if (av_open_input_file (&format_context, video_file_.c_str(), 0, 0, 0) != 0) {
      LOG(ERROR) << "Could not open file: " << video_file_;
      return false;
    }

    if (av_find_stream_info(format_context) < 0) {
      LOG(ERROR) << video_file_ << " is not a valid movie file.";
      return false;
    }

    // Get video stream index.
    video_stream_idx_ = -1;

    for (uint i = 0; i < format_context->nb_streams; ++i) {
      if (format_context->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
        video_stream_idx_ = i;
        break;
      }
    }

    if (video_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find video stream in " << video_file_;
      return false;
    }

    AVCodecContext* codec_context = format_context->streams[video_stream_idx_]->codec;
    AVCodec* codec = avcodec_find_decoder (codec_context->codec_id);

    if (!codec) {
      LOG(ERROR) << "Unsupported codec for file " << video_file_;
      return false;
    }

    if (avcodec_open(codec_context, codec) < 0) {
      LOG(ERROR) << "Could not open codec";
      return false;
    }

    AVStream* av_stream = format_context->streams[video_stream_idx_];
    fps_ = av_q2d(av_stream->avg_frame_rate);
    LOG(INFO) << "Frame rate: " << fps_;
    time_base_ = av_q2d(av_stream->time_base);

    // if av_q2d wasn't able to figure out the frame rate, set it 24
    if (fps_ != fps_) {
      LOG(WARNING) << "Can't figure out frame rate - Defaulting to 24";
      fps_ = 24;
    }

    // Limit to meaningful values. Sometimes avg_frame_rate.* holds garbage.
    if (fps_ < 5) {
      fps_ = 5;
    }

    if (fps_ > 60) {
      fps_ = 60;
    }

    int frame_guess = av_stream->nb_frames;
    if (frame_guess == 0) {
      frame_guess = av_stream->duration * time_base_ * fps_;
      LOG(INFO) << "Frames in video: " << frame_guess
                << " (guessed from duration and fps).";
    } else {
      LOG(INFO) << "Frames in video: " << frame_guess << " (specified in stream)";
    }

    //  (int)(fps * (float)format_context->duration / (float)AV_TIME_BASE + 0.5);

    bytes_per_pixel_ = PixelFormatToNumChannels(pixel_format_);

    frame_width_ = codec_context->width;
    frame_height_ = codec_context->height;
    frame_width_step_ = frame_width_ * bytes_per_pixel_;

    // Pad width_step to be a multiple of 4.
    if (frame_width_step_ % 4) {
      frame_width_step_ = frame_width_step_ + (4 - frame_width_step_ % 4);
      ASSERT_LOG(frame_width_step_ % 4 == 0);
    }

    // Save some infos for later use.
    codec_ = codec;
    codec_context_ = codec_context;
    format_context_ = format_context;

    // Allocate temporary structures.
    frame_yuv_ = avcodec_alloc_frame();
    frame_bgr_ = avcodec_alloc_frame();

    if (!frame_yuv_ || !frame_bgr_) {
      LOG(ERROR) << "Could not allocate AVFrames.";
      return false;
    }

    int pix_fmt;
    switch (pixel_format_) {
      case PIXEL_FORMAT_RGB24:
        pix_fmt = PIX_FMT_RGB24;
        break;
      case PIXEL_FORMAT_BGR24:
        pix_fmt = PIX_FMT_BGR24;
        break;
      case PIXEL_FORMAT_ARGB32:
        pix_fmt = PIX_FMT_ARGB;
        break;
      case PIXEL_FORMAT_ABGR32:
        pix_fmt = PIX_FMT_ABGR;
        break;
      case PIXEL_FORMAT_RGBA32:
        pix_fmt = PIX_FMT_RGBA;
        break;
      case PIXEL_FORMAT_BGRA32:
        pix_fmt = PIX_FMT_BGRA;
        break;
      case PIXEL_FORMAT_YUV422:
        pix_fmt = PIX_FMT_YUYV422;
        break;
      case PIXEL_FORMAT_LUMINANCE:
        pix_fmt = PIX_FMT_GRAY8;
        break;
    }

  uint8_t* bgr_buffer = (uint8_t*)av_malloc(avpicture_get_size((::PixelFormat)pix_fmt,
                                                                 frame_width_,
                                                                 frame_height_));

    avpicture_fill((AVPicture*)frame_bgr_,
                   bgr_buffer,
                   (::PixelFormat)pix_fmt,
                   frame_width_,
                   frame_height_);

    // Setup SwsContext for color conversion.
    sws_context_ = sws_getContext(frame_width_,
                                  frame_height_,
                                  codec_context_->pix_fmt,
                                  frame_width_,
                                  frame_height_,
                                  (::PixelFormat)pix_fmt,
                                  SWS_BICUBIC,
                                  NULL,
                                  NULL,
                                  NULL);
    if(!sws_context_) {
      LOG(ERROR) << "Could not setup SwsContext for color conversion.";
      return false;
    }

    current_pos_ = 0;
    used_as_root_ = set->empty();
    VideoStream* vid_stream = new VideoStream(frame_width_,
                                              frame_height_,
                                              frame_width_step_,
                                              fps_,
                                              frame_guess,
                                              pixel_format_,
                                              stream_name_);

    set->push_back(shared_ptr<VideoStream>(vid_stream));

    frame_num_ = 0;
    return true;
  }

  void VideoReaderUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    if (!used_as_root_) {
      input->push_back(shared_ptr<VideoFrame>(ReadNextFrame()));
      output->push_back(input);
      ++frame_num_;
    }
  }

  void VideoReaderUnit::PostProcess(list<FrameSetPtr>* append) {
    if (used_as_root_) {
      VideoFrame* next_frame = ReadNextFrame();
      if (next_frame != NULL) {
        // Make new frameset and push VideoFrame.
        FrameSetPtr frame_set (new FrameSet());
        frame_set->push_back(shared_ptr<VideoFrame>(next_frame));
        append->push_back(frame_set);
        ++frame_num_;
      }
    }
  }

  VideoFrame* VideoReaderUnit::ReadNextFrame() {
    // Read frame by frame.
    AVPacket packet;
    bool frame_processed = false;

    if (trim_frames_ > 0 && frame_num_ == trim_frames_) {
      return NULL;
    }

    // Test if we already have a read a package (e.g. Seek command)
    if (next_packet_) {
      packet = *next_packet_;
      next_packet_.reset();
    } else {
      // No packages left?
      if (av_read_frame(format_context_, &packet) < 0)
        return NULL;
    }

    do {
      if (packet.stream_index == video_stream_idx_) {
        int frame_finished;
        avcodec_decode_video(codec_context_,
                             frame_yuv_,
                             &frame_finished,
                             packet.data,
                             packet.size);

        if (frame_finished) {
          // Convert to BGR.
          sws_scale(sws_context_, frame_yuv_->data, frame_yuv_->linesize, 0,
                    frame_height_, frame_bgr_->data, frame_bgr_->linesize);

          double pts = packet.pts * time_base_;
          current_pos_ = pts;
          // current_dts_unscaled_ = packet.dts;
          //dts_scale_ = packet.duration;

          VideoFrame* cur_frame = new VideoFrame(frame_width_,
                                                 frame_height_,
                                                 bytes_per_pixel_,
                                                 frame_width_step_,
                                                 pts);

          const uchar* src_data = frame_bgr_->data[0];
          uchar* dst_data = cur_frame->mutable_data();

          // Copy to our image.
          for (int i = 0;
               i < frame_height_;
               ++i, src_data += frame_bgr_->linesize[0], dst_data += frame_width_step_) {
            memcpy(dst_data, src_data, bytes_per_pixel_ * frame_width_);
          }

          frame_processed = true;
          av_free_packet(&packet);
          return cur_frame;
        }
      }
      av_free_packet(&packet);
    } while (av_read_frame(format_context_, &packet) >= 0); // Single frame could consists
                                                            // of multiple packages.
    LOG(ERROR) << "Unexpected end of ReadNextFrame()";
    return NULL;
  }

  bool VideoReaderUnit::SeekImpl(double time) {
    if (time == current_pos_)
      return false;

    // Flush buffers.
    int seek_flag = AVSEEK_FLAG_BACKWARD;
    int64_t target_pts = time / time_base_;
    LOG(INFO) << "Seeking to " << time << " from curr_pos @ " << current_pos_
              << " (target_pts: " << target_pts << ")";

    av_seek_frame(format_context_, -1, target_pts, seek_flag);
    avcodec_flush_buffers(codec_context_);

    next_packet_.reset(new AVPacket);
    codec_context_->hurry_up = 1;
    int runs = 0;
    while (runs++ < 1000) {     // Correct frame should not be more than 1000 frames away.
      if (av_read_frame(format_context_, next_packet_.get()) < 0)
        break;

      int64_t pts = next_packet_->pts;

      if (pts >= target_pts) {
        current_pos_ = pts * time_base_;
        break;
      }

      int frame_finished;    // uninteresting here.
      avcodec_decode_video(codec_context_, frame_yuv_, &frame_finished,
                           next_packet_->data, next_packet_->size);
      av_free_packet(next_packet_.get());
    }

    codec_context_->hurry_up = 0;

    return true;
  }

  bool VideoReaderUnit::SkipFrames(int frames) {
	  double seek_time = current_pos_ + (double) frames / fps_;

	  if (seek_time > 0) {
		  if (Seek(seek_time))
			  return NextFrame();
		  else
			  return false;
	  }
	  else {
		  return false;
	  }
  }

  bool VideoReaderUnit::PrevFrame() {
    return SkipFrames(-1);
  }
}
