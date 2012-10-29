/*
 *  conversion_units.h
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 7/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef CONVERSION_UNITS_H__
#define CONVERSION_UNITS_H__

#include "video_unit.h"

namespace VideoFramework {

  class LuminanceUnit : public VideoUnit {
  public:
    LuminanceUnit(const string& video_stream_name = "VideoStream",
                  const string& luminance_stream_name = "LuminanceStream");
    virtual ~LuminanceUnit() {}

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);
  private:
    string video_stream_name_;
    string luminance_stream_name_;

    int video_stream_idx_;
    PixelFormat pixel_format_;

    int frame_width_;
    int frame_height_;
    int width_step_;
  };

  class FlipBGRUnit : public VideoUnit {
  public:
    FlipBGRUnit(const string& video_stream_name = "VideoStream",
                const string& output_stream_name = "VideoStream2");
    virtual ~FlipBGRUnit() {}

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);
  private:
    string video_stream_name_;
    string output_stream_name_;

    int video_stream_idx_;

    int frame_width_;
    int frame_height_;
    int width_step_;
  };

  // Applies a global color twist weight[color] * color_value + offset[value].
  class ColorTwist : public VideoUnit {
  public:
    ColorTwist(const vector<float>& weights,
               const vector<float>& offsets,
               const std::string& video_stream_name = "VideoStream");

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append) {}

  protected:
    vector<float> weights_;
    vector<float> offsets_;

    std::string vid_stream_name_;
    int video_stream_idx_;

    int frame_width_;
    int frame_height_;
    int frame_num_;
  };

}

#endif // CONVERSION_UNITS_H__
