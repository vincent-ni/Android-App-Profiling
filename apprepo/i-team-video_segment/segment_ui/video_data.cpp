
#include "video_data.h"

#include "assert_log.h"
#include "video_reader_unit.h"

using namespace VideoFramework;

VideoData::~VideoData() {
}

bool VideoData::LoadFromFile(std::string filename) {
  VideoReaderUnit reader(filename,
                         0,
                         "VideoStream",
                         PIXEL_FORMAT_BGRA32);

  AddFrameUnit add_frames(this);
  add_frames.AttachTo(&reader);

  // Read file.
  if (!reader.PrepareAndRun()) {
    LOG(ERROR) << "Can't open file.";
    return false;
  }

  int approx_size = width_ * height_ *sizeof(int) * frames_ / 1024 / 1024;
  LOG(INFO) << "Done reading video file ... Video data actually consumes "
            << approx_size << " MB of memory\n";
  return true;
}

void VideoData::TrimTo(int num_frames) {
  if(num_frames >= frames_) {
    LOG(WARNING) << "Not enough frames.";
    return;
  }

  frames_ = num_frames;
  data_.resize(frames_);
}

bool AddFrameUnit::OpenStreams(StreamSet* set) {
  // Find video stream idx.
  video_stream_idx_ = FindStreamIdx("VideoStream", set);
  if (video_stream_idx_ < 0) {
    LOG(ERROR) << "Could not find Video stream!\n";
    return false;
  }

  const VideoStream* vid_stream =
      dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

  ASSERT_LOG(vid_stream);

  frame_width_ = vid_stream->frame_width();
  frame_height_ = vid_stream->frame_height();

  // Set VideoData members.
  video_data_->height_ = frame_height_;
  video_data_->width_ = frame_width_;
  video_data_->fps_ = vid_stream->fps();
  frame_num_ = 0;

  return true;
}

void AddFrameUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  static int count = 0;
  ++count;

  const VideoFrame* frame =
      dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
  ASSERT_LOG(frame);

  IplImage image;
  frame->ImageView(&image);

  shared_ptr<IplImage> new_frame = cvCreateImageShared(frame_width_,
                                                       frame_height_,
                                                       IPL_DEPTH_8U,
                                                       4);
  cvCopy(&image, new_frame.get());
  video_data_->data_.push_back(new_frame);
  ++frame_num_;

  output->push_back(input);
}

void AddFrameUnit::PostProcess(list<FrameSetPtr>* append) {
  video_data_->frames_ = frame_num_;
}

