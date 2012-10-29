#include <iostream>
#include <boost/scoped_ptr.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <fstream>
#include <opencv2/imgproc/imgproc_c.h>
#include <string>

#include "conversion_units.h"
//#include "cmd/gpu_flow_unit.h"
#include "cl_flow_unit.h"
#include "video_unit.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"

namespace po = boost::program_options;
using std::string;

int main(int argc, char** argv) {
  po::options_description desc("Allowed options");
  desc.add_options()
  ("help", "produce this help message")
  ("i", po::value<string>(), "input video file")
// ("iterations", po::value<int>(), "flow iterations")
  ("flow_type", po::value<string>(), "[forward | backward | both]")
  ("renderflow", "visualizes optical flow")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  int flow_type = -1;
  if (vm.count("flow_type")) {
    if (vm["flow_type"].as<string>() == "forward") {
      flow_type = VideoFramework::CLFlowUnit::FLOW_FORWARD;
      std::cout << "Computing forward flow.\n";
    } else if (vm["flow_type"].as<string>() == "backward") {
      flow_type = VideoFramework::CLFlowUnit::FLOW_BACKWARD;
      std::cout << "Computing backward flow.\n";
    } else if (vm["flow_type"].as<string>() == "both") {
      flow_type = VideoFramework::CLFlowUnit::FLOW_BOTH;
      std::cout << "Computing forward and backward flow.\n";
    }
  }

  if (flow_type == -1) {
    std::cout << "flow_type not set or  set to invalid value. "
              << "Only \"forward\", \"backward\" or \"both\" "
              << "supported.\n";
    return 1;
  }

  std::string file;
  if (vm.count("i")) {
    file = vm["i"].as<string>();
    std::cerr << "Flow computation of file " << file << ".\n";
  } else {
    std::cerr << "Please specify input file.\n Usage:\n" << desc;
    return 1;
  }

  // Number of optical flow iterations.
  int iterations = 10;

  if (vm.count("iterations")) {
    iterations = vm["iterations"].as<int>();
  }

  std::cout << "Performing " << iterations << " flow iterations.\n";

  VideoFramework::VideoReaderUnit reader(file);
  VideoFramework::LuminanceUnit lum_unit;

  string video_out = "";
  if (vm.count("renderflow")) {
    video_out = "FlowVideoStream";
  }

  VideoFramework::CLFlowUnit flow_unit(file.substr(0, file.size() - 4) + ".flow",
                                       (VideoFramework::CLFlowUnit::FlowType)flow_type,
                                       iterations,
                                       "LuminanceStream",
                                       "", // No flow backward output stream.
                                       "", // No flow forward output stream.
                                       video_out);
  boost::scoped_ptr<VideoFramework::VideoDisplayUnit> display_unit;
  boost::scoped_ptr<VideoFramework::VideoWriterUnit> writer;
  if (vm.count("renderflow")) {
    display_unit.reset(new VideoFramework::VideoDisplayUnit(video_out));
    writer.reset(new VideoFramework::VideoWriterUnit(
                   file.substr(0, file.size() - 4) + "_flow.mov", video_out));
  }

  lum_unit.AttachTo(&reader);
  flow_unit.AttachTo(&lum_unit);

  if (vm.count("renderflow")) {
    display_unit->AttachTo(&flow_unit);
    writer->AttachTo(&flow_unit);
  }

  std::cerr << "Running.";
  if (!reader.PrepareAndRun()) {
    std::cerr << "Video run not successful!\n";
  }

  return 0;
}
