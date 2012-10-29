/*
 *  conversion_units.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 7/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "conversion_units.h"

#include "assert_log.h"
#include "image_util.h"
using namespace ImageFilter;

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

namespace VideoFramework {

  LuminanceUnit::LuminanceUnit(const string& video_stream_name,
                               const string& luminance_stream_name) :
  video_stream_name_(video_stream_name), luminance_stream_name_(luminance_stream_name) {

  }

  bool LuminanceUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(video_stream_name_, set);
    ASSURE_LOG(video_stream_idx_ >= 0) << "Could not find video stream!\n";

    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
        set->at(video_stream_idx_).get());
    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();
    pixel_format_ = vid_stream->pixel_format();

    if (pixel_format_ != PIXEL_FORMAT_RGB24 &&
        pixel_format_ != PIXEL_FORMAT_BGR24 &&
        pixel_format_ != PIXEL_FORMAT_RGBA32) {
      LOG(FATAL) << "Unsupported input pixel format!\n";
      return false;
    }

    width_step_ = frame_width_;
    if (width_step_ % 4) {
      width_step_ = width_step_ + (4 - width_step_ % 4);
      ASSERT_LOG(width_step_ % 4 == 0);
    }

    VideoStream* lum_stream = new VideoStream(frame_width_,
                                              frame_height_,
                                              width_step_,
                                              vid_stream->fps(),
                                              vid_stream->frame_count(),
                                              PIXEL_FORMAT_LUMINANCE,
                                              luminance_stream_name_);
    set->push_back(shared_ptr<VideoStream>(lum_stream));
    return true;
  }

  void LuminanceUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    const VideoFrame* frame =
        dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);

    VideoFrame* lum_frame = new VideoFrame(frame_width_, frame_height_, 1, width_step_,
                                           frame->pts());

    IplImage lum_image;
    lum_frame->ImageView(&lum_image);

    // Map from our pixel format to OpenCV format.
    int convert_flags;

    switch (pixel_format_) {
      case PIXEL_FORMAT_RGB24:
        convert_flags = CV_RGB2GRAY;
        break;
      case PIXEL_FORMAT_BGR24:
        convert_flags = CV_BGR2GRAY;
        break;
      case PIXEL_FORMAT_RGBA32:
        convert_flags = CV_RGBA2GRAY;
        break;
      default:
        LOG(FATAL) << "Unsupported input pixel format.\n";
        return;
    }

    cvCvtColor(&image, &lum_image, convert_flags);

    input->push_back(shared_ptr<DataFrame>(lum_frame));
    output->push_back(input);
  }

  void LuminanceUnit::PostProcess(list<FrameSetPtr>* append) {
  }

  FlipBGRUnit::FlipBGRUnit(const string& video_stream_name,
                           const string& output_stream_name)
      : video_stream_name_(video_stream_name),
        output_stream_name_(output_stream_name) {

  }


  bool FlipBGRUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(video_stream_name_, set);

    ASSURE_LOG(video_stream_idx_ >= 0) << "Could not find Video stream!\n";

    const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                        set->at(video_stream_idx_).get());
    ASSURE_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();
    width_step_ = vid_stream->width_step();

    PixelFormat pixel_format = vid_stream->pixel_format();

    if (pixel_format != PIXEL_FORMAT_RGB24 &&
        pixel_format != PIXEL_FORMAT_BGR24) {
      LOG(FATAL) << "Unsupported input pixel format!\n";
      return false;
    }

    PixelFormat output_format = pixel_format == PIXEL_FORMAT_RGB24 ?
    PIXEL_FORMAT_BGR24 : PIXEL_FORMAT_RGB24;

    VideoStream* output_stream = new VideoStream(frame_width_, frame_height_,
                                                 width_step_, vid_stream->fps(),
                                                 vid_stream->frame_count(),
                                                 output_format, output_stream_name_);

    set->push_back(shared_ptr<VideoStream>(output_stream));
    return true;
  }

  void FlipBGRUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);

    VideoFrame* output_frame = new VideoFrame(frame_width_, frame_height_, frame->channels(),
                                              width_step_, frame->pts());

    IplImage out_image;
    output_frame->ImageView(&out_image);

    cvCvtColor(&image, &out_image, CV_RGB2BGR);

    input->push_back(shared_ptr<DataFrame>(output_frame));
    output->push_back(input);
  }

  void FlipBGRUnit::PostProcess(list<FrameSetPtr>* append) {
  }

  bool ColorTwist::OpenStreams(StreamSet* set) {
    video_stream_idx_ = FindStreamIdx(vid_stream_name_, set);

    if (video_stream_idx_ < 0) {
      LOG(ERROR) << "Could not find Video stream!\n";
      return false;
    }

    const VideoStream* vid_stream =
    dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

    ASSERT_LOG(vid_stream);

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    return true;
  }

namespace {
  float clamp_uchar(float c) {
    return std::max(0.f, std::min(255.f, c));
  }
}

  ColorTwist::ColorTwist(const vector<float>& weights,
                         const vector<float>& offsets,
                         const std::string& video_stream_name)
      : weights_(weights),
        offsets_(offsets),
        vid_stream_name_(video_stream_name) {
  }

  void ColorTwist::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    VideoFrame* frame = dynamic_cast<VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);

    for (int i = 0; i < frame_height_; ++i) {
      uchar* color_ptr = RowPtr<uchar>(&image, i);

      for (int j = 0; j < frame_width_; ++j, color_ptr +=3 ) {
        color_ptr[0] = (uchar) clamp_uchar((float)color_ptr[0] * weights_[0] +
                                           offsets_[0]);
        color_ptr[1] = (uchar) clamp_uchar((float)color_ptr[1] * weights_[1] +
                                           offsets_[1]);
        color_ptr[2] = (uchar) clamp_uchar((float)color_ptr[2] * weights_[2] +
                                           offsets_[2]);
      }
    }

    output->push_back(input);
  }


}
