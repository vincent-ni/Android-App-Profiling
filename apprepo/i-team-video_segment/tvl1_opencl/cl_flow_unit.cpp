
#include "cl_flow_unit.h"

// system includes
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>

#include <fstream>
#include <boost/scoped_ptr.hpp>
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/highgui.h>
#include <opencv2/imgproc/imgproc.hpp>

#include "assert_log.h"
#include "flow_reader.h"
#include "image_filter.h"
#include "tvl1.hpp"

using namespace cv;

namespace VideoFramework {

CLFlowUnit::CLFlowUnit(const string& out_file,
                       FlowType flow_type,
                       int flow_iterations,
                       const string& input_stream_name,
                       const string& backward_flow_stream_name,
                       const string& forward_flow_stream_name,
                       const string& video_out_stream_name)
  : out_file_(out_file),
    flow_type_(flow_type),
    flow_iterations_(flow_iterations),
    input_stream_name_(input_stream_name),
    backward_flow_stream_name_(backward_flow_stream_name),
    forward_flow_stream_name_(forward_flow_stream_name),
    video_out_stream_name_(video_out_stream_name) {
  flow_lib_.reset(new TVL1());
}

CLFlowUnit::~CLFlowUnit() {

}

bool CLFlowUnit::OpenStreams(StreamSet* set) {
  video_stream_idx_ = FindStreamIdx(input_stream_name_, set);

  if (video_stream_idx_ < 0) {
    std::cerr << "CLFlowUnit::OpenStreams: "
              << "Could not find video stream!\n";
    return false;
  }

  // Prepare flow lib.
  float lambda = 40;
  unsigned int num_iter = flow_iterations_;
  unsigned int num_warps = 3;
  float scale_factor = 0.7f;

  flow_lib_->set_lambda(lambda);
  flow_lib_->set_max_iterations(num_iter);
  flow_lib_->set_max_warps(num_warps);
  flow_lib_->set_scale_factor(scale_factor);

  frame_number_ = 0;

  // Get video stream.
  const VideoStream* vid_stream = dynamic_cast<const VideoStream*>(
                                    set->at(video_stream_idx_).get());
  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();
  int pixel_format = vid_stream->pixel_format();

  if (vid_stream->pixel_format() != PIXEL_FORMAT_LUMINANCE) {
    std::cerr << "CLFlowUnit: Expecting luminance input.\n";
    return false;
  }

  width_step_ = cv::alignSize(frame_width_ * 3, 4);

  if (!forward_flow_stream_name_.empty()) {
    DataStream* flow_stream = new DataStream(forward_flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(flow_stream));
  }

  if (!backward_flow_stream_name_.empty()) {
    DataStream* flow_stream = new DataStream(backward_flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(flow_stream));
  }

  if (!video_out_stream_name_.empty()) {
    VideoStream* video_out_stream =
      new VideoStream(frame_width_,
                      frame_height_,
                      width_step_,
                      vid_stream->fps(),
                      vid_stream->frame_count(),
                      PIXEL_FORMAT_BGR24,
                      video_out_stream_name_);
    set->push_back(shared_ptr<VideoStream>(video_out_stream));
  }

  // Open file.
  if (!out_file_.empty()) {
    ofs_.open(out_file_.c_str(), std::ios_base::out | std::ios_base::binary);

    // Write header.
    int flow_type = flow_type_;
    ofs_.write((char*)&frame_width_, sizeof(frame_width_));
    ofs_.write((char*)&frame_height_, sizeof(frame_height_));
    ofs_.write((char*)&flow_type, sizeof(flow_type));
  }

  return true;
}
void CLFlowUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  // Get raw video frame data.
  const VideoFrame* frame =
    dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());

  cv::Mat frame_view;
  frame->MatView(&frame_view);

  shared_ptr<cv::Mat> curr_img(new cv::Mat(frame_height_, frame_width_, CV_8UC1));
  frame_view.copyTo(*curr_img);

  cv::Mat flow_x, flow_y;
  if (frame_number_ > 0) {
    shared_ptr<DenseFlowFrame> forward_flow_frame;
    shared_ptr<DenseFlowFrame> backward_flow_frame;

    if (flow_type_ == FLOW_FORWARD || flow_type_ == FLOW_BOTH) {
      forward_flow_frame.reset(new DenseFlowFrame(frame_width_, frame_height_, false));
      forward_flow_frame->MatView(&flow_x, &flow_y);
      flow_lib_->optical_flow_tvl1(*prev_img_,
                                   *curr_img,
                                   flow_x,
                                   flow_y);

      if (!forward_flow_stream_name_.empty()) {
        input->push_back(forward_flow_frame);
      }

      if (!out_file_.empty()) {
        // Write to file.
        ofs_.write((char*)flow_x.data,
                   sizeof(float) * frame_width_ * frame_height_);
        ofs_.write((char*)flow_y.data,
                   sizeof(float) * frame_width_ * frame_height_);
      }
    }

    if (flow_type_ == FLOW_BACKWARD || flow_type_ == FLOW_BOTH) {
      backward_flow_frame.reset(new DenseFlowFrame(frame_width_, frame_height_, true));
      backward_flow_frame->MatView(&flow_x, &flow_y);

      flow_lib_->optical_flow_tvl1(*curr_img,
                                   *prev_img_,
                                   flow_x,
                                   flow_y);

      if (!backward_flow_stream_name_.empty()) {
        input->push_back(backward_flow_frame);
      }

      if (!out_file_.empty()) {
        // Write to file.
        ofs_.write((char*)flow_x.data, sizeof(float) * frame_width_ * frame_height_);
        ofs_.write((char*)flow_y.data, sizeof(float) * frame_width_ * frame_height_);
      }
    }

    if (!video_out_stream_name_.empty()) {
      // Convert to HSV color image.
      VideoFrame* vid_frame = new VideoFrame(frame_width_,
                                             frame_height_,
                                             3,
                                             width_step_,
                                             frame->pts());

      cv::Mat rgb_image;
      vid_frame->MatView(&rgb_image);

      cv::Mat hsv_image(frame_height_, frame_width_, CV_8UC3);

      for (int i = 0; i < frame_height_; ++i) {
        const float* x_ptr = flow_x.ptr<float>(i);
        const float* y_ptr = flow_y.ptr<float>(i);
        uint8_t* hsv_ptr = hsv_image.ptr<uint8_t>(i);
        for (int j = 0, cols = flow_x.cols;
             j < cols;
             ++j, hsv_ptr += 3, ++x_ptr, ++y_ptr) {
          hsv_ptr[0] = (uint8_t)((atan2f(*y_ptr, *x_ptr) / M_PI + 1) * 90);
          hsv_ptr[1] = hsv_ptr[2] =
                         (uint8_t)std::min<float>(hypot(*y_ptr, *x_ptr) * 20.0f, 255.0f);
        }
      }

      cv::cvtColor(hsv_image, rgb_image, CV_HSV2BGR);
      input->push_back(shared_ptr<VideoFrame>(vid_frame));
    }

  } else {
    VideoFrame* vid_frame = new VideoFrame(frame_width_,
                                           frame_height_,
                                           3,
                                           width_step_,
                                           frame->pts());

    cv::Mat rgb_image;
    vid_frame->MatView(&rgb_image);
    rgb_image.setTo(0);
    input->push_back(shared_ptr<DataFrame>(vid_frame));
  }

  prev_img_.swap(curr_img);

  output->push_back(input);
  ++frame_number_;
  if (frame_number_ % 10 == 0) {
    std::cerr << "FlowUnit: Processed Frame #" << frame_number_ << "\n";
  }
}

void CLFlowUnit::PostProcess(list<FrameSetPtr>* append) {
  // Close file.
  if (!out_file_.empty()) {
    ofs_.close();
  }
}

}  // namespace VideoFramework.
