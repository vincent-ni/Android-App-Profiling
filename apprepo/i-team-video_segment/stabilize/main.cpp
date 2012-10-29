/*
 *  main.cpp
 *  stabilize
 *
 *  Created by Matthias Grundmann on 10/29/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include <iostream>
#include "image_filter.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"
#include "optical_flow_unit.h"
#include "conversion_units.h"
#include <string>

using namespace VideoFramework;

#include <cv.h>
#include <highgui.h>

#include "assert_log.h"
#include "image_util.h"
#include "stabilize.h"


int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Params: VIDEO_FILE\n";
    return 1;
  }
  
  std::string file = std::string(argv[1]);
  std::cout << "Processing " << file << "\n";
  
  VideoReaderUnit reader(file);
  LuminanceUnit lum_unit;
  OpticalFlowUnit flow_unit(0.05, 1); // 0.1 & 1
  StabilizeUnit stabilize;
  VideoDisplayUnit display("VideoStream");
  
  lum_unit.AttachTo(&reader);
  flow_unit.AttachTo(&lum_unit);
  stabilize.AttachTo(&flow_unit);
  display.AttachTo(&stabilize);
  
  VideoWriterUnit writer("stabilized_output.mov", "VideoStream");
  
  VideoWriterOptions options;
  options.bit_rate = 20000000;
  options.fraction = 2;
  writer.PassOptions(options);
  
  writer.AttachTo(&display);
  
  if(!reader.PrepareAndRun()) {
    std::cerr << "Video run not successful!\n";
  }
  
  return 0;
}