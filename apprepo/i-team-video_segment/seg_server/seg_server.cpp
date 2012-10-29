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

#include <core/core_c.h>
#include <highgui/highgui_c.h>

#include "assert_log.h"
#include "conversion_units.h"
#include "flow_reader.h"
#include "cmd/gpu_flow_unit.h"
#include "image_filter.h"
#include "image_util.h"
#include "optical_flow_unit.h"
#include "segmentation_unit.h"
#include "video_capture_unit.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_pipeline.h"
#include "video_writer_unit.h"
#include "warping.h"


using namespace ImageFilter;
namespace po = boost::program_options;

bool SegmentFromFile(const string& passed_file, const po::variables_map& vm) {
  // Setup input.
  scoped_ptr<VideoFramework::VideoReaderUnit> reader;
  // Path of input file. 
  path file_path(".");    
  // Filename stripped of ending, e.g. /home/foo/bar.mov -> /home/foo/bar
  string file_base;   
  
  path my_path(passed_file);
  file_path = my_path.parent_path();
  string file_name = passed_file;
    
  if (vm.count("trim_to")) {
    const int trim_frames = vm["trim_to"].as<int>();
    reader.reset(new VideoFramework::VideoReaderUnit(file_name, trim_frames));
  } else {
    reader.reset(new VideoFramework::VideoReaderUnit(file_name));
  }
  
  file_base = file_name.substr(0, file_name.find_last_of("."));
  VideoFramework::VideoUnit* segment_input_unit = reader.get();
    
  // Setup segmentation options.
  Segment::DenseSegmentOptions seg_options;
  Segment::RegionSegmentOptions region_options;
  
  if(!SetOptionsFromVariableMap(vm, &seg_options, &region_options)) {
    std::cerr << "Error in specified options. Aborting.";
    return false;
  }
  
  // Read in flow if flow file is existent at locaiton.
  scoped_ptr<VideoFramework::GPUFlowUnit> gpu_flow;
  scoped_ptr<VideoFramework::LuminanceUnit> luminance_conv;
  bool use_flow = false;
  
  if (!vm.count("noflow")) {
    LOG(INFO) << "Segmenting with optical flow.";
    luminance_conv.reset(new VideoFramework::LuminanceUnit());
    gpu_flow.reset(new VideoFramework::GPUFlowUnit(
        "",  // no file output.
        VideoFramework::GPUFlowUnit::FLOW_BACKWARD,
        10));

    luminance_conv->AttachTo(reader.get());
    gpu_flow->AttachTo(luminance_conv.get());
    segment_input_unit = gpu_flow.get();
    use_flow = true;
  } else {
    seg_options.set_flow_stream_name("");
    region_options.set_flow_stream_name("");
  }
  
  std::string seg_options_string;
  seg_options.SerializeToString(&seg_options_string);
  Segment::DenseSegmentUnit dense_segment(seg_options_string);
  dense_segment.AttachTo(segment_input_unit);
  
  VideoFramework::VideoUnit* last_segment_unit = &dense_segment;
  scoped_ptr<Segment::RegionSegmentUnit> region_segment;
  if (!vm.count("oversegment")) {
    string region_options_string;
    region_options.SerializeToString(&region_options_string);
    region_segment.reset(new Segment::RegionSegmentUnit(region_options_string));
    region_segment->AttachTo(&dense_segment);
    last_segment_unit = region_segment.get();
  }
  
  // Optional units.
  scoped_ptr<Segment::SegmentationRenderUnit> display_render;
  scoped_ptr<VideoFramework::VideoDisplayUnit> segment_display;
  scoped_ptr<Segment::SegmentationRenderUnit> render1;
  scoped_ptr<Segment::SegmentationRenderUnit> render2;
  scoped_ptr<Segment::SegmentationRenderUnit> render3;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer1;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer2;
  scoped_ptr<VideoFramework::VideoWriterUnit> writer3;
  
  // Should we display the segmentation?
  if (vm.count("display_level")) {
    float disp_level= vm["display_level"].as<float>();
    
    display_render.reset(new Segment::SegmentationRenderUnit(0.9,
                                                             disp_level,
                                                             true,   // highlight edges
                    
                                         false,  // no shape desc
                                                             true,   // concat with source
                                                             "VideoStream",
                                                             "SegmentationStream", 
                                                             "DisplaySegmentStream"));
    display_render->AttachTo(last_segment_unit);
    
    segment_display.reset(new VideoFramework::VideoDisplayUnit("DisplaySegmentStream"));
    segment_display->AttachTo(display_render.get());
  }
  
  if (vm.count("render_and_save")) {
    render1.reset(new Segment::SegmentationRenderUnit(0.85, .9, true, false, false,
                                                      "VideoStream",
                                                      "SegmentationStream", 
                                                      "ResultStream1"));
    render2.reset(new Segment::SegmentationRenderUnit(0.85, .0, true, false, false,
                                                      "VideoStream",
                                                      "SegmentationStream", 
                                                      "ResultStream2"));
    render3.reset(new Segment::SegmentationRenderUnit(0.85, .5, true, false, false,
                                                      "VideoStream",
                                                      "SegmentationStream",
                                                      "ResultStream3"));
    VideoFramework::VideoWriterOptions options;
    options.bit_rate = 16000000;
    options.fraction = 16;
    
    writer1.reset(new VideoFramework::VideoWriterUnit(
        (file_path / "mpeg_out1.mp4").string(), 
        "ResultStream1",
        options));
    writer2.reset(new VideoFramework::VideoWriterUnit(
        (file_path / "mpeg_out2.mp4").string(), 
        "ResultStream2",
        options));
    writer3.reset(new VideoFramework::VideoWriterUnit(
        (file_path / "mpeg_out3.mp4").string(),
        "ResultStream3",
        options));
    
    render1->AttachTo(last_segment_unit);
    render2->AttachTo(last_segment_unit);
    render3->AttachTo(last_segment_unit);
    
    writer1->AttachTo(render1.get());
    writer2->AttachTo(render2.get());
    writer3->AttachTo(render3.get());
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
  
  if(!reader->PrepareAndRun()) {
    std::cerr << "Video run not successful!\n";
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  po::options_description desc("Allowed options");
  Segment::SetupOptionsDescription(&desc);  
 
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);    
  
  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }
  
  std::string passed_file;
  if (vm.count("i")) {
    passed_file = vm["i"].as<string>();
    std::cout << "Segmenting input file " << passed_file << ".\n";
  } else {
    std::cout << "Please specify input file.\n Usage:\n" << desc;
    return 1;
  }
  
  Log<LogToStdErr>::ReportLevel() = INFO_V1;
  SegmentFromFile(passed_file, vm);
  std::cout << "Segmenation succesfull.\n";
  return 0;
}
