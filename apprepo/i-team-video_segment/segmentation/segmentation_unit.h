/*
 *  segmentation_unit.h
 *  Segmentation
 *
 *  Created by Matthias Grundmann on 10/30/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef SEGMENTATION_UNIT_H__
#define SEGMENTATION_UNIT_H__

#include "segmentation.h"
#include "segmentation_io.h"
#include "segmentation_options.pb.h"
#include "video_unit.h"

#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fstream>
#include <string>
#include <vector>

namespace cv {
  class Mat;
}

namespace Segment {

  using namespace VideoFramework;
  using boost::shared_array;
  using boost::shared_ptr;
  using boost::scoped_ptr;
  using std::string;
  using std::vector;

  typedef unsigned char uchar;

  class Segmentation;
  class SegmentationWriter;

  class SegmentationStream : public DataStream {
   public:
    SegmentationStream(int frame_width,
                       int frame_height,
                       const string& stream_name = "SegmentationStream")
    : DataStream(stream_name), frame_width_(frame_width), frame_height_(frame_height) { }

    int frame_width() const { return frame_width_; }
    int frame_height() const { return frame_height_; }
   private:
    int frame_width_;
    int frame_height_;
  };


  // Derive for redefining pixel feature distances.
  // Uses L2 RGB distance between pixels by default.
  class DenseSegmentUnit : public VideoFramework::VideoUnit {
   public:
    DenseSegmentUnit(const string& options);
    ~DenseSegmentUnit();

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

    // Re-define the following functions:
    // Override to load feature specific streams.
    virtual bool OpenFeatureStreams(StreamSet* set);   // no-op by default.

    // Method to pre-compute and buffer features for a frame-set.
    // Second parameter indicates max. number of framesets required for buffering.
    // For example if buffer_size = 2, your buffer only need to contain the prev. and
    // current frameset's features (rest can be discarded). A larger buffer_size than the
    // previous one always indicates the buffer should be filled UNTIL it reaches the
    // speficied size.
    virtual void ExtractFrameSetFeatures(FrameSetPtr input,
                                         int buffer_size);

    // Override to pass image pixel features to the segmentation. Passed are the index
    // of the current element in the buffer filled and maintained by the
    // above function. Use Segmentation::AddGenericImage on curr_elem's features.
    // If constrained_segmentation != NULL, it is expected that AddGenericImageConstrained
    // is called instead of AddGenericImage.
    virtual void AddFrameToSegmentation(int curr_frame,
                                        const SegmentationDesc* constained_segmentation,
                                        Segmentation* seg);

    // Similar to above function to introduce temporal connections via
    // Segmentation::ConnectTemporally. If flow is specified use
    // ConnectTemporallyAlongFlow instead.
    virtual void ConnectSegmentationTemporally(int curr_frame,
                                               int prev_frame,
                                               const cv::Mat* flow_x,
                                               const cv::Mat* flow_y,
                                               Segmentation* seg);

   protected:
    // Accessors for read-only content.
    int video_stream_idx() const { return video_stream_idx_; }

    // Returns -1 if flow is not present.
    int flow_stream_idx() const { return flow_stream_idx_; }

    int frame_width() const { return frame_width_; }
    int frame_height() const { return frame_height_; }

    // Smooths image based on options.
    void SmoothImage(const cv::Mat& src, cv::Mat* dst) const;

   private:
    // Streaming mode.
    void SegmentAndOutputChunk(int sz,
                               int overlap_start,
                               list<FrameSetPtr>* append);

   private:
    int video_stream_idx_;
    int flow_stream_idx_;

    DenseSegmentOptions seg_options_;

    int frame_width_;
    int frame_height_;

    int input_frames_;
    int processed_chunks_;

    // Chunk processing.
    // Layout of a chunk is:
    // 0 ....               chunk_size
    // 0 ..           ^ <-> ^
    //                overlap_frames
    // 0 ..           ^  ^
    //                constraint_frames
    // We output all frames from 0 to chunk_size - overlap_frames.
    // Not all frames in the overlap are used as constraints, the rest is look-ahead
    // to avoid local minima.
    // Constraint frames are always output twice: While their segmentation agrees,
    // they contain important neighboring information needed for connections in time.
    int chunk_size_;
    int overlap_frames_;
    int constraint_frames_;
    int max_region_id_;
    int min_region_size_;

    bool dense_segmentation_filter_finished_;

    // Temporary buffers for EdgeTangent based smoothing.
    shared_ptr<CvMat> smoothing_flow_x_;
    shared_ptr<CvMat> smoothing_flow_y_;
    shared_ptr<CvMat> smoothing_step_;

    shared_ptr<cv::Mat> scaled_image_;
    shared_ptr<Segmentation> seg_;

    vector< shared_ptr<SegmentationDesc> > overlap_segmentations_;

    vector< shared_ptr<IplImage> > overlap_flow_x_;
    vector< shared_ptr<IplImage> > overlap_flow_y_;

    vector<shared_ptr<cv::Mat> > buffered_feature_images_;

    // Buffer FrameSet's for one chunk.
    list<FrameSetPtr> frame_set_buffer_;
  };

  class RegionSegmentUnit : public VideoFramework::VideoUnit {
  public:
    RegionSegmentUnit(const string& options);
    ~RegionSegmentUnit();

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);
  protected:
    // Overload the functions below to supply your own region features and distances.
    // Override to load feature specific streams, called at the end of OpenStreams.
    virtual bool OpenFeatureStreams(StreamSet* set) { return true; }  // no-op by default.

    // Returns list of feature extractors to compute descriptors for each region.
    virtual void GetDescriptorExtractorList(FrameSetPtr input,
                                            DescriptorExtractorList* extractors);

    // Returns region distance (combines per descriptor distances into one normalized
    // distance).
    virtual RegionDistance* GetRegionDistance();

 protected:
    // Accessors for read-only content.
    int video_stream_idx() const { return video_stream_idx_; }

    // Returns -1 if flow is not present.
    int flow_stream_idx() const { return flow_stream_idx_; }

    int frame_width() const { return frame_width_; }
    int frame_height() const { return frame_height_; }

  private:
    void SegmentAndOutputChunk(int overlap_start,
                               int lookahead_start,
                               list<FrameSetPtr>* append);

  private:
    int video_stream_idx_;
    int flow_stream_idx_;
    int seg_stream_idx_;

    RegionSegmentOptions seg_options_;

    int frame_width_;
    int frame_height_;
    int input_frames_;
    int read_chunks_;
    int processed_chunk_sets_;
    int last_overlap_start_;
    int last_lookahead_start_;

    // Chunk processing.
    int chunk_set_size_;
    int chunk_set_overlap_;
    int constraint_chunks_;

    vector<int> max_region_ids_;
    int discard_levels_;

    int prev_chunk_id_;
    int num_output_frames_;

    bool region_segmentation_filter_finished_;

    shared_ptr<IplImage> lab_input_;  // Input frame as lab image.
    shared_ptr<Segmentation> seg_;
    shared_ptr<Segmentation> new_seg_;

    // Buffer FrameSet's for one chunk set.
    list<FrameSetPtr> frame_set_buffer_;
  };

  // Streaming writer, frames are written in chunks of chunk_size.
  // If strip_essentials is set, we don't write protobuffer for each frame but
  // specialized binary format for annotiation flash UI while stripping some non-important  // informatation (descriptors, etc.)
  class SegmentationWriterUnit : public VideoFramework::VideoUnit {
   public:
    SegmentationWriterUnit(const string& filename,
                           bool strip_to_essentials = false,
                           const string& vid_stream_name = "VideoStream",
                           const string& seg_stream_name = "SegmentationStream",
                           int chunk_size = 20);
    ~SegmentationWriterUnit() {}

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

   private:
    string filename_;
    bool strip_to_essentials_;
    string vid_stream_name_;
    string seg_stream_name_;

    int vid_stream_idx_;
    int seg_stream_idx_;
    int frame_number_;

    SegmentationWriter writer_;
    int chunk_size_;
  };

  class SegmentationReaderUnit : public VideoFramework::VideoUnit {
  public:
    SegmentationReaderUnit(const string& filename,
                           const string& seg_stream_name = "SegmentationStream");
    ~SegmentationReaderUnit();

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);
    virtual void PostProcess(list<FrameSetPtr>* append);

  protected:
    virtual bool SeekImpl(int64_t dts);

  private:
    string seg_stream_name_;
    int seg_stream_index_;
    int frame_width_;
    int frame_height_;

    SegmentationReader reader_;
  };

  class SegmentationRenderUnit : public VideoFramework::VideoUnit {
  public:
    // You can either supply an integer hierarchy_level to select the desired granularity
    // or a fraction f in (0, 1) to select f * max_hierarchy_level.
    SegmentationRenderUnit(float blend_alpha = 0.5,
                           float hierarchy_level = 0,
                           bool highlight_edges = true,
                           bool draw_shape_descriptors = false,
                           bool concat_with_source = false,
                           const string& vid_stream_name = "VideoStream",
                           const string& seg_stream_name = "SegmentationStream",
                           const string& out_stream_name = "RenderedRegionStream");
    ~SegmentationRenderUnit() {}

    virtual bool OpenStreams(StreamSet* set);
    virtual void ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output);

    void SetHierarchyLevel(float level) { hierarchy_level_ = level; }

  private:
    string vid_stream_name_;
    string seg_stream_name_;
    string out_stream_name_;

    int vid_stream_idx_;
    int seg_stream_idx_;
    int out_stream_idx_;

    int frame_width_;
    int frame_height_;
    int frame_width_step_;

    int prev_chunk_id_;
    int frame_number_;

    shared_ptr<IplImage> render_frame_;
    shared_ptr<SegmentationDesc> seg_hier_;   // Holds the segmentation for first frame.
                                              // Hierarchy is only saved once.

    float blend_alpha_;
    float hierarchy_level_;
    bool highlight_edges_;
    bool draw_shape_descriptors_;
    bool concat_with_source_;
  };

  void SetupOptionsDescription(boost::program_options::options_description* options);

  bool SetOptionsFromVariableMap(const boost::program_options::variables_map& vm,
                                 Segment::DenseSegmentOptions* seg_options,
                                 Segment::RegionSegmentOptions* region_options);
}  // namespace Segment.

#endif // SEGMENTATION_UNIT_H__
