/*
 *  segmentation_io.cpp
 *  segmentat_util
 *
 *  Created by Matthias Grundmann on 6/30/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include "assert_log.h"
#include "segmentation_io.h"
#include "segmentation.pb.h"

#include <iostream>

typedef unsigned char uchar;

namespace Segment {

bool SegmentationWriter::OpenFile() {
  // Open file to write
  LOG(INFO_V1) << "Writing segmentation to file " << filename_;
  ofs_.open(filename_.c_str(),
            std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);

  if (!ofs_) {
    LOG(ERROR) << "Could not open " << filename_ << " to write!\n";
    return false;
  }

  num_chunks_ = 0;
  curr_offset_ = 0;
  return true;
}

void SegmentationWriter::AddSegmentationToChunk(const SegmentationDesc& desc,
                                                int64_t pts) {
  // Buffer for later.
  file_offsets_.push_back(curr_offset_);
  chunk_buffer_.push_back(string());
  desc.SerializeToString(&chunk_buffer_.back());

  // Increment by size of a SEG_FRAME.
  curr_offset_ += chunk_buffer_.back().size() + 4 + sizeof(int32_t);
  time_stamps_.push_back(pts);
}

void SegmentationWriter::AddSegmentationDataToChunk(const char* data,
                                                    int size,
                                                    int64_t pts) {
  // Buffer for later.
  file_offsets_.push_back(curr_offset_);
  chunk_buffer_.push_back(string());
  chunk_buffer_.back().assign(data, size);

  curr_offset_ += size + 4 + sizeof(int32_t);
  time_stamps_.push_back(pts);
}

namespace {
  template <class T> const char* ToConstCharPtr(const T* t) {
    return reinterpret_cast<const char*>(t);
  }
  template <class T> char* ToCharPtr(T* t) {
    return reinterpret_cast<char*>(t);
  }
}

void SegmentationWriter::WriteChunk() {
  // Compile Header information.
  int32_t num_frames = file_offsets_.size();
  int32_t chunk_id = num_chunks_++;

  ofs_.write("CHNK", 4);
  ofs_.write(ToConstCharPtr(&chunk_id), sizeof(chunk_id));
  ofs_.write(ToConstCharPtr(&num_frames), sizeof(num_frames));

  int64_t size_of_header =
    4 +
    2 * sizeof(int32_t) +
    num_frames * 2 * sizeof(int64_t) +
    sizeof(int64_t);

  // Advance offsets by size of header.
  curr_offset_ += size_of_header;
  for (size_t i = 0; i < file_offsets_.size(); ++i) {
    file_offsets_[i] += size_of_header;
  }

  // Write offsets and pts.
  for (size_t i = 0; i < file_offsets_.size(); ++i) {
    ofs_.write(ToConstCharPtr(&file_offsets_[i]), sizeof(file_offsets_[i]));
  }

  ASSURE_LOG(file_offsets_.size() == time_stamps_.size());
  for (size_t i = 0; i < time_stamps_.size(); ++i) {
    ofs_.write(ToConstCharPtr(&time_stamps_[i]), sizeof(time_stamps_[i]));
  }

  // Write offset of next header.
  ofs_.write(ToConstCharPtr(&curr_offset_), sizeof(curr_offset_));

  // Write frames.
  ASSURE_LOG(file_offsets_.size() == chunk_buffer_.size());
  for (size_t i = 0; i < chunk_buffer_.size(); ++i) {
    ofs_.write("SEGD", 4);
    int32_t frame_size = chunk_buffer_[i].length();
    ofs_.write(ToConstCharPtr(&frame_size), sizeof(frame_size));
    ofs_.write(ToConstCharPtr(&chunk_buffer_[i][0]), frame_size);
  }

  // Clear chunk information.
  chunk_buffer_.clear();
  file_offsets_.clear();
  time_stamps_.clear();
}

void SegmentationWriter::WriteTermHeaderAndClose() {
   if (!chunk_buffer_.empty()) {
     WriteChunk();
   }

   ofs_.write("TERM", 4);
   ofs_.write(ToConstCharPtr(&num_chunks_), sizeof(num_chunks_));
   ofs_.close();
}

void SegmentationWriter::FlushAndReopen(const string& filename) {
  if (!chunk_buffer_.empty()) {
    WriteChunk();
  }

  WriteTermHeaderAndClose();
  filename_ = filename;
  curr_offset_ = 0;
  num_chunks_ = 0;
  OpenFile();
}

bool SegmentationReader::OpenFileAndReadHeaders() {
  // Open file.
  LOG(INFO_V1) << "Reading segmentation from file " << filename_;
  ifs_.open(filename_.c_str(), std::ios_base::in | std::ios_base::binary);

  if (!ifs_) {
    LOG(ERROR) << "Could not open segmentation file " << filename_ << "\n";
    return false;
  }

  // Read file offsets until TERM header.
  char header_type[5] = {0, 0, 0, 0, 0};
  int prev_header_id = -1;
  while (true) {
    ifs_.read(header_type, 4);

    // End of file, return.
    if (strcmp(header_type, "TERM") == 0) {
      break;
    }

    // We only process chunk headers while over skipping seg frames.
    if (strcmp(header_type, "CHNK") != 0) {
      LOG(ERROR) << "Parsing error, expected chunk header at current offset."
                 << " Found: " << header_type;
      return false;
    }

    int32_t header_id;
    ifs_.read(ToCharPtr(&header_id), sizeof(header_id));
    ASSURE_LOG(prev_header_id + 1 == header_id)
        << prev_header_id << " " << header_id;

    prev_header_id = header_id;

    int32_t num_frames_in_chunk;
    ifs_.read(ToCharPtr(&num_frames_in_chunk), sizeof(num_frames_in_chunk));

    // Read offsets.
    for (int f = 0; f < num_frames_in_chunk; ++f) {
      int64_t offset;
      ifs_.read(ToCharPtr(&offset), sizeof(offset));
      file_offsets_.push_back(offset);
    }

    // Read timestamps.
    for (int f = 0; f < num_frames_in_chunk; ++f) {
      int64_t timestamp;
      ifs_.read(ToCharPtr(&timestamp), sizeof(timestamp));
      time_stamps_.push_back(timestamp);
    }

    int64_t next_header_pos;
    ifs_.read(ToCharPtr(&next_header_pos), sizeof(next_header_pos));
    ifs_.seekg(next_header_pos);
  }

  return true;
}

void SegmentationReader::SegmentationResolution(int* width, int* height) {
  ASSURE_LOG(width);
  ASSURE_LOG(height);

  const int curr_playhead = curr_frame_;
  SeekToFrame(0);
  int frame_sz = ReadNextFrameSize();
  vector<unsigned char> buffer(frame_sz);
  ReadNextFrame(&buffer[0]);

  SegmentationDesc segmentation;
  segmentation.ParseFromArray(&buffer[0], buffer.size());

  *width = segmentation.frame_width();
  *height = segmentation.frame_height();

  if (curr_playhead < NumFrames()) {
    SeekToFrame(curr_playhead);
  }
}

void SegmentationReader::SeekToFrame(int frame) {
  ASSURE_LOG(frame < file_offsets_.size()) << "Requested frame out of bound.";
  curr_frame_ = frame;
}

int SegmentationReader::ReadNextFrameSize() {
  // Seek to next frame (to skip chunk headers).
  ifs_.seekg(file_offsets_[curr_frame_]);
  char header_type[5] = {0, 0, 0, 0, 0};
  ifs_.read(header_type, 4);
  if (!strcmp(header_type, "SEGD") == 0) {
    LOG(ERROR) << "Expecting segmentation header. Error parsing file.";
    return -1;
  }

  ifs_.read(ToCharPtr(&frame_sz_), sizeof(frame_sz_));
  return frame_sz_;
}

void SegmentationReader::ReadNextFrame(uchar* data) {
  ifs_.read(ToCharPtr(data), frame_sz_);
  ++curr_frame_;
}

namespace {
  template <class T> void write(std::ostringstream& stream, const T& t) {
    stream.write((const char*)&t, sizeof(t));
  }
}

void StripToEssentials(const SegmentationDesc& desc, string* binary_rep) {
  std::ostringstream seg_data;

  int frame_width = desc.frame_width();
  int frame_height = desc.frame_width();
  write(seg_data, frame_width);
  write(seg_data, frame_height);

  // Number of regions.
  int num_regions = desc.region_size();
  write(seg_data, num_regions);

  // Save regions.
  for(int i = 0; i < num_regions; ++i) {
    const SegmentationDesc::Region2D& r = desc.region(i);

    // Id.
    int id = r.id();
    write(seg_data, id);

    // Scanlines.
    int num_scan_inter = r.raster().scan_inter_size();
    write(seg_data, num_scan_inter);

    for (int j = 0; j < num_scan_inter; ++j) {
      int y = r.raster().scan_inter(j).y();
      int left = r.raster().scan_inter(j).left_x();
      int right = r.raster().scan_inter(j).right_x();

      write(seg_data, y);
      write(seg_data, left);
      write(seg_data, right);
    }

    // ShapeMoments.
    int shape_sz = r.shape_moments().size();
    write(seg_data, shape_sz);
    int mean_x = r.shape_moments().mean_x();
    write(seg_data, mean_x);
    int mean_y = r.shape_moments().mean_y();
    write(seg_data, mean_y);
    int moment_xx = r.shape_moments().moment_xx();
    write(seg_data, moment_xx);
    int moment_xy = r.shape_moments().moment_xy();
    write(seg_data, moment_xy);
    int moment_yy = r.shape_moments().moment_yy();
    write(seg_data, moment_yy);
  }

  // Save hierarchies.
  int hierarchy_size = desc.hierarchy_size();
  write(seg_data, hierarchy_size);

  for (int i = 0; i < hierarchy_size; ++i) {
    int num_regions = desc.hierarchy(i).region_size();
    write(seg_data, num_regions);

    for (int j = 0; j < num_regions; ++j) {
      const SegmentationDesc::CompoundRegion& r = desc.hierarchy(i).region(j);

      // Id.
      int id = r.id();
      write(seg_data, id);

      // Size.
      int size = r.size();
      write(seg_data, size);

      // Parent id.
      int parent_id = r.parent_id();
      write(seg_data, parent_id);

      // Children.
      int num_children = r.child_id_size();
      write(seg_data, num_children);

      for (int c = 0; c < num_children; ++c) {
        int child_id = r.child_id(c);
        write(seg_data, child_id);
      }

      // Start and end frame.
      int start_frame = r.start_frame();
      write(seg_data, start_frame);
      int end_frame = r.end_frame();
      write(seg_data, end_frame);
    }
  }

  seg_data.flush();
  *binary_rep = string(seg_data.str());
}

}  // namespace Segment.
