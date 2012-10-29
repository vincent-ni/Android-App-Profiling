/*
 *  converter.cpp
 *  segment_converter
 *
 *  Created by Matthias Grundmann on 8/13/2011.
 *  Copyright 2011 Matthias Grundmann. All rights reserved.
 *
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "assert_log.h"
#include "segmentation_io.h"
#include "segmentation_render.h"
#include "segmentation_util.h"

using namespace Segment;

int main(int argc, char** argv) {
  // Get filename from command prompt.
  if (argc != 3) {
    std::cout << "Usage: segment_converter FILE_NAME OPTION\n"
              << "[--text_format] : Outputs one protobuffer per frame as text. \n"
              << "[--binary_format] : Outputs one protobuffer per frame as binary. \n"
              << "[--bitmap_ids=<level>] : Outputs one image per frame as "
              << "3 channel PNG.\n\t Pixel represents region ID as single 24bit int "
              << "(little endian!). \n"
              << "[--bitmap_color=<level>] :  Outputs one image per frame as "
              << "3 channel PNG.\n\t Pixel represents per-region random color as 3 "
              << "channel uint.\n\t Region boundary highlighted in black. \n"
              << "[--strip=<destination>]: Saves proto buffer in binary format, stripping it to essentials.\n";
    return 1;
  }

  enum ConvMode { CONV_TEXT, CONV_BINARY, CONV_BITMAP_ID, CONV_BITMAP_COLOR, STRIP };
  ConvMode mode;
  int hier_level = 0;
  string dest_filename;

  if (strcmp(argv[2], "--text_format") == 0) {
    mode = CONV_TEXT;
    std::cout << "Converting to text.";
  } else if (strcmp(argv[2], "--binary_format") == 0) {
    mode = CONV_BINARY;
    std::cout << "Converting to binary format.";
  } else if (std::string(argv[2]).find("--bitmap_ids") == 0) {
    mode = CONV_BITMAP_ID;
    if (std::string(argv[2]).size() <= 13) {
      std::cout << "Missing hierarchy level for bitmap_ids.\n";
      return 1;
    }
    hier_level = std::atoi(std::string(argv[2]).substr(13).c_str());
    std::cout << "Converting to id bitmaps for hierarchy level: " << hier_level << "\n";
  } else if (std::string(argv[2]).find("--bitmap_color") == 0) {
    mode = CONV_BITMAP_COLOR;
    if (std::string(argv[2]).size() <= 15) {
      std::cout << "Missing hierarchy level for bitmap_color.\n";
      return 1;
    }

    hier_level = std::atoi(std::string(argv[2]).substr(15).c_str());
    std::cout << "Converting to color bitmaps for hierarchy level: "
              << hier_level << "\n";
  } else if (std::string(argv[2]).find("--strip") == 0) {
    mode = STRIP;
    if (std::string(argv[2]).size() <= 8) {
      std::cout << "Missing destination file for stripping.\n";
      return 1;
    }

    dest_filename = std::string(argv[2]).substr(8);
  } else {
    std::cout << "Unknown mode specified.\n";
    return 1;
  }

  std::string filename(argv[1]);

  // Read segmentation file.
  SegmentationReader segment_reader(filename);
  segment_reader.OpenFileAndReadHeaders();

  std::cout << "Segmentation file " << filename << " contains "
            << segment_reader.NumFrames() << " frames.\n";

  SegmentationWriter* writer = NULL;
  if (mode == STRIP) {
    writer = new SegmentationWriter(dest_filename);
    if (!writer->OpenFile()) {
      std::cout << "Could not open destination file.\n";
      delete writer;
      return 1;
    }
  }

  Hierarchy hierarchy;
  const int chunk_size = 100;    // By default use chunks of 100 frames.

  for (int f = 0; f < segment_reader.NumFrames(); ++f) {
    segment_reader.SeekToFrame(f);

    // Read from file.
    vector<unsigned char> data_buffer(segment_reader.ReadNextFrameSize());
    segment_reader.ReadNextFrame(&data_buffer[0]);

    SegmentationDesc segmentation;
    segmentation.ParseFromArray(&data_buffer[0], data_buffer.size());

    if (segmentation.hierarchy_size() > 0) {
      hierarchy.Clear();
      hierarchy.MergeFrom(segmentation.hierarchy());
    }

    char curr_file[50];
    if (mode == CONV_TEXT || mode == CONV_BINARY) {
      sprintf(curr_file, "frame%04d.pb", f);
    } else {
      sprintf(curr_file, "frame%04d.png", f);
    }

    if (f % 5 == 0) {
      std::cout << "Writing frame " << f << " of "
                << segment_reader.NumFrames() << "\n";
    }

    int frame_width = segmentation.frame_width();
    int frame_height = segmentation.frame_height();

    if (mode == CONV_BINARY) {
      std::ofstream ofs(curr_file, std::ios_base::out | std::ios_base::binary);
      segmentation.SerializeToOstream(&ofs);
    } else if (mode == CONV_TEXT) {
      std::ofstream ofs(curr_file, std::ios_base::out);
      ofs << segmentation.DebugString();
    } else if (mode == CONV_BITMAP_ID) {
      cv::Mat id_image(frame_height, frame_width, CV_8UC4);
      SegmentationDescToIdImage((int*)id_image.data,
                                id_image.step[0],
                                frame_width,
                                frame_height,
                                hier_level,
                                segmentation,
                                &hierarchy);
      // Discard most significant 8bit (little endian).
      vector<cv::Mat> id_channels;
      cv::split(id_image, id_channels);
      cv::Mat frame_buffer(frame_height, frame_width, CV_8UC3);
      cv::merge(&id_channels[0], 3, frame_buffer);
      cv::imwrite(curr_file, frame_buffer);
    } else if (mode == CONV_BITMAP_COLOR) {
      cv::Mat frame_buffer(frame_height, frame_width, CV_8UC3);
      RenderRegionsRandomColor((char*)frame_buffer.data,
                               frame_buffer.step[0],
                               frame_width,
                               frame_height,
                               3,
                               hier_level,
                               true,
                               false,
                               segmentation,
                               &hierarchy);
      cv::imwrite(curr_file, frame_buffer);
    } else if (mode == STRIP) {
      string stripped_data;
      StripToEssentials(segmentation, &stripped_data);
      writer->AddSegmentationDataToChunk(&stripped_data[0],
                                         stripped_data.size(),
                                         f);
      if (f > 0 && f % chunk_size == 0) {
        writer->WriteChunk();
      }
    }
  }

  if (mode == STRIP) {
    writer->WriteTermHeaderAndClose();
    delete writer;
  }

  segment_reader.CloseFile();
  return 0;
}
