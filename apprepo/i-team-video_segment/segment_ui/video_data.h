
#ifndef VIDEODATA_H
#define VIDEODATA_H

#include <boost/scoped_array.hpp>
using boost::scoped_array;
#include <boost/utility.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <list>

#include "image_util.h"
using namespace ImageFilter;

#include "video_unit.h";

class AddFrameUnit;

// Fixed 32bit per pixel frame. Holds data for all video frames.
class VideoData {
public:
	VideoData() : height_(0), width_(0), fps_(0), frames_(0) {};
	~VideoData();

	bool LoadFromFile (std::string filename);

	int GetWidth() const { return width_; }
	int GetHeight() const { return height_; }
	int GetFrames() const { return frames_; }
	float GetFPS() const { return fps_; }
  void TrimTo(int num_frames);

	const std::vector<shared_ptr<IplImage> >& GetData() const { return data_; }
  const IplImage* GetFrameAt(int frame) const { return data_[frame].get(); }

protected:
	int height_;
	int	width_;
	float	fps_;
	int frames_;

	std::vector<shared_ptr<IplImage> > data_;
  friend class AddFrameUnit;
};

// Simple wrapper to initialize VideoData from VideoReader
class AddFrameUnit : public VideoFramework::VideoUnit {
public:
  AddFrameUnit(VideoData* video_data) : video_data_(video_data) { }
  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

protected:
  VideoData* video_data_;
  int video_stream_idx_;
  int frame_width_;
  int frame_height_;
  int frame_num_;
};


#endif
