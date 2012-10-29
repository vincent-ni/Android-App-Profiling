
// Simple Command Line Tool for testing the Flow Library

#ifndef GPU_FLOW_UNIT_H__
#define GPU_FLOW_UNIT_H__

#include "video_unit.h"

#include <boost/scoped_ptr.hpp>
#include <fstream>

class TVL1;

namespace {
class Mat;
}

namespace VideoFramework {

// Specify to computed flow type via flow_type. Flow is written to file (if specified)
// or output as streams (if set).
// Optional video_out_stream_name renders backward (or forward if type=FLOW_FORWARD)
// flow as image, where angle is denoted by hue while saturation and value are
// determined from flow magnitude.
class CLFlowUnit : public VideoUnit {
public:
  enum FlowType { FLOW_FORWARD = 0, FLOW_BACKWARD = 1, FLOW_BOTH = 2 };
  CLFlowUnit(const string& out_file = "",
             FlowType flow_type = FLOW_BACKWARD,
             int flow_iterations = 40,
             const string& input_stream_name = "LuminanceStream",
             const string& backward_flow_stream_name =
               "BackwardFlowStream",
             const string& forward_flow_stream_name = "",
             const string& video_out_stream_name = "");
  ~CLFlowUnit();

  virtual bool OpenStreams(StreamSet* set);
  virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
  virtual void PostProcess(list<FrameSetPtr>* append);

private:
  int video_stream_idx_;

  shared_ptr<TVL1> flow_lib_;

  std::string out_file_;
  FlowType flow_type_;
  int flow_iterations_;

  std::string input_stream_name_;
  std::string forward_flow_stream_name_;
  std::string backward_flow_stream_name_;
  std::string video_out_stream_name_;

  int frame_number_;
  int frame_width_;
  int frame_height_;
  int width_step_;
  std::ofstream ofs_;

  shared_ptr<cv::Mat> prev_img_;
};

}  // namespace VideoFramework.

#endif  // GPU_FLOW_UNIT_H__
