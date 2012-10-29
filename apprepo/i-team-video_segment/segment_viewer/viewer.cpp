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

#include "assert_log.h"
#include "segmentation_io.h"
#include "segmentation_util.h"
#include "segmentation_render.h"

using namespace Segment;

// Slider positions.
int g_hierarchy_level;
int g_frame_pos;

// Frame width and height.
int g_frame_num_;
int g_frame_width;
int g_frame_height;

// This should be scoped_ptr's. Removed to remove boost dependency.
SegmentationReader* g_segment_reader;
SegmentationDesc* g_seg_hierarchy;
int g_hierarchy_pos;

// Render target.
IplImage* g_frame_buffer = 0;

// Indicates if automatic playing is set.
bool g_playing;

// The name used to identify the GTK window
std::string window_name = "main_window";


// Returns number of hierarchy_levels.
void RenderCurrentFrame(int frame_number) {
  g_segment_reader->SeekToFrame(frame_number);

  // Read from file.
  vector<unsigned char> data_buffer(g_segment_reader->ReadNextFrameSize());
  g_segment_reader->ReadNextFrame(&data_buffer[0]);

  SegmentationDesc segmentation;
  segmentation.ParseFromArray(&data_buffer[0], data_buffer.size());

  // Update hierarchy is necessary.
  if (g_hierarchy_pos != segmentation.hierarchy_frame_idx()) {
    g_hierarchy_pos = segmentation.hierarchy_frame_idx();
    g_segment_reader->SeekToFrame(g_hierarchy_pos);

   // Read from file.
    vector<unsigned char> data_buffer(g_segment_reader->ReadNextFrameSize());
    g_segment_reader->ReadNextFrame(&data_buffer[0]);
    g_seg_hierarchy->ParseFromArray(&data_buffer[0], data_buffer.size());
  }

  // Allocate frame_buffer if necessary
  if (g_frame_buffer == NULL) {
    g_frame_buffer = cvCreateImage(cvSize(g_frame_width,
                                          g_frame_height), IPL_DEPTH_8U, 3);
  }

  // Render segmentation at specified level.
  RenderRegionsRandomColor(g_frame_buffer->imageData,
                           g_frame_buffer->widthStep,
                           g_frame_buffer->width,
                           g_frame_buffer->height,
                           g_frame_buffer->nChannels,
                           g_hierarchy_level,
                           true,
                           false,
                           segmentation,
                           &g_seg_hierarchy->hierarchy());
}

void FramePosChanged(int pos) {
  g_frame_pos = pos;
  RenderCurrentFrame(g_frame_pos);
  cvShowImage(window_name.c_str(), g_frame_buffer);
}

void HierarchyLevelChanged(int level) {
  g_hierarchy_level = level;
  RenderCurrentFrame(g_frame_pos);
  cvShowImage(window_name.c_str(), g_frame_buffer);
}

int main(int argc, char** argv) {
  // Get filename from command prompt.
  if (argc != 2 && argc != 3) {
    std::cout << "Usage: segment_viewer FILE_NAME [\"WINDOW NAME\"]\n";
    return 1;
  }

  std::string filename(argv[1]);
  g_playing = false;

  // if window name provided
  if (argc == 3) {
    window_name = argv[2];
  }
  else {
    // if not set as the filename
    size_t temp = filename.find_last_of("/\\");
    window_name = filename.substr(temp+1);
  }

  // Read segmentation file.
  g_segment_reader = new SegmentationReader(filename);
  g_segment_reader->OpenFileAndReadHeaders();
  g_frame_pos = 0;

  std::cout << "Segmentation file " << filename << " contains "
            << g_segment_reader->NumFrames() << " frames.\n";

  // Read first frame, it contains the hierarchy.
  vector<unsigned char> data_buffer(g_segment_reader->ReadNextFrameSize());
  g_segment_reader->ReadNextFrame(&data_buffer[0]);

  // Save hierarchy for first chunk.
  g_seg_hierarchy = new SegmentationDesc;
  g_seg_hierarchy->ParseFromArray(&data_buffer[0], data_buffer.size());
  g_hierarchy_pos = 0;
  g_frame_num_ = g_segment_reader->NumFrames();
  g_frame_width = g_seg_hierarchy->frame_width();
  g_frame_height = g_seg_hierarchy->frame_height();

  std::cout << "Video resolution: "
            << g_frame_width << "x" << g_frame_height << "\n";

  // Create OpenCV window.
  cvNamedWindow(window_name.c_str(), CV_WINDOW_NORMAL);

  cvCreateTrackbar("frame_pos",
                   window_name.c_str(),
                   &g_frame_pos,
                   g_segment_reader->NumFrames() - 1,
                   &FramePosChanged);

  cvCreateTrackbar("hier_level",
                   window_name.c_str(),
                   &g_hierarchy_level,
                   g_seg_hierarchy->hierarchy_size() - 1,
                   &HierarchyLevelChanged);

  RenderCurrentFrame(0);
  cvShowImage(window_name.c_str(), g_frame_buffer);

  int key_value = 0;

  // Yotam Doron recommended this kind of loop.
  while (1) {
    key_value = cvWaitKey(30) & 0xFF;
    if (key_value == 27) {
      break;
    }

    if (g_playing) {
      FramePosChanged((g_frame_pos + 1) % g_frame_num_);
    }

    switch (key_value) {
      case 110:
        FramePosChanged((g_frame_pos + 1) % g_frame_num_);
        break;
      case 112:
        FramePosChanged((g_frame_pos - 1 + g_frame_num_) % g_frame_num_);
        break;
      case 32:
        g_playing = !g_playing;
      default:
        break;
    }
  }

  g_segment_reader->CloseFile();
  delete g_segment_reader;

  cvReleaseImage(&g_frame_buffer);
  delete g_seg_hierarchy;

   return 0;
}
