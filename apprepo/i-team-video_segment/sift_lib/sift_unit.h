/*
 *  sift_unit.h
 *  sift-lib
 *
 *  Created by Matthias Grundmann on 6/7/2010.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef SIFT_UNIT_H__
#define SIFT_UNIT_H__

#include "common.h"
#include "video_unit.h"

#include <boost/serialization/vector.hpp>

class SiftGPU;

namespace VideoFramework {

  class SiftFrame : public DataFrame {
  public:
    struct SiftFeature {
      float x;
      float y;
      float scale;
      float orientation;
      float descriptor[128];

      // Returns L2 difference of descriptor w.r.t. to other feature feat.
      float L2Diff(const SiftFeature& feat) const;

    private:
      // Serialization stuff.
      friend class boost::serialization::access;
      template <class Archive>
      void serialize(Archive& ar, const unsigned int version) {
        ar & x & y & scale & orientation;
        for (int i = 0; i < 128; ++i) {
          ar & descriptor[i];
        }
      }
    };

    int NumFeatures() const { return features_.size(); }
    void CopyFeatures(vector<SiftFeature>& sift_features) {
      features_ = sift_features;
    }

    void SwapFeatures(vector<SiftFeature>* sift_features) {
      features_.swap(*sift_features);
    }

    const vector<SiftFeature>& Features() const { return features_; }

  private:
    // Serialization stuff.
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar & boost::serialization::base_object<DataFrame>(*this);
      ar & features_;
    }

  protected:
    vector<SiftFeature> features_;
    DECLARE_DATA_FRAME(SiftFrame);
  };

class SiftUnit : public VideoFramework::VideoUnit {
public:
  SiftUnit(bool visualize_feature_points,
           const std::string& vid_stream_name = "LuminanceStream",
           const std::string& sift_stream_name = "SiftStream");
  ~SiftUnit();

  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

protected:
  bool visualize_feature_points_;

  std::string vid_stream_name_;
  std::string sift_stream_name_;

  int vid_stream_idx_;
  int sift_stream_idx_;

  int frame_width_;
  int frame_height_;
  int frame_num_;

  // The actual sift computation.
  shared_ptr<SiftGPU> sift_gpu_;
};

class SiftWriter : public VideoFramework::VideoUnit {
public:
  // If reset_frames frames is set, sift-frames will be released from memory.
  SiftWriter(bool reset_frames,
             const std::string& file_name,
             const std::string& sift_stream_name = "SiftStream");

  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

protected:
  bool reset_frames_;
  std::string file_name_;
  std::string sift_stream_name_;

  std::ofstream ofs_;

  int sift_stream_idx_;
};

class SiftReader : public VideoFramework::VideoUnit {
 public:
  SiftReader(const std::string& file_name,
             const std::string& sift_stream_name = "SiftStream");

  virtual bool OpenStreams(VideoFramework::StreamSet* set);
  virtual void ProcessFrame(VideoFramework::FrameSetPtr input,
                            std::list<VideoFramework::FrameSetPtr>* output);
  virtual void PostProcess(std::list<VideoFramework::FrameSetPtr>* append);

 protected:
  std::string file_name_;
  std::string sift_stream_name_;

  std::ifstream ifs_;
};

}

#endif  // SIFT_UNIT_H__
