/*
 *  video_writer_unit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 10/27/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#define __STDC_CONSTANT_MACROS
#include "video_writer_unit.h"

#include "assert_log.h"

#include <iostream>
#include <cstdio>

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

bool VideoWriterUnit::ffmpeg_initialized_ = false;

VideoWriterUnit::VideoWriterUnit(const string& video_file,
                                 const string& stream_name,
                                 const VideoWriterOptions& options) :
  video_file_(video_file), stream_name_(stream_name), options_(options) {
}

bool VideoWriterUnit::OpenStreams(StreamSet* set) {
  // Setup FFMPEG.
  if (!ffmpeg_initialized_) {
    ffmpeg_initialized_ = true;
    av_register_all();
  }

  // Find video stream index.
  video_stream_idx_ = -1;
  for (StreamSet::const_iterator i = set->begin(); i != set->end(); ++i) {
    if ((*i)->stream_name() == stream_name_) {
      video_stream_idx_ = i - set->begin();
    }
  }

  if (video_stream_idx_ < 0) {
    std::cerr << "VideoWriterUnit::OpenStreams: "
              << "Could not find Video stream!\n";
    return false;
  }

  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                      set->at(video_stream_idx_).get());
  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();
  if (!options_.fps) {
    options_.fps = vid_stream->fps();
  }

  if (!options_.output_format.empty()) {
    output_format_ = guess_format(options_.output_format.c_str(), NULL, NULL);
  } else {
    output_format_ = guess_format(NULL, video_file_.c_str(), NULL);
  }

  output_width_ = frame_width_;
  output_height_ = frame_height_;

  if (options_.scale != 1) {
    if (options_.scale_max_dim || options_.scale_min_dim) {
      LOG(WARNING) << "Scale set, ignoring scale_[max|min]_dim.";
    }
    output_width_ *= options_.scale;
    output_height_ *= options_.scale;
  } else {
    if (options_.scale_max_dim) {
      float max_dim = std::max(frame_width_, frame_height_);
      output_width_ = (float)frame_width_ / max_dim * options_.scale_max_dim;
      output_height_ = (float)frame_height_ / max_dim * options_.scale_max_dim;
    } else if (options_.scale_min_dim) {
      float min_dim = std::min(frame_width_, frame_height_);
      output_width_ = (float)frame_width_ / min_dim * options_.scale_min_dim;
      output_height_ = (float)frame_height_ / min_dim * options_.scale_min_dim;
    }
  }

  int w_reminder = output_width_ % options_.fraction;
  if (w_reminder > 0) {
    if (w_reminder < options_.fraction / 2) {
      output_width_ -= w_reminder;
    } else {
      output_width_ += (options_.fraction - w_reminder);
    }
  }

  int h_reminder = output_height_ % options_.fraction;
  if (h_reminder > 0) {
    if (h_reminder < options_.fraction / 2) {
      output_height_ -= h_reminder;
    } else {
      output_height_ += (options_.fraction - h_reminder);
    }
  }

#ifdef _WIN32
  format_context_ = av_alloc_format_context();
#else
  format_context_ = avformat_alloc_context();
#endif

  if(!format_context_) {
    std::cerr << "VideoWriterUnit::OpenStreams: Could not open format context.\n";
    return false;
  }

  // Set ouput format and filename.
  format_context_->oformat = output_format_;

#ifndef WIN32
  snprintf(format_context_->filename, sizeof(format_context_->filename), "%s",
           video_file_.c_str());
#else
  _snprintf(format_context_->filename, sizeof(format_context_->filename), "%s",
           video_file_.c_str());
#endif

  // Add video stream.
  video_stream_ = av_new_stream(format_context_, 0);
  if (!video_stream_) {
    std::cerr << "VideoWriterUnit::OpenStreams: Could not allocate video stream.\n";
    return false;
  }

  // Set standard parameters.
  codec_context_ = video_stream_->codec;
  codec_context_->codec_id = output_format_->video_codec;  //CODEC_ID_H264;

  // H264 settings.
  /*
  codec_context_->coder_type = FF_CODER_TYPE_AC;
  codec_context_->flags |= CODEC_FLAG_LOOP_FILTER | CODEC_FLAG_GLOBAL_HEADER;
  codec_context_->ildct_cmp = FF_CMP_CHROMA;
  codec_context_->partitions =
  0; //X264_PART_I8X8 | X264_PART_I4X4 | X264_PART_P8X8 | X264_PART_P4X4;
  codec_context_->me_method = ME_HEX;
  codec_context_->me_subpel_quality = 6;
  codec_context_->me_range = 16;
  codec_context_->gop_size = 16;

  codec_context_->keyint_min = 25;
  codec_context_->scenechange_threshold = 40;
  codec_context_->i_quant_factor = 0.71;
  codec_context_->b_frame_strategy = 1;

  codec_context_->qcompress = 0.6;
  codec_context_->qmin = 10;
  codec_context_->qmax = 51;
  codec_context_->max_qdiff = 4;
  codec_context_->max_b_frames = 3;

  codec_context_->bframebias = 3;
  codec_context_->refs = 1;
  codec_context_->directpred = 1;
  codec_context_->trellis = 0;

  codec_context_->flags2 = CODEC_FLAG2_WPRED |
                           CODEC_FLAG2_FASTPSKIP |
                           CODEC_FLAG2_BPYRAMID;

  codec_context_->weighted_p_pred = 2;
*/

  codec_context_->codec_type = CODEC_TYPE_VIDEO;
  codec_context_->bit_rate = options_.bit_rate;
  codec_context_->bit_rate_tolerance = options_.bit_rate;
  codec_context_->width = output_width_;
  codec_context_->height = output_height_;

  LOG(INFO) << "Encoding with " << options_.fps << " fps.";
  codec_context_->time_base = av_d2q(1.0 / options_.fps, 10000);

  codec_context_->pix_fmt = PIX_FMT_YUV420P;

  if (codec_context_->codec_id == CODEC_ID_MPEG2VIDEO) {
    codec_context_->max_b_frames = 2;
  }

  if (codec_context_->codec_id == CODEC_ID_MPEG1VIDEO) {
    codec_context_->mb_decision = 2;
  }

  if(!strcmp(output_format_->name, "mp4") ||
     !strcmp(output_format_->name, "mov") ||
     !strcmp(output_format_->name, "3gp")) {
    codec_context_->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  // Set parameters.
  if (av_set_parameters(format_context_, NULL) < 0) {
    std::cerr << "VideoWriterUnit::OpenStreams: Invalid output format parameters.\n";
    return false;
  }

  // Find and open codec.
  codec_ = avcodec_find_encoder(codec_context_->codec_id);
  if (!codec_) {
    std::cerr << "VideoWriterUnit::OpenStreams: Codec not found.\n";
    return false;
  }

  if (avcodec_open(codec_context_, codec_) < 0) {
    std::cerr << "VideoWriterUnit::OpenStreams: Could not open codec.\n";
    return false;
  }

  video_buffer_size_ = std::max(200000, frame_width_ * frame_height_ * 4);
  video_buffer_ = (uint8_t*)av_malloc(video_buffer_size_);

  frame_encode_ = avcodec_alloc_frame();
  frame_bgr_ = avcodec_alloc_frame();

  if (!frame_bgr_ || !frame_encode_) {
    std::cerr << "VideoWriterUnit::OpenStreams: Could not alloc tmp. images.\n";
    return false;
  }

  uint8_t* encode_buffer =
      (uint8_t*)av_malloc(avpicture_get_size(codec_context_->pix_fmt,
                                             codec_context_->width,
                                             codec_context_->height));

  avpicture_fill((AVPicture*)frame_encode_, encode_buffer, codec_context_->pix_fmt,
                 codec_context_->width, codec_context_->height);

  uint8_t* bgr_buffer = (uint8_t*)av_malloc(avpicture_get_size(PIX_FMT_BGR24,
                                                               frame_width_,
                                                               frame_height_));
  avpicture_fill((AVPicture*)frame_bgr_,
                 bgr_buffer,
                 PIX_FMT_BGR24,
                 frame_width_,
                 frame_height_);

  // Open output file, if needed.
  if(!(output_format_->flags & AVFMT_NOFILE)) {
    if (url_fopen(&format_context_->pb, video_file_.c_str(), URL_WRONLY) < 0) {
      std::cerr << "VideoWriterUnit::OpenStreams: Could not open "
                << video_file_ << ".\n";
      return false;
    }
  }

  av_write_header(format_context_);

  // Setup color conversion.
  sws_context_ = sws_getContext(frame_width_,
                                frame_height_,
                                PIX_FMT_BGR24,
                                codec_context_->width,
                                codec_context_->height,
                                codec_context_->pix_fmt,
                                SWS_BICUBIC,
                                NULL,
                                NULL,
                                NULL);

  if (!sws_context_) {
    std::cerr << "VideoWriterUnit::OpenStreams: Could initialize sws_context.\n";
    return false;
  }

  frame_num_ = 0;
  return true;
}

void VideoWriterUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  // Write single frame.
  const VideoFrame* frame = dynamic_cast<const VideoFrame*>(
                                input->at(video_stream_idx_).get());
  assert(frame);

  //video_pts = (double)video_st->pts.val * video_st->time_base.num / video_st->time_base.den;
  //if (!video_st || video_pts >= STREAM_DURATION))
    //break;

  // Copy video_frame to frame_bgr_.
  const uchar* src_data = frame->data();
  uchar* dst_data = frame_bgr_->data[0];

  for (int i = 0;
       i < frame_height_;
       ++i, src_data += frame->width_step(), dst_data += frame_bgr_->linesize[0])
    memcpy(dst_data, src_data, 3*frame_width_);

  // Convert bgr picture to codec.
  sws_scale(sws_context_, frame_bgr_->data, frame_bgr_->linesize, 0, frame_height_,
            frame_encode_->data, frame_encode_->linesize);

  // Encode.
  int ret_val;
  if (format_context_->flags & AVFMT_RAWPICTURE) {
    AVPacket packet;
    av_init_packet(&packet);

    packet.flags |= PKT_FLAG_KEY;
    packet.stream_index = video_stream_->index;
    packet.data = (uint8_t*)frame_encode_;
    packet.size = sizeof(AVPicture);

    ret_val = av_write_frame(format_context_, &packet);
  } else {
    frame_encode_->pts = ++frame_num_; //frame->pts() / av_q2d(codec_context_->time_base);
    int output_size = avcodec_encode_video(codec_context_,
                                           video_buffer_,
                                           video_buffer_size_,
                                           frame_encode_);
    if (output_size > 0) {
      AVPacket packet;
      av_init_packet(&packet);

      if (codec_context_->coded_frame->pts != AV_NOPTS_VALUE) {
        packet.pts = av_rescale_q(codec_context_->coded_frame->pts,
                                  codec_context_->time_base,
                                  video_stream_->time_base);
       }

      if (codec_context_->coded_frame->key_frame) {
        packet.flags |= PKT_FLAG_KEY;
      }

      packet.stream_index = video_stream_->index;
      packet.data = video_buffer_;
      packet.size = output_size;

      ret_val = av_write_frame(format_context_, &packet);
    } else {
      ret_val = 0;
    }

    if (ret_val != 0) {
      std::cerr << "VideoWriterUnit::ProcessFrame: Error while writing frame.\n";
    }
  }

  output->push_back(input);
}

void VideoWriterUnit::PostProcess(list<FrameSetPtr>* append) {
  // Close file.
  av_write_trailer(format_context_);

  if(!output_format_->flags & AVFMT_NOFILE) {
    url_fclose(format_context_->pb);
  }

  // Free resources.
  avcodec_close(codec_context_);
  av_free(frame_encode_->data[0]);
  av_free(frame_encode_);

  av_free(frame_bgr_->data[0]);
  av_free(frame_bgr_);

  av_free(video_buffer_);

  for (uint i = 0; i < format_context_->nb_streams; ++i) {
    av_freep(&format_context_->streams[i]->codec);
    av_freep(&format_context_->streams);
  }

  av_free(format_context_);
}

int VideoWriterUnit::LineSize() const {
  return frame_bgr_->linesize[0];
}

}
