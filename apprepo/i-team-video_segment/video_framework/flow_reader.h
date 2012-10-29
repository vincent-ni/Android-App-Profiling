/*
 *  flow_reader.h
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/19/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */


#ifndef FLOW_READER_H__
#define FLOW_READER_H__

#include <fstream>
#include "video_unit.h"

namespace VideoFramework {

  class DenseFlowFrame : public DataFrame {
  public:
    DenseFlowFrame(int width, int height, bool backward_flow);

    int width() const {return width_;}
    int width_step() const {return width_ * sizeof(float);}
    int height() const {return height_;}
    bool is_backward_flow() const { return backward_flow_; }

    void ImageView(IplImage* view, bool flow_x) const;

  private:
    DenseFlowFrame() : width_(0), height_(0) {}

  protected:
    int width_;
    int height_;
    bool backward_flow_;

    DECLARE_DATA_FRAME(DenseFlowFrame)
  };

  enum DenseFlowType { FLOW_FORWARD = 0, FLOW_BACKWARD = 1, FLOW_BOTH = 2 };
  class DenseFlowReader {
  public:
    DenseFlowReader(const string& filename)
        : filename_(filename), width_(0), height_(0), flow_type_(FLOW_FORWARD) {}

    bool OpenAndReadHeader();

    int RequiredBufferSize() const { return sizeof(float) * width_ * height_ * 2; }
    // If flow type is FLOW_BOTH, two calls are necessary, first call returns forward,
    // second one backward flow.
    void GetNextFlowFrame(uchar* buffer);
    bool MoreFramesAvailable();

    int width() const { return width_; }
    int height() const { return height_; }

    int FlowType() const { return flow_type_; }
    void ImageViewFromFrame(uchar* buffer, bool flow_x, IplImage* view) const;

  private:
    const string filename_;

    int width_;
    int height_;
    DenseFlowType flow_type_;

    std::ifstream ifs_;
  };

  class DenseFlowReaderUnit : public VideoUnit {
  public:
    // Based on flow file type either one or both flow streams are added.
    DenseFlowReaderUnit(const std::string& file,
                        const std::string& video_stream_name = "VideoStream",
                        const std::string& backward_flow_stream_name = "BackwardFlowStream",
                        const std::string& forward_flow_stream_name = "ForwardFlowStream")
        : flow_file_(file),
          vid_stream_name_(video_stream_name),
          backward_flow_stream_name_(backward_flow_stream_name),
          forward_flow_stream_name_(forward_flow_stream_name),
          reader_(file) {}

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

  protected:
    std::string flow_file_;
    std::string vid_stream_name_;
    int vid_stream_idx_;
    std::string backward_flow_stream_name_;
    std::string forward_flow_stream_name_;

    DenseFlowReader reader_;

    int frame_width_;
    int frame_height_;
    DenseFlowType flow_type_;
    int frame_number_;
  };
}

#endif  // FLOW_READER_H__
