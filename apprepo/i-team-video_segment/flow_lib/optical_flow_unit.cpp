/*
 *  optical_flow_unit.cpp
 *  VideoFramework
 *
 *  Created by Matthias Grundmann on 7/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "optical_flow_unit.h"

#include <iostream>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include <ext/hash_map>
using __gnu_cxx::hash_map;

#include <opencv2/video/tracking.hpp>

#include "assert_log.h"
#include "image_util.h"
#include "region_flow.h"
#include "segmentation_util.h"

using namespace ImageFilter;

namespace VideoFramework {

  DEFINE_DATA_FRAME(PolygonFrame)
  DEFINE_DATA_FRAME(OpticalFlowFrame)
  bool WithinConvexPoly(const CvPoint2D32f& pt,
                        const vector<CvPoint2D32f>& poly,
                        float dist) {
    for (int i = 1; i < poly.size(); ++i) {
      CvPoint2D32f diff = poly[i] - poly[i-1];
      CvPoint2D32f normal = normalize(cvPoint2D32f(-diff.y, diff.x));
      if (dot(normal, pt - poly[i-1]) < -dist)
        return false;
    }

    return true;
  }


  OpticalFlowFrame::OpticalFlowFrame(int matched_frames, bool is_key_frame)
      : matched_frames_(matched_frames),
        is_key_frame_(is_key_frame),
        features_(matched_frames),
        frame_ids_(matched_frames) {

  }

  int OpticalFlowFrame::AddFeatures(const CvPoint2D32f* prev_features,
                                    const CvPoint2D32f* cur_features,
                                    const char* feat_mask,
                                    int max_features,
                                    float threshold_dist,
                                    int frame_num,
                                    int frame_id,
                                    float with_noise_sigma) {

    ASSURE_LOG(frame_num < matched_frames_)
      << "frame_num exceeds matched_frames constructor argument";

    features_[frame_num].reserve(max_features);
    const float max_dist = threshold_dist * (pow(abs(frame_id), 0.85) + 1);

    for (int i = 0; i < max_features; ++i) {
      if (feat_mask[i]) {
        TrackedFeature feat(cur_features[i],
                             cvPoint2D32f(prev_features[i].x - cur_features[i].x,
                                          prev_features[i].y - cur_features[i].y));

        if (sqrt(feat.vec.x * feat.vec.x + feat.vec.y * feat.vec.y) < max_dist) {
          if (with_noise_sigma) {
            feat.vec.x += 2.f * ((float)rand() / RAND_MAX - .5f) * with_noise_sigma;
            feat.vec.y += 2.f * ((float)rand() / RAND_MAX - .5f) * with_noise_sigma;
          }
          features_[frame_num].push_back(feat);
        }
      }
    }

    frame_ids_[frame_num] = frame_id;
    return features_[frame_num].size();
  }

  namespace {
  typedef OpticalFlowFrame::TrackedFeature TrackedFeature;
  typedef vector<vector<TrackedFeature> > FeatureBins;

  void OutlierRejection(FeatureBins* feature_bins) {
    // Run ransac on each bin.
    vector<TrackedFeature> inlier_set;
    vector<TrackedFeature> best_inlier_set;
    const size_t min_inlier_set = 7;

    for (FeatureBins::iterator f_list = feature_bins->begin();
         f_list != feature_bins->end();
         ++f_list) {
      if (f_list->size() < min_inlier_set) {
        f_list->clear();
        continue;
      }

      best_inlier_set.clear();

      int max_iterations = 50;
      for (int i = 0; i < max_iterations; ++i) {
        // Pick vector randomly.
        CvPoint2D32f vec = f_list->at(rand() % f_list->size()).vec;
        inlier_set.clear();

        // Compute inlier set.
        for (vector<TrackedFeature>::const_iterator j = f_list->begin();
             j != f_list->end();
             ++j) {
          CvPoint2D32f diff = j->vec - vec;
          if (sq_norm(diff) < 3.84) {
            inlier_set.push_back(*j);
          }
        }

        // Assure probability of an outlier is not zero.
        const float eps = clamp(1.f  - (float)inlier_set.size() / (float) f_list->size(),
                                1e-6f, 1.f - 1e-6f);
        // -4.6052 == log(1 - 0.99)
        max_iterations = (int)(-4.6052f / std::log(1.f - (1.f - eps)));
        max_iterations = std::min(max_iterations, 10);

        if (inlier_set.size() > best_inlier_set.size() &&
            inlier_set.size() >= min_inlier_set) {
          inlier_set.swap(best_inlier_set);
        }
      }

      if (best_inlier_set.size() >= 3) {
        f_list->swap(best_inlier_set);
      } else {
        f_list->clear();
      }
    }
  }

  struct FeatureHasher {
    FeatureHasher(int frame_width_) : frame_width(frame_width_) { }

    size_t operator()(const TrackedFeature& f) const {
      return (int)f.loc.y * frame_width + (int)f.loc.x;
    }

    int frame_width;
  };

  struct PointHasher {
    PointHasher(int frame_width_) : frame_width(frame_width_) { }

    size_t operator()(const CvPoint2D32f& pt) const {
      return (int)pt.y * frame_width + (int)pt.x;
    }

    int frame_width;
  };

  // Features are deemed equal, if location is equal.
  struct FeatureComparator {
    bool operator()(const TrackedFeature& lhs, const TrackedFeature& rhs) const {
      return lhs.loc.x == rhs.loc.x && lhs.loc.y == rhs.loc.y;
    }
  };

  struct PointComparator {
    bool operator()(const CvPoint2D32f& lhs, const CvPoint2D32f& rhs) {
      return lhs.x == rhs.x && lhs.y == rhs.y;
    }
  };

  typedef hash_map<TrackedFeature, bool, FeatureHasher, FeatureComparator>
    FeatureHash;

  typedef hash_map<CvPoint2D32f, vector<CvPoint2D32f>, PointHasher, PointComparator>
    FeatureMatchHash;

  }  // namespace.

  void OpticalFlowFrame::ImposeLocallyConsistentGridFlow(int frame_width,
                                                         int frame_height,
                                                         int frame_num,
                                                         float grid_ratio_x,
                                                         float grid_ratio_y,
                                                         int num_offsets,
                                                         int num_scales) {
    FeatureHash feature_hash(features_[frame_num].size(), FeatureHasher(frame_width));

    for (int s = 0; s < num_scales; ++s) {
      const float ratio_x = grid_ratio_x / (float)(1 << s);
      const float ratio_y = grid_ratio_y / (float)(1 << s);
      const int grid_size_x = ratio_x * frame_width;
      const int grid_size_y = ratio_y * frame_height;

      const float inv_grid_size_x = 1.0f / grid_size_x;
      const float inv_grid_size_y = 1.0f / grid_size_y;

      for (int i = 0; i < num_offsets; ++i) {
        for (int j = 0; j < num_offsets; ++j) {
          const int offset_x = grid_size_x * j / num_offsets;
          const int offset_y = grid_size_y * i / num_offsets;
          const float diff_x = j == 0 ? 0.f : grid_size_x - offset_x;
          const float diff_y = i == 0 ? 0.f : grid_size_y - offset_y;
          const int grid_num_x = ceil((frame_width + diff_x) * inv_grid_size_x);
          const int grid_num_y = ceil((frame_height + diff_y) * inv_grid_size_y);

          const int grid_size = grid_num_x * grid_num_y;
          FeatureBins feature_bins(grid_size);

          // Put features into grid bins.
          for (vector<TrackedFeature>::const_iterator feat = features_[frame_num].begin();
               feat != features_[frame_num].end();
               ++feat) {
            const int bin_x = (feat->loc.x + diff_x) * inv_grid_size_x;
            const int bin_y = (feat->loc.y + diff_y) * inv_grid_size_y;
            feature_bins[bin_y * grid_num_x + bin_x].push_back(*feat);
          }

          OutlierRejection(&feature_bins);

          // Hash inliers.
          for (FeatureBins::const_iterator bin = feature_bins.begin();
               bin != feature_bins.end();
               ++bin) {
            for (vector<TrackedFeature>::const_iterator feat = bin->begin();
                 feat != bin->end();
                 ++feat) {
              feature_hash[*feat] = true;
            }
          }
        }
      }
    }

    // Unpack from grid to one vector of features.
    features_[frame_num].clear();
    for (FeatureHash::const_iterator bucket = feature_hash.begin();
         bucket != feature_hash.end();
         ++bucket) {
      features_[frame_num].push_back(bucket->first);
    }
  }

  void OpticalFlowFrame::GetFeatureMatchList(int frame_width,
                                             int min_matches,
                                             FeatureMatchList* feature_list) {
    ASSURE_LOG(feature_list);
    feature_list->clear();

    if (MatchedFrames() == 0) {
      return;
    }

    FeatureMatchHash match_hash(features_[0].size(), PointHasher(frame_width));

    // Blindly insert first matches.
    for (vector<TrackedFeature>::const_iterator feat = features_[0].begin();
         feat != features_[0].end();
         ++feat) {
      match_hash.insert(std::make_pair(feat->loc,
                                       vector<CvPoint2D32f>(1,
                                                            feat->loc + feat->vec)));
    }

    for (int f = 1; f < matched_frames_; ++f) {
      for (vector<TrackedFeature>::const_iterator feat = features_[f].begin();
           feat != features_[f].end();
           ++feat) {
        FeatureMatchHash::iterator feat_iter = match_hash.find(feat->loc);
        if (feat_iter == match_hash.end()) {
          continue;
        }

        // Test if not truncated.
        if (feat_iter->second.size() == f) {
          feat_iter->second.push_back(feat->loc + feat->vec);
        }
      }
    }

    // Convert hash to list.
    for (FeatureMatchHash::const_iterator bucket = match_hash.begin();
         bucket != match_hash.end();
         ++bucket) {
      vector<CvPoint2D32f> points;
      points.push_back(bucket->first);
      points.insert(points.end(), bucket->second.begin(), bucket->second.end());
      if (points.size() > min_matches) {
        feature_list->push_back(points);
      }
    }
  }

  void OpticalFlowFrame::FeatureMatchListToFlowFrame(
      const FeatureMatchList& feature_matches,
      FlowFrame* flow_frame) {
    ASSURE_LOG(flow_frame);
    flow_frame->Clear();

    for (FeatureMatchList::const_iterator matches = feature_matches.begin();
         matches != feature_matches.end();
         ++matches) {
      FlowMatches* flow_matches = flow_frame->add_matches();
      for (vector<CvPoint2D32f>::const_iterator pt = matches->begin();
           pt != matches->end();
           ++pt) {
        FlowPoint* flow_point = flow_matches->add_point();
        flow_point->set_x(pt->x);
        flow_point->set_y(pt->y);
      }
    }
  }

  void OpticalFlowFrame::FillVector(std::vector<TrackedFeature>* features,
                                    int frame_num) const {
    ASSURE_LOG(frame_num < matched_frames_);
    *features = features_[frame_num];
  }


  OpticalFlowUnit::OpticalFlowUnit(float feat_density,
                                   int frame_num,
                                   int matching_stride,
                                   const string& video_stream_name,
                                   const string& flow_stream_name,
                                   const string& poly_stream_name)
      : feat_density_(feat_density),
        frame_num_(frame_num),
        matching_stride_(matching_stride),
        video_stream_name_(video_stream_name),
        flow_stream_name_(flow_stream_name),
        poly_stream_name_(poly_stream_name) {
    keyframe_tracking_ = false;
  }

  OpticalFlowUnit::OpticalFlowUnit(float feat_density,
                                   int frame_num,
                                   const string& video_stream_name,
                                   const string& flow_stream_name,
                                   const string& seg_stream_name,
                                   const string& region_flow_stream_name,
                                   const string& key_frame_stream_name)
      : feat_density_(feat_density),
        frame_num_(frame_num),
        matching_stride_(1),
        video_stream_name_(video_stream_name),
        flow_stream_name_(flow_stream_name),
        seg_stream_name_(seg_stream_name),
        region_flow_stream_name_(region_flow_stream_name),
        key_frame_stream_name_(key_frame_stream_name) {
    keyframe_tracking_ = true;
  }

  OpticalFlowUnit::~OpticalFlowUnit() {
  }

  bool OpticalFlowUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(video_stream_name_, set);
    ASSURE_LOG(video_stream_idx_ >= 0) << "Could not find Video stream!";

    const VideoStream* vid_stream =
        dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();
    max_features_ = frame_width_ * feat_density_ * frame_height_* feat_density_;

    prev_features_.reset(new CvPoint2D32f[max_features_]);

    cur_features_.reset(new CvPoint2D32f[max_features_]);
    flow_status_.reset(new char[max_features_]);
    flow_track_error_.reset(new float[max_features_]);

    if (vid_stream->pixel_format() != PIXEL_FORMAT_LUMINANCE) {
      std::cerr << "OpticalFlowUnit::OpenStreams: "
                << "Expecting 8-bit grayscale video stream. Use converter unit upfront.";
    }

    DataStream* flow_stream = new DataStream(flow_stream_name_);
    set->push_back(shared_ptr<DataStream>(flow_stream));

    // Allocate temporary images.
    eig_image_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_32F, 1);
    tmp_image_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_32F, 1);
    prev_image_ = boost::circular_buffer<shared_ptr<IplImage> >(
        frame_num_ * matching_stride_);

    cur_pyramid_ = cvCreateImageShared(frame_width_ + 8,
                                       frame_height_ / 2,
                                       IPL_DEPTH_8U, 1);

    for (int i = 0; i < frame_num_; ++i) {
      prev_pyramid_.push_back(cvCreateImageShared(frame_width_ + 8,
                                                  frame_height_ / 2,
                                                  IPL_DEPTH_8U,
                                                  1));
    }

    processed_frames_ = 0;

    return true;
  }

  bool OpticalFlowUnit::OpenFeedbackStreams(const StreamSet& set) {
    if (keyframe_tracking_) {
      std::cout << "OpticalFlowUnit: Key frame tracking activated.\n";

      seg_stream_idx_ = FindStreamIdx(seg_stream_name_, &set);

      if (seg_stream_idx_ < 0) {
        std::cerr << "OpticalFlowUnit::OpenFeedbackStreams: "
        << "Expecting segmentation stream " << seg_stream_name_ << "\n";
        return false;
      }

      region_flow_idx_ = FindStreamIdx(region_flow_stream_name_, &set);

      if (region_flow_idx_ < 0) {
        std::cerr << "OpticalFlowUnit::OpenFeedbackStreams: "
        << "Expecting RegionFlow stream " << region_flow_stream_name_ << "\n";
        return false;
      }

      // Allocate tmp. id image.
      id_img_.reset(new int[frame_width_ * frame_height_]);

      key_frame_stream_idx_ = FindStreamIdx(key_frame_stream_name_, &set);
      last_key_frame_ = 0;
    } else {
    // PolygonStream?
    if (poly_stream_name_.size()) {
      poly_stream_idx_ = FindStreamIdx(poly_stream_name_, &set);
      if (poly_stream_idx_ < 0) {
        std::cerr << "Could not find PolyStream!\n";
        return false;
        }
      }
    }
    return true;
  }

  void OpticalFlowUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
    PreviousFrameTrack(input, output);
  }

  void OpticalFlowUnit::ProcessFrame(FrameSetPtr input,
                                     const FrameSet& feedback_input,
                                     list<FrameSetPtr>* output) {
    if (keyframe_tracking_) {
      KeyFrameTrack(input, feedback_input, output);
    } else if (poly_stream_name_.size()) {
      const PolygonFrame* poly_frame =
      dynamic_cast<const PolygonFrame*>(feedback_input.at(poly_stream_idx_).get());

      if (poly_frame) {
        vector<CvPoint2D32f> poly = poly_frame->GetPoints();
        PreviousFrameTrack(input, output, &poly);
      } else {
        PreviousFrameTrack(input, output);
      }
    }
  }

  void OpticalFlowUnit::PostProcess(list<FrameSetPtr>* append) {

  }

  void OpticalFlowUnit::PreviousFrameTrack(FrameSetPtr input,
                                           list<FrameSetPtr>* output,
                                           const vector<CvPoint2D32f>* polygon) {
    // Get luminance IplImage.
    const VideoFrame* frame =
        dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());

    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);

    ASSERT_LOG(image.nChannels == 1);

    // First match always direct predecessor.
    int num_matches = processed_frames_ > 0 ? 1 : 0;
    if (processed_frames_ > 0) {
      // For t = num_matches - 1, we need have
      // processed_frames_ - 1 - t * matching_stride_ >= 0
      // t * matching_stride <= processed_frames_ - 1
      // num_matches - 1  <= (processed_frames_ - 1) / matching_stride
      // num_matches <= (processed_frames_ - 1) / matching_stride + 1
      num_matches += std::min(frame_num_ - 1,
                             (processed_frames_ - 1) / matching_stride_);
    }

    shared_ptr<OpticalFlowFrame> off(new OpticalFlowFrame(num_matches, false));

    int num_features = max_features_;
    const float frame_diam = sqrt((float)(frame_width_ * frame_width_ +
                                          frame_height_ * frame_height_));

    // Extract features in current image.
    // Change param to .05 for fewer features and more speed.
    cvGoodFeaturesToTrack(&image,
                          eig_image_.get(),
                          tmp_image_.get(),
                          cur_features_.get(),
                          &num_features,
                          .005,
                          5,
                          0);

    if (polygon) {
      // Reject all features out of bounds.
      vector<CvPoint2D32f> outline(*polygon);
      for (int i = 0; i < outline.size(); ++i) {
        outline[i].y = frame_height_ - 1 - outline[i].y;
      }
      outline.push_back(outline[0]);

      // Use prev. features as tmp. buffer
      int p = 0;
      for (int i = 0; i < num_features; ++i) {
        CvPoint2D32f pt = cur_features_[i];
        pt.y = frame_height_ - 1 - pt.y;
        if (WithinConvexPoly(pt, outline, 0))
          prev_features_[p++] = cur_features_[i];
      }

      cur_features_.swap(prev_features_);
      num_features = p;
    }

    CvTermCriteria term_criteria = cvTermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS,
                                                  50, .02);
    int flow_flags = 0;

    for (int t = 0; t < off->MatchedFrames(); ++t ) {
      int prev_frame = processed_frames_ - 1 - t * matching_stride_;
      ASSERT_LOG(prev_frame >= 0);

      // Find correspond features in previous image for each extracted
      // feature in current frame.
      cvCalcOpticalFlowPyrLK(&image,
                             prev_image_[t * matching_stride_].get(),
                             cur_pyramid_.get(),
                             prev_pyramid_[0].get(),  // just one pyramid here.
                             cur_features_.get(),
                             prev_features_.get(),
                             num_features,
                             cvSize(10, 10),
                             3,
                             flow_status_.get(),
                             flow_track_error_.get(),
                             term_criteria,
                             flow_flags);

      flow_flags |= CV_LKFLOW_PYR_A_READY;
      flow_flags |= CV_LKFLOW_INITIAL_GUESSES;

      off->AddFeatures(prev_features_.get(),
                       cur_features_.get(),
                       flow_status_.get(),
                       num_features,
                       frame_diam * .05,
                       t,
                       -1 - t * matching_stride_,
                       0.f);
    }

    input->push_back(off);
    prev_image_.push_front(cvCloneImageShared(&image));

    ++processed_frames_;

    output->push_back(input);
  }

  void OpticalFlowUnit::KeyFrameTrack(FrameSetPtr input, const FrameSet& feedback_input,
                                      list<FrameSetPtr>* output) {
    // Get luminance IplImage.
    const VideoFrame* frame = dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());
    ASSERT_LOG(frame);

    IplImage image;
    frame->ImageView(&image);

    ASSERT_LOG(image.nChannels == 1);

    const int retrack_dist_ = 1000;

    bool reset_key_frame = false;
    if (key_frame_stream_idx_ >= 0 && processed_frames_ > 0) {
      const ValueFrame<bool>* key_frame =
          dynamic_cast<ValueFrame<bool>*>(feedback_input.at(key_frame_stream_idx_).get());

      reset_key_frame = key_frame->value();
    }

    // We set a key-frame is reset_key_frame is already set
    // or frame_num_ frames have passed.
    if (processed_frames_ - last_key_frame_ >= frame_num_) {
      reset_key_frame = true;
    }

    const int frames_to_track = 1;

    int frame_size = 0;           // Size of optical flow frame.
    if (processed_frames_ > 0) {
      // Key-frame: Track all frames until last key-frame.
      if (reset_key_frame)
        frame_size = processed_frames_ - last_key_frame_;
      else if (processed_frames_ - last_key_frame_ <= frames_to_track) {
         // Second frame. Prev. frame and key-frame are identical.
        frame_size = processed_frames_ - last_key_frame_;
      }
      else if ((processed_frames_ - last_key_frame_) % retrack_dist_ == 0)
        frame_size = processed_frames_ - last_key_frame_;
      else
        frame_size = frames_to_track + 1;   // plus key-frame track.
    }

    shared_ptr<OpticalFlowFrame> off(new OpticalFlowFrame(frame_size, reset_key_frame));

    int num_features = max_features_;

    // Track w.r.t. previous frame
    if (processed_frames_ > 0) {
      // Extract features in current image.
      cvGoodFeaturesToTrack(&image, eig_image_.get(), tmp_image_.get(),
                            cur_features_.get(), &num_features, .005, 2, 0);

      CvTermCriteria term_criteria = cvTermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS,
                                                    50, .02);
      int flow_flags = 0;

      // Track w.r.t. previous frame.
      // If key-frame, track w.r.t to all previous frames until last key-frame.
      int tracked_frames = std::min(frame_size, frames_to_track); //1;
      if (reset_key_frame)
        tracked_frames = processed_frames_ - last_key_frame_;
      else {
        // Do a dense track from time to time to rid out false matches.
        if ((processed_frames_ - last_key_frame_) % retrack_dist_ == 0)
          tracked_frames = processed_frames_ - last_key_frame_;
      }


      for (int t = 0; t < tracked_frames; ++t) {
        ASSURE_LOG(processed_frames_ - t - 1 >= 0);
        cvCalcOpticalFlowPyrLK(&image,
                               prev_image_[t].get(),
                               cur_pyramid_.get(),
                               prev_pyramid_[t].get(),
                               cur_features_.get(),
                               prev_features_.get(),
                               num_features,
                               cvSize(10, 10),
                               3,
                               flow_status_.get(),
                               flow_track_error_.get(),
                               term_criteria, flow_flags);

        flow_flags |= CV_LKFLOW_INITIAL_GUESSES;
        flow_flags |= CV_LKFLOW_PYR_A_READY;
        float frame_diam = sqrt((float)(frame_width_ * frame_width_ +
                                        frame_height_ * frame_height_));

        off->AddFeatures(prev_features_.get(), cur_features_.get(),
                         flow_status_.get(), num_features, frame_diam * .05,
                         t, -t - 1, 0.f);
      }

      if (reset_key_frame) {
        // Is key-frame --> reinitialize.
        last_key_frame_ = processed_frames_;
      // Test for sufficient data and no retrack happens.
      } else if (processed_frames_ - last_key_frame_ > frames_to_track &&
                 (processed_frames_ - last_key_frame_) % retrack_dist_ != 0) {
        // Track w.r.t. prev. key-frame.
        const DataFrame* seg_frame = feedback_input[seg_stream_idx_].get();
        Segment::SegmentationDesc desc;
        desc.ParseFromArray(seg_frame->data(), seg_frame->size());

        // Expect per-frame segmentation.
        ASSURE_LOG(desc.hierarchy_size() > 1)
            << "Hierarchy expected to be present in every frame.";

        const RegionFlowFrame* flow_frame =
            dynamic_cast<const RegionFlowFrame*>(feedback_input[region_flow_idx_].get());

        // Get region_flow w.r.t prev. key_frame, which is always the last frame in region_flow.
        vector<RegionFlowFrame::RegionFlow> region_flow;
        flow_frame->FillVector(&region_flow, flow_frame->MatchedFrames() - 1);
        ASSURE_LOG(flow_frame->FrameID(flow_frame->MatchedFrames() - 1) + processed_frames_ - 1
                   == last_key_frame_);

        int level = flow_frame->Level();

        Segment::SegmentationDescToIdImage(id_img_.get(),
                                           frame_width_ * sizeof(int),    // width_step
                                           frame_width_,
                                           frame_height_,
                                           level,
                                           desc,
                                           0);  // no hierarchy.

        // Initialize prev. feature locations with guess from region flow.
        int tracked_feature = 0;

        // Buffer region vector lookups.
        // Neighbor processing later on, can be costly if executed for each feature indepently.
        hash_map<int, CvPoint2D32f> region_vec;

        for (int i = 0; i < num_features; ++i) {
          int x = prev_features_[i].x + 0.5;
          int y = prev_features_[i].y + 0.5;

          x = std::max(0, std::min(x, frame_width_ - 1));
          y = std::max(0, std::min(y, frame_height_ - 1));

          int region_id = id_img_[y * frame_width_ + x];

          // Buffered?
          hash_map<int, CvPoint2D32f>::const_iterator look_up_loc = region_vec.find(region_id);
          if (look_up_loc != region_vec.end() &&
              look_up_loc->first == region_id) {
            prev_features_[i] = prev_features_[i] + look_up_loc->second;
            ++tracked_feature;
            continue;
          }

          RegionFlowFrame::RegionFlow rf_to_find;
          rf_to_find.region_id = region_id;
          vector<RegionFlowFrame::RegionFlow>::const_iterator rf_loc =
          std::lower_bound(region_flow.begin(), region_flow.end(), rf_to_find);

          if(rf_loc != region_flow.end() &&
             rf_loc->region_id == region_id) {
            prev_features_[i] = prev_features_[i] + rf_loc->mean_vector;
            region_vec[region_id] = rf_loc->mean_vector;
            ++tracked_feature;
          } else {
            // Poll the neighbors recursively.
            int neighbor_matches = 0;
            CvPoint2D32f neighbor_vec = cvPoint2D32f(0, 0);

            const int max_rounds = 2;
            vector<int> to_visit(1, region_id);
            vector<int> visited;

            for (int round = 0; round < max_rounds; ++round) {
              // Add all neighbors of all elements in to_visit that are not in visited to to_visit.
              vector<int> new_to_visit;

              for (vector<int>::const_iterator v = to_visit.begin(); v != to_visit.end(); ++v) {
                const Segment::CompoundRegion& comp_region =
                    GetCompoundRegionFromId(*v, desc.hierarchy(level));

                for (int j = 0,
                     n_sz = comp_region.neighbor_id_size();
                     j < n_sz;
                     ++j) {
                  int n_id = comp_region.neighbor_id(j);
                  vector<int>::const_iterator visit_loc = std::lower_bound(visited.begin(),
                                                                             visited.end(),
                                                                             n_id);
                  if (visit_loc == visited.end() ||
                      *visit_loc != n_id)
                    new_to_visit.push_back(n_id);
                }
              }

              // Remove duplicates from new_to_vist.
              std::sort(new_to_visit.begin(), new_to_visit.end());
              vector<int>::iterator new_to_visit_end = std::unique(new_to_visit.begin(),
                                                                   new_to_visit.end());

              // Process all members in new_to_visit.
              for (vector<int>::const_iterator v = new_to_visit.begin();
                   v != new_to_visit_end;
                   ++v) {
                RegionFlowFrame::RegionFlow rf_to_find;
                rf_to_find.region_id = *v;

                vector<RegionFlowFrame::RegionFlow>::const_iterator rf_loc =
                std::lower_bound(region_flow.begin(), region_flow.end(), rf_to_find);

                if(rf_loc != region_flow.end() &&
                   rf_loc->region_id == rf_to_find.region_id) {
                  ++neighbor_matches;
                  neighbor_vec = neighbor_vec + rf_loc->mean_vector;
                }
              }

              if (neighbor_matches > 0) {
                ++tracked_feature;
                prev_features_[i] = prev_features_[i] + neighbor_vec * (1.0f / neighbor_matches);
                region_vec[region_id] = neighbor_vec * (1.0f / neighbor_matches);
                break;
              }

              // Setup next round.
              visited.insert(visited.end(), to_visit.begin(), to_visit.end());
              to_visit.swap(new_to_visit);
              visited.insert(visited.end(), to_visit.begin(), to_visit.end());
            }
          }  // end polling neighbors.
        } // end for loop over features.

        std::cout << "Tracked feature ratio: " << (float)tracked_feature / num_features << "\n";

        // Compute flow to key frame.
        int t = processed_frames_ - last_key_frame_ - 1;
        flow_flags |= CV_LKFLOW_INITIAL_GUESSES;
        flow_flags |= CV_LKFLOW_PYR_A_READY;

        cvCalcOpticalFlowPyrLK(&image,
                               prev_image_[t].get(),
                               cur_pyramid_.get(),
                               prev_pyramid_[t].get(),
                               cur_features_.get(),
                               prev_features_.get(),
                               num_features,
                               cvSize(10, 10),
                               3,
                               flow_status_.get(),
                               flow_track_error_.get(),
                               term_criteria, flow_flags);

        float frame_diam = sqrt((float)(frame_width_ * frame_width_ +
                                        frame_height_ * frame_height_));

        // Add to end.
        off->AddFeatures(prev_features_.get(), cur_features_.get(),
                         flow_status_.get(), num_features, frame_diam * .05,
                         frame_size - 1, -t - 1, 0.f);

      }
    }

    input->push_back(off);
    prev_image_.push_front(cvCloneImageShared(&image));

    ++processed_frames_;
    output->push_back(input);
  }

  bool OpticalFlowUnit::SeekImpl(int64_t pts) {
    processed_frames_ = 0;
    return true;
  }

  bool FlowUtilityUnit::OpenStreams(StreamSet* set) {
    // Find video stream idx.
    video_stream_idx_ = FindStreamIdx(options_.video_stream_name(), set);

    ASSURE_LOG(video_stream_idx_ >= 0) << "Could not find video stream.";

    const VideoStream* vid_stream =
        dynamic_cast<const VideoStream*>(set->at(video_stream_idx_).get());

    frame_width_ = vid_stream->frame_width();
    frame_height_ = vid_stream->frame_height();

    flow_stream_idx_ = FindStreamIdx(options_.flow_stream_name(), set);
    ASSURE_LOG(flow_stream_idx_ >= 0) << "Could not find flow stream.";

    if (!options_.feature_match_file().empty()) {
      // Truncate file.
      ofs_.open(options_.feature_match_file().c_str(),
                std::ios_base::out | std::ios_base::binary);
    }

    return true;
  }

  void FlowUtilityUnit::ProcessFrame(FrameSetPtr input,
                                     list<FrameSetPtr>* output) {

    OpticalFlowFrame* flow_frame =
        dynamic_cast<OpticalFlowFrame*>(input->at(flow_stream_idx_).get());

    if (options_.impose_grid_flow()) {
      for (int f = 0; f < flow_frame->MatchedFrames(); ++f) {
        flow_frame->ImposeLocallyConsistentGridFlow(frame_width_,
                                                    frame_height_,
                                                    f,
                                                    options_.grid_ratio_x(),
                                                    options_.grid_ratio_y(),
                                                    options_.grid_num_offsets(),
                                                    options_.grid_num_scales());
      }
    }

    FeatureMatchList feature_list;
    if (options_.plot_flow() || !options_.feature_match_file().empty()) {
      flow_frame->GetFeatureMatchList(frame_width_,
                                      flow_frame->MatchedFrames() *
                                          options_.frac_feature_matches(),
                                      &feature_list);
    }

    if (options_.plot_flow()) {
      const VideoFrame* video_frame =
            dynamic_cast<const VideoFrame*>(input->at(video_stream_idx_).get());

      ASSERT_LOG(video_frame);

      IplImage image;
      video_frame->ImageView(&image);
      PlotFlow(feature_list, flow_frame->MatchedFrames(), &image);
    }

    if (!options_.feature_match_file().empty()) {
      // Save feature_list to file.
      FlowFrame proto_frame;
      OpticalFlowFrame::FeatureMatchListToFlowFrame(feature_list, &proto_frame);
      int frame_size = proto_frame.ByteSize();
      ofs_.write(reinterpret_cast<const char*>(&frame_size), sizeof(frame_size));
      proto_frame.SerializeToOstream(&ofs_);
    }

    output->push_back(input);
  }

  void FlowUtilityUnit::PostProcess(list<FrameSetPtr>* append) {
    if (!options_.feature_match_file().empty()) {
      ofs_.close();
    }
  }

  void FlowUtilityUnit::PlotFlow(const FeatureMatchList& feature_list,
                                 int max_matches,
                                 IplImage* frame) {
    for (FeatureMatchList::const_iterator matches = feature_list.begin();
         matches != feature_list.end();
         ++matches) {
      for (vector<CvPoint2D32f>::const_iterator pt = matches->begin();
           pt + 1 != matches->end();
           ++pt) {
        const int idx = pt - matches->begin();
        cvLine(frame, trunc(pt[0]), trunc(pt[1]),
               cvScalar(0, 255 - idx * 160 / max_matches, 0));
      }
    }
  }
}
