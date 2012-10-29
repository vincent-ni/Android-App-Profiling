/*
 *  joint_seg_tree.cpp
 *  joint_seg_tree_sample
 *
 *  Created by Matthias Grundmann on 9/20/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "assert_log.h"
#include "conversion_units.h"
#include "flow_reader.h"
#include "image_filter.h"
#include "image_util.h"
#include "joint_segmentation_unit.h"
#include "segmentation_unit.h"
#include "sift_unit.h"
#include "video_capture_unit.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"
#include "video_pool.h"
#include "warping.h"

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

using namespace ImageFilter;
namespace po = boost::program_options;

int main(int argc, char** argv) {
  Log<LogToStdErr>::ReportLevel() = INFO_V1;
  
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce this help message")
    ("i", po::value<string>(), "first input video file")
    ("j", po::value<string>(), "second input video file")
    ("save_render_to", po::value<string>(),
     "renders result and saves it to specifed output file")
    ("write_to_file", po::value<string>(),
     "Writes joint video stream and the segmentation to file. Only specify prefix.")
  ;
  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);    
  
  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  vector<std::string> input_files;
  if (vm.count("i")) {
    input_files.push_back(vm["i"].as<string>());
  } else {
    LOG(FATAL) << "Please specify first input file.\n Usage:\n" << desc;
  }

  if (vm.count("j")) {
    input_files.push_back(vm["j"].as<string>());
  } else {
    LOG(FATAL) << "Please specify first input file.\n Usage:\n" << desc;
  }
  
  vector<std::string> file_bases;
  for (vector<std::string>::const_iterator file = input_files.begin(); 
       file != input_files.end();
       ++file) {
    LOG(INFO) << "Using " << *file << " as input.";
    file_bases.push_back(file->substr(0, file->find_last_of(".")));
  }
  
  // Setup input units.
  const int num_inputs = input_files.size();
  vector<shared_ptr<VideoFramework::VideoReaderUnit> > video_readers(num_inputs);
  vector<shared_ptr<VideoFramework::SiftReader> > sift_readers(num_inputs);
  vector<shared_ptr<Segment::SegmentationReaderUnit> > seg_readers(num_inputs);

  for (int i = 0; i < num_inputs; ++i) {
    video_readers[i].reset(new VideoFramework::VideoReaderUnit(input_files[i]));
    sift_readers[i].reset(new VideoFramework::SiftReader(file_bases[i] + ".sift"));
    sift_readers[i]->AttachTo(video_readers[i].get());
    
    seg_readers[i].reset(new Segment::SegmentationReaderUnit(
        file_bases[i] + "_segment.pb"));
    
    seg_readers[i]->AttachTo(sift_readers[i].get());
  }
  
  // Bundle all reader unit into one source.
  scoped_ptr<VideoFramework::MultiSource> multi_source(
      new VideoFramework::MultiSource(VideoFramework::MultiSource::DRAIN_SOURCE));
  
  for (int i = 0; i < num_inputs; ++i) {
    multi_source->AddSource(video_readers[i].get());
  }
  
  // Setup pool as sink.
  scoped_ptr<Segment::JointSegmentationUnit> joint_seg_unit(
      new Segment::JointSegmentationUnit());
  
  for (int i = 0; i < num_inputs; ++i) {
    joint_seg_unit->AttachTo(seg_readers[i].get());
  }
  
  // Attach render to pool.
  scoped_ptr<Segment::SegmentationRenderUnit> display_render;
  display_render.reset(new Segment::SegmentationRenderUnit(0.85,
                                                           0.5,
                                                           true,
                                                           false,
                                                           false,
                                                           "JointVideoStream",
                                                           "JointSegmentStream", 
                                                           "DisplaySegmentStream"));
  display_render->AttachTo(joint_seg_unit.get());
  
  // Attach Display to render.
  scoped_ptr<VideoFramework::VideoDisplayUnit> video_display(
    new VideoFramework::VideoDisplayUnit("DisplaySegmentStream"));
  video_display->AttachTo(display_render.get());
  
  // Save result to file if desired.
  scoped_ptr<VideoFramework::VideoWriterUnit> render_writer;
  VideoFramework::VideoWriterOptions writer_options;
  writer_options.bit_rate = 16000000;
  writer_options.fraction = 4;
  
  if (vm.count("save_render_to")) {
    render_writer.reset(new VideoFramework::VideoWriterUnit(
        vm["save_render_to"].as<string>(), "DisplaySegmentStream"));
    render_writer->PassOptions(writer_options);
    
    render_writer->AttachTo(video_display.get());                       
  }
  
  scoped_ptr<VideoFramework::VideoWriterUnit> joint_video_writer;
  scoped_ptr<Segment::SegmentationWriterUnit> seg_writer;
  
  if (vm.count("write_to_file")) {
    string file_prefix = vm["write_to_file"].as<string>();
    joint_video_writer.reset(new VideoFramework::VideoWriterUnit(file_prefix + ".mov",
                                                                 "JointVideoStream"));
    joint_video_writer->PassOptions(writer_options);
    joint_video_writer->AttachTo(joint_seg_unit.get());
    
    seg_writer.reset(new Segment::SegmentationWriterUnit(file_prefix + "_segment.pb",
                                                         "JointVideoStream",
                                                         "JointSegmentStream"));
    seg_writer->AttachTo(joint_seg_unit.get());
  }

  if(!multi_source->Run()) {
    std::cerr << "Video run not successful!\n";
  }
   
  std::cout << "Peter, lach mal.\n";
  return 0;
}
