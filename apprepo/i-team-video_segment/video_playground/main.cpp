/*
 *  main.cpp
 *  make-test
 *
 *  Created by Matthias Grundmann on 9/26/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#include <iostream>
#include "image_filter.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"
#include "optical_flow_unit.h"
#include "conversion_units.h"
#include "sift_unit.h"
#include <string>

using namespace VideoFramework;

#include <cv.h>
#include <highgui.h>

#include "warping.h"
#include "assert_log.h"
#include "image_util.h"
#include "color_checker.h"

/*
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Params: VIDEO_FILE\n";
    return 1;
  }
  
  std::string file = std::string(argv[1]);
  std::cout << "Processing " << file << "\n";
  
  VideoReaderUnit reader(file);
  LuminanceUnit lum_unit;
  SiftUnit sift_unit(true);
  VideoDisplayUnit display("LuminanceStream");
  
  lum_unit.AttachTo(&reader);
  sift_unit.AttachTo(&lum_unit);
  display.AttachTo(&sift_unit);
  
  if(!reader.Run()) {
    std::cerr << "Video run not successful!\n";
  }
  
  return 0;
}
*/

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cout << "Params: VIDEO_FILE OUT_FILE\n";
    return 1;
  }
  
  std::string file = std::string(argv[1]);
  std::cout << "Processing " << file << "\n";
  
  // std::string weight_file = std::string(argv[2]);
  std::string out_file = std::string(argv[2]);
  
  VideoReaderUnit reader(file);
  vector<float> twist;
  twist.push_back(1.2);
  twist.push_back(0.8);
  twist.push_back(1.1);
  vector<float> offset;
  offset.push_back(15);
  offset.push_back(-30);
  offset.push_back(0);
  
  ColorTwist color_twist(twist, offset);
  VideoDisplayUnit display("VideoStream");
  VideoWriterUnit writer(out_file, "VideoStream");

  VideoWriterOptions options;
  options.bit_rate = 20000000;
  options.fraction = 2;
  writer.PassOptions(options);
  
  color_twist.AttachTo(&reader);
  display.AttachTo(&color_twist);
  writer.AttachTo(&display);
  
  if(!reader.Run()) {
    std::cerr << "Video run not successful!\n";
  }
  
  std::cout << "Peter, lach mal.\n";
  return 0;
}

/*
int main(int argc, char** argv) {
  if (argc != 6) {
    std::cout << "Params: VIDEO_FILE  CHECKER_FILE  OUT_FILE MIN_TRESH MAX_TRESH\n";
    return 1;
  }
    
  std::string file = std::string(argv[1]);
  std::cout << "Processing " << file << "\n";

  std::string checker_file = std::string(argv[2]);
  std::string out_file = std::string(argv[3]);
  
  int min_tresh = atoi(argv[4]);
  int max_thresh = atoi(argv[5]);
  
  VideoReaderUnit reader(file);
  LuminanceUnit lum_unit;
  
  ColorCheckerUnit checker_unit(checker_file, out_file, min_tresh, max_thresh);
  VideoDisplayUnit display("VideoStream");
  VideoWriterUnit writer("output.mov", "VideoStream");
  VideoWriterOptions options;
  options.bit_rate = 20000000;
  options.scale = 0.5;
  writer.PassOptions(options);
  
  lum_unit.AttachTo(&reader);
  checker_unit.AttachTo(&lum_unit);
  display.AttachTo(&reader);
  writer.AttachTo(&display);
  
  if(!reader.Run()) {
    std::cerr << "Video run not successful!\n";
  }
   
  std::cout << "Peter, lach mal.\n";
  return 0;
}
 */
