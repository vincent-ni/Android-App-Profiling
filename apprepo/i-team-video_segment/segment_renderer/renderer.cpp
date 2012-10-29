/*
 *  main.cpp
 *  segmentation_sample
 *
 *  Created by Matthias Grundmann on 6/17/2010.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include <iostream>
#include <string>
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <boost/lexical_cast.hpp>

#include "assert_log.h"
#include "segmentation_io.h"
#include "segmentation_util.h"
#include "segmentation_unit.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"

using namespace Segment;
using namespace VideoFramework;

int main(int argc, char** argv) {
  // Get filename from command prompt.
  if (argc < 4) {
    std::cout << "Usage: segment_renderer INPUT_FILE_NAME RENDER_LEVEL"
              << "OUTPUT_VIDEO [MIN_FRAME_DIM]\n";
    return 1;
  }

  SegmentationReaderUnit seg_reader(argv[1]);
  SegmentationRenderUnit seg_render(1,
                                    boost::lexical_cast<float>(argv[2]),
                                    true,
                                    false,
                                    false,
                                    "",
                                    "SegmentationStream",
                                    "VideoStream");
  seg_render.AttachTo(&seg_reader);

  VideoWriterOptions writer_options;
  writer_options.bit_rate = 1500000;
  if (argc > 4) {
    int min_dim = boost::lexical_cast<int>(argv[4]);
    writer_options.scale_min_dim = min_dim;
  }

  VideoWriterUnit writer_unit(argv[3], "VideoStream", writer_options);
  writer_unit.AttachTo(&seg_render);

  if (!seg_reader.PrepareAndRun()) {
    std::cerr << "Error during processing.";
  }

  return 0;
}
