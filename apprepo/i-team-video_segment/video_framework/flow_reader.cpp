/*
 *  flow_reader.cpp
 *  SpacePlayer
 *
 *  Created by Matthias Grundmann on 1/19/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "flow_reader.h"
#include "image_util.h"
#include "assert_log.h"

namespace VideoFramework {

  DEFINE_DATA_FRAME(DenseFlowFrame)

  DenseFlowFrame::DenseFlowFrame(int width, int height, bool backward_flow)
      : DataFrame(2 * width * height * sizeof(float)),
        width_(width),
        height_(height),
        backward_flow_(backward_flow) {}

  void DenseFlowFrame::ImageView(IplImage* view, bool flow_x) const {
    cvInitImageHeader(view, cvSize(width_, height_), IPL_DEPTH_32F, 1);
    if (flow_x)
      cvSetData(view, data_, width_ * sizeof(float));
    else
      cvSetData(view, data_ + width_ * height_ * sizeof(float), width_ * sizeof(float));
  }

  bool DenseFlowReaderUnit::OpenStreams(StreamSet* set) {
    vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);
    if (vid_stream_idx_ < 0) {
      LOG(ERROR) << "Can not find video stream.\n";
      return false;
    }

    const VideoStream* vid_stream =
       dynamic_cast<const VideoStream*>(set->at(vid_stream_idx_).get());

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    reader_.OpenAndReadHeader();

    if (reader_.width() != frame_width_ || reader_.height() != frame_height_) {
      LOG(ERROR) << "Flow file has different dimension than input video.\n";
      return false;
    }

    // Add stream.
    if (reader_.FlowType() == FLOW_FORWARD ||
        reader_.FlowType() == FLOW_BOTH) {
      DataStream* flow_stream = new DataStream(forward_flow_stream_name_);
      set->push_back(shared_ptr<DataStream>(flow_stream));
    }

    // Add stream.
    if (reader_.FlowType() == FLOW_BACKWARD ||
        reader_.FlowType() == FLOW_BOTH) {
      DataStream* flow_stream = new DataStream(backward_flow_stream_name_);
      set->push_back(shared_ptr<DataStream>(flow_stream));
    }

    frame_number_ = 0;
    return true;
  }


  void DenseFlowReaderUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    const VideoFrame* frame =
        dynamic_cast<const VideoFrame*>(input->at(vid_stream_idx_).get());
    ASSERT_LOG(frame);

    if (!reader_.MoreFramesAvailable()) {
      input->push_back(
          shared_ptr<DataFrame>(new DenseFlowFrame(0, 0, reader_.FlowType())));
    } else {
      if (frame_number_) {
        if (reader_.FlowType() == FLOW_FORWARD ||
            reader_.FlowType() == FLOW_BOTH) {
          DenseFlowFrame* flow_frame = new DenseFlowFrame(frame_width_,
                                                          frame_height_,
                                                          false);
          reader_.GetNextFlowFrame((uchar*)flow_frame->mutable_data());
          input->push_back(shared_ptr<DataFrame>(flow_frame));
        }

        if (reader_.FlowType() == FLOW_BACKWARD ||
            reader_.FlowType() == FLOW_BOTH) {
          DenseFlowFrame* flow_frame = new DenseFlowFrame(frame_width_,
                                                          frame_height_,
                                                          true);
          reader_.GetNextFlowFrame((uchar*)flow_frame->mutable_data());
          input->push_back(shared_ptr<DataFrame>(flow_frame));
        }
      } else {
        input->push_back(shared_ptr<DataFrame>(
          new DenseFlowFrame(0, 0, reader_.FlowType() == FLOW_BACKWARD)));
        if (reader_.FlowType() == FLOW_BOTH) {
          input->push_back(shared_ptr<DataFrame>(new DenseFlowFrame(0, 0, true)));
        }
      }
    }

    output->push_back(input);
    ++frame_number_;
  }

  void DenseFlowReaderUnit::PostProcess(list<FrameSetPtr>* output) {
  }

  bool DenseFlowReader::OpenAndReadHeader() {
    // Open file.
    LOG(INFO_V1) << "Reading flow from file " << filename_;
    ifs_.open(filename_.c_str(), std::ios_base::in | std::ios_base::binary);
    if (!ifs_) {
      LOG(ERROR) << "DenseFlowReader::OpenAndReadHeader: Can not open binary flow file.\n";
      return false;
    }

    ifs_.read((char*)&width_, sizeof(width_));
    ifs_.read((char*)&height_, sizeof(height_));
    ifs_.read((char*)&flow_type_, sizeof(int));

    return true;
  }

  bool DenseFlowReader::MoreFramesAvailable() {
    return ifs_.peek() != std::char_traits<char>::eof();
  }

  void DenseFlowReader::GetNextFlowFrame(uchar* buffer) {
    ifs_.read((char*)buffer, RequiredBufferSize());
  }


  void DenseFlowReader::ImageViewFromFrame(uchar* buffer,
                                           bool flow_x,
                                           IplImage* view) const {
    cvInitImageHeader(view, cvSize(width_, height_), IPL_DEPTH_32F, 1);
    if (flow_x)
      cvSetData(view, buffer, width_ * sizeof(float));
    else
      cvSetData(view, buffer + width_ * height_ * sizeof(float), width_ * sizeof(float));
  }
}
