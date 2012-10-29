/*
 *  seg_tree.cpp
 *  seg_tree_sample
 *
 *  Created by Matthias Grundmann on 9/26/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include <boost/scoped_ptr.hpp>
using boost::scoped_ptr;
#include <boost/filesystem.hpp>
using boost::filesystem::path;
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>
#include <string>

#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "assert_log.h"
#include "conversion_units.h"
#include "depth.h"
#include "flow_reader.h"
#include "image_filter.h"
#include "image_util.h"
#include "optical_flow_unit.h"
#include "segmentation_unit.h"
#include "video_capture_unit.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_display_qt_unit.h"
#include "video_pipeline.h"
#include "video_writer_unit.h"
#include "sift_unit.h"
#include "warping.h"

using namespace ImageFilter;
namespace po = boost::program_options;

void SegmentFromFile(const string& passed_file,
                     const string& depth_file,
                     const po::variables_map& vm) {
  // Setup input.
  scoped_ptr<VideoFramework::VideoReaderUnit> reader;
  scoped_ptr<VideoFramework::VideoReaderUnit> depth_reader;
  scoped_ptr<VideoFramework::DepthVideoConverterUnit> depth_converter;

  // Path of input file.
  path file_path(".");
  path depth_file_path(".");

  // Filename stripped of ending, e.g. /home/foo/bar.mov -> /home/foo/bar
  string file_base;

  path my_path(passed_file);
  file_path = my_path.parent_path();
  string file_name = passed_file;

  string depth_file_name;
  if (vm.count("depth")) {
    path my_depth_path(depth_file);
    depth_file_path = my_depth_path.parent_path();
    depth_file_name = depth_file;
  }

  if (vm.count("trim_to")) {
    const int trim_frames = vm["trim_to"].as<int>();
    reader.reset(new VideoFramework::VideoReaderUnit(file_name, trim_frames));
  } else {
    reader.reset(new VideoFramework::VideoReaderUnit(file_name));
  }

  VideoFramework::VideoUnit* segment_input_unit = reader.get();
  if (vm.count("depth")) {
    depth_reader.reset(
      new VideoFramework::VideoReaderUnit(depth_file_name,
                                          0,
                                          "DepthVideoStream",
                                          VideoFramework::PIXEL_FORMAT_RGB24));
    depth_reader->AttachTo(reader.get());

    depth_converter.reset(
      new VideoFramework::DepthVideoConverterUnit(true));   // purge video.
    depth_converter->AttachTo(depth_reader.get());

    segment_input_unit = depth_converter.get();
  }

  file_base = file_name.substr(0, file_name.find_last_of("."));

  scoped_ptr<VideoFramework::SiftUnit> sift_unit;
  scoped_ptr<VideoFramework::SiftWriter> sift_writer;
  scoped_ptr<VideoFramework::LuminanceUnit> lum_unit;

  if (vm.count("extract_sift")) {
    lum_unit.reset(new VideoFramework::LuminanceUnit);
    lum_unit->AttachTo(segment_input_unit);
    sift_unit.reset(new VideoFramework::SiftUnit(false));
    sift_unit->AttachTo(lum_unit.get());
    sift_writer.reset(new VideoFramework::SiftWriter(true, file_base + ".sift"));
    sift_writer->AttachTo(sift_unit.get());

    segment_input_unit = sift_writer.get();
  }

  // Setup segmentation options.
  Segment::DenseSegmentOptions seg_options;
  Segment::RegionSegmentOptions region_options;

  SetOptionsFromVariableMap(vm, &seg_options, &region_options);

  // Read in flow if flow file is existent at location.
  scoped_ptr<VideoFramework::DenseFlowReaderUnit> flow_reader;
  bool use_flow = false;

  if (!vm.count("noflow")) {
    std::string flow_file = file_base + ".flow";
    std::ifstream flow_ifs(flow_file.c_str(), std::ios_base::in);
    if (flow_ifs.is_open()) {
      flow_ifs.close();

      // Flow file exists.
      flow_reader.reset(new VideoFramework::DenseFlowReaderUnit(flow_file));
      use_flow = true;
    }
  }

  if (use_flow == false) {
    seg_options.set_flow_stream_name("");
    region_options.set_flow_stream_name("");
  }

  std::string seg_options_string;
  seg_options.SerializeToString(&seg_options_string);

  scoped_ptr<Segment::DenseSegmentUnit> dense_segment;
  if (vm.count("depth")) {
    dense_segment.reset(new Segment::DenseDepthSegmentUnit(seg_options_string));
  } else {
    dense_segment.reset(new Segment::DenseSegmentUnit(seg_options_string));
  }

  if (use_flow) {
    LOG(INFO_V1) << "Using optical flow from file";
    flow_reader->AttachTo(segment_input_unit);
    dense_segment->AttachTo(flow_reader.get());
  } else {
    dense_segment->AttachTo(segment_input_unit);
  }

  VideoFramework::VideoUnit* last_segment_unit = dense_segment.get();
  scoped_ptr<Segment::RegionSegmentUnit> region_segment;
  if (!vm.count("oversegment")) {
    string region_options_string;
    region_options.SerializeToString(&region_options_string);
    if (vm.count("depth")) {
      region_segment.reset(new Segment::RegionDepthSegmentUnit(region_options_string));
    } else {
      region_segment.reset(new Segment::RegionSegmentUnit(region_options_string));
    }
    region_segment->AttachTo(dense_segment.get());
    last_segment_unit = region_segment.get();
  }

  // Optional units.
  scoped_ptr<Segment::SegmentationRenderUnit> display_render;
  scoped_ptr<VideoFramework::VideoDisplayQtUnit> segment_display;
  VideoFramework::VideoDisplayQtUnit video_display("VideoStream");
  video_display.AttachTo(last_segment_unit);

  scoped_ptr<Segment::SegmentationRenderUnit> render1;
  scoped_ptr<Segment::SegmentationRenderUnit> render2;
  scoped_ptr<Segment::SegmentationRenderUnit> render3;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer1;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer2;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer3;

  // Should we display the segmentation?
  if (vm.count("display_level")) {
    float disp_level = vm["display_level"].as<float>();

    display_render.reset(new Segment::SegmentationRenderUnit(0.9,
                         disp_level,
                         true,   // highlight edges
                         false,  // no shape desc
                         true,   // concat with source
                         "VideoStream",
                         "SegmentationStream",
                         "DisplaySegmentStream"));
    display_render->AttachTo(last_segment_unit);

    segment_display.reset(new VideoFramework::VideoDisplayQtUnit("DisplaySegmentStream"));
    segment_display->AttachTo(display_render.get());
  }

  if (vm.count("render_and_save")) {
    render1.reset(new Segment::SegmentationRenderUnit(0.85, 0.6, true, false, false,
                  "VideoStream",
                  "SegmentationStream",
                  "ResultStream1"));
    render2.reset(new Segment::SegmentationRenderUnit(0.85, .3, true, false, false,
                  "VideoStream",
                  "SegmentationStream",
                  "ResultStream2"));
    render3.reset(new Segment::SegmentationRenderUnit(0.85, .15, true, false, false,
                  "VideoStream",
                  "SegmentationStream",
                  "ResultStream3"));

    VideoFramework::VideoWriterOptions options;
    options.bit_rate = 16000000;
    options.fraction = 16;

    writer1.reset(new VideoFramework::VideoWriterUnit(
                    (file_path / "mpeg_out1.mp4").string(), "ResultStream1", options));
    writer2.reset(new VideoFramework::VideoWriterUnit(
                    (file_path / "mpeg_out2.mp4").string(), "ResultStream2", options));
    writer3.reset(new VideoFramework::VideoWriterUnit(
                    (file_path / "mpeg_out3.mp4").string(), "ResultStream3", options));

    render1->AttachTo(last_segment_unit);
    render2->AttachTo(last_segment_unit);
    render3->AttachTo(last_segment_unit);

    writer1->AttachTo(render1.get());
    writer2->AttachTo(render2.get());
    writer3->AttachTo(render3.get());
  }

  scoped_ptr<VideoFramework::LuminanceUnit> flow_lum_unit;
  scoped_ptr<VideoFramework::OpticalFlowUnit> flow_unit;
  scoped_ptr<VideoFramework::FlowUtilityUnit> flow_utility_unit;
  scoped_ptr<VideoFramework::VideoDisplayUnit> flow_display;

  if (vm.count("klt_flow")) {
    flow_lum_unit.reset(new VideoFramework::LuminanceUnit());
    flow_unit.reset(new VideoFramework::OpticalFlowUnit(0.05, 10));
    VideoFramework::FlowUtilityOptions utility_options;
    utility_options.set_plot_flow(true);
    utility_options.set_impose_grid_flow(true);
    utility_options.set_feature_match_file("feature_match_file.pb");
    flow_utility_unit.reset(new VideoFramework::FlowUtilityUnit(utility_options));

    flow_lum_unit->AttachTo(last_segment_unit);
    flow_unit->AttachTo(flow_lum_unit.get());
    flow_utility_unit->AttachTo(flow_unit.get());

    flow_display.reset(new VideoFramework::VideoDisplayUnit("VideoStream"));
    flow_display->AttachTo(flow_utility_unit.get());
  }

  scoped_ptr<Segment::SegmentationWriterUnit> seg_writer;
  if (vm.count("write_to_file")) {
    LOG(INFO) << "Writing result to file " << file_base + "_segment.pb.";
    bool strip = false;
    if (vm.count("strip")) {
      strip = true;
    }
    seg_writer.reset(new Segment::SegmentationWriterUnit(file_base + "_segment.pb",
                     strip));
    seg_writer->AttachTo(last_segment_unit);
  }

  if (!reader->PrepareAndRun()) {
    std::cerr << "Video run not successful!\n";
  }
}

void SegmentFromCamera(const po::variables_map& vm) {
  int target_fps = 16;

  scoped_ptr<VideoFramework::VideoCaptureUnit> capture(
    new VideoFramework::VideoCaptureUnit("VideoStream", 5, target_fps));

  VideoFramework::VideoUnit* last_segment_unit;
  last_segment_unit = capture.get();

  // Setup dense segmentation options.
  Segment::DenseSegmentOptions seg_options;

  seg_options.set_presmoothing(Segment::DenseSegmentOptions::PRESMOOTH_GAUSSIAN);
  seg_options.set_chunk_size(3);
  seg_options.set_chunk_overlap_ratio(0.2);
  seg_options.set_flow_stream_name("");

  std::string seg_options_string;
  seg_options.SerializeToString(&seg_options_string);

  scoped_ptr<Segment::DenseSegmentUnit> dense_segment;
  dense_segment.reset(new Segment::DenseSegmentUnit(seg_options_string));
  dense_segment->AttachTo(last_segment_unit);

  last_segment_unit = dense_segment.get();

  // Setup region segmentation options.
  Segment::RegionSegmentOptions region_options;

  region_options.set_chunk_set_size(3);
  region_options.set_chunk_set_overlap(1);
  region_options.set_flow_stream_name("");

  string region_options_string;
  region_options.SerializeToString(&region_options_string);

  scoped_ptr<Segment::RegionSegmentUnit> region_segment;
  region_segment.reset(new Segment::RegionSegmentUnit(region_options_string));
  region_segment->AttachTo(last_segment_unit);

  last_segment_unit = region_segment.get();

  //  Setup displays for the segmentation
  scoped_ptr<Segment::SegmentationRenderUnit> display_render;
  scoped_ptr<VideoFramework::VideoDisplayQtUnit> segment_display;
  float disp_level = vm["display_level"].as<float>();

  display_render.reset(new Segment::SegmentationRenderUnit(
                         0.9,
                         disp_level,
                         true,   // highlight edges
                         false,  // no shape desc
                         true,   // concat with source
                         "VideoStream",
                         "SegmentationStream",
                         "DisplaySegmentStream"));
  display_render->AttachTo(last_segment_unit);

  last_segment_unit = display_render.get();

  segment_display.reset(new VideoFramework::VideoDisplayQtUnit(
                          "DisplaySegmentStream"));
  segment_display->AttachTo(last_segment_unit);

  last_segment_unit = segment_display.get();

  //  Go
  if (!capture->PrepareProcessing()) {
    LOG(ERROR) << "Video prepare not successful!\n";
  } else if (!capture->PrepareAndRun()) {
    LOG(ERROR) << "Video run not successful!\n";
  }
}

int main(int argc, char** argv) {
  po::options_description desc("Allowed options");

  // Options to customize segmentation behavior.
  Segment::SetupOptionsDescription(&desc);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  std::string passed_file;
  std::string depth_file = "";

  if (vm.count("i")) {
    passed_file = vm["i"].as<string>();
    std::cout << "Segmenting input file " << passed_file << ".\n";
  } else {
    std::cout << "Please specify input file.\n Usage:\n" << desc;
    return 1;
  }

  if (vm.count("depth")) {
    depth_file = vm["depth"].as<string>();
    std::cout << "... with depth file " << depth_file << ".\n";
  }

  Log<LogToStdErr>::ReportLevel() = INFO_V1;

  if (passed_file == "CAMERA") {
    SegmentFromCamera(vm);
  } else {
    SegmentFromFile(passed_file, depth_file, vm);
  }

  std::cout << "Peter, lach mal.\n";
  return 0;
}
