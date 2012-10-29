#ifndef PIXEL_DISTANCE_H__
#define PIXEL_DISTANCE_H__

#include <opencv2/core/core.hpp>

#include "assert_log.h"

namespace Segment {
  // Common interface for fast pixel distance evaluation in scanline order.
  // // Sets position of current pixel location.
  // void MoveAnchorTo(int x, int y);
  // // Advances in scanline order.
  // void IncrementAnchor();
  // // Same for test pixel location.
  // void MoveTestAnchorTo(int x, int y);
  // void IncrementTestAnchor();
  // Returns pixel feature distance between anchor and test-anchor displaced by dx, dy.
  // dx, dy. Constraints:
  // For spatial distance dx is in {-1, 0, 1} and dy in {0, 1}.
  // For temporal distance both dx and dy are in {-1, 0, 1}.
  // float PixelDistance(int dx, int dy);

  // Can be parametrized by generic DistanceFunctions, see below for examples.
  template <class DistanceFunction>
  class SpatialCvMatDistance {
   public:
    typedef typename DistanceFunction::data_type data_type;
    enum { stride = DistanceFunction::stride };

    SpatialCvMatDistance(const cv::Mat& image,
                         const DistanceFunction& distance = DistanceFunction())
        : image_(image), distance_(distance) { }

    void MoveAnchorTo(int x, int y) {
      anchor_ptr_ = image_.ptr<data_type>(y) + stride * x;
    }

    void IncrementAnchor() {
      anchor_ptr_ += stride;
    }

    void MoveTestAnchorTo(int x, int y) {
      test_ptr_[0] = image_.ptr<data_type>(y) + stride * x;
      test_ptr_[1] = image_.ptr<data_type>(y + 1) + stride * x;
    }

    void IncrementTestAnchor() {
      test_ptr_[0] += stride;
      test_ptr_[1] += stride;
    }

    float PixelDistance(int dx, int dy) {
      return distance_(anchor_ptr_, test_ptr_[dy] + stride * dx);
    }

   private:
    const cv::Mat image_;

    const data_type* anchor_ptr_;
    const data_type* test_ptr_[2];

    DistanceFunction distance_;
  };

  template <class DistanceFunction>
  class TemporalCvMatDistance {
  public:
    typedef typename DistanceFunction::data_type data_type;
    enum { stride = DistanceFunction::stride };

    TemporalCvMatDistance(const cv::Mat& curr_image,
                          const cv::Mat& prev_image,
                          const DistanceFunction& distance = DistanceFunction())
    : curr_image_(curr_image), prev_image_(prev_image), distance_(distance) {
      ASSURE_LOG(stride == curr_image.channels());
      ASSURE_LOG(stride == prev_image.channels());
      ASSURE_LOG(curr_image.size() == prev_image.size());
     }

    void MoveAnchorTo(int x, int y) {
      anchor_ptr_ = curr_image_.ptr<data_type>(y) + stride * x;
    }

    void IncrementAnchor() {
      anchor_ptr_ += stride;
    }

    void MoveTestAnchorTo(int x, int y) {
      test_ptr_[0] = prev_image_.ptr<data_type>(y - 1) + stride * x;
      test_ptr_[1] = prev_image_.ptr<data_type>(y) + stride * x;
      test_ptr_[2] = prev_image_.ptr<data_type>(y + 1) + stride * x;
    }

    void IncrementTestAnchor() {
      test_ptr_[0] += stride;
      test_ptr_[1] += stride;
      test_ptr_[2] += stride;
    }

    float PixelDistance(int dx, int dy) {
      return distance_(anchor_ptr_, test_ptr_[dy + 1] + stride * dx);
    }

  private:
    const cv::Mat curr_image_;
    const cv::Mat prev_image_;

    const data_type* anchor_ptr_;
    const data_type* test_ptr_[3];

    DistanceFunction distance_;
  };

  // Pixel distance's for [Spatial | Temporal]CvMatDistance.

  template <class DataType, int channels>
  struct PixelDistanceFunction {
    typedef DataType data_type;
    enum { stride = channels };
  };

  struct ColorDiff3L1 : public PixelDistanceFunction<float, 3> {
    static const float denom;   // = 1.0 / 3.0.
    float operator()(const float* ptr_1, const float* ptr_2) {
      const float diff_1 = ptr_1[0] - ptr_2[0];
      const float diff_2 = ptr_1[1] - ptr_2[1];
      const float diff_3 = ptr_1[2] - ptr_2[2];
      return (fabs(diff_1) + fabs(diff_2) + fabs(diff_3)) * denom;
    }
  };

  struct ColorDiff3L2 : public PixelDistanceFunction<float, 3> {
    static const float denom;   // = 1.0 / sqrt(3.0).
    float operator()(const float* ptr_1, const float* ptr_2) {
      const float diff_1 = ptr_1[0] - ptr_2[0];
      const float diff_2 = ptr_1[1] - ptr_2[1];
      const float diff_3 = ptr_1[2] - ptr_2[2];
      return sqrt(diff_1 * diff_1 + diff_2 * diff_2 + diff_3 * diff_3) * denom;
    }
  };

  typedef SpatialCvMatDistance<ColorDiff3L1> SpatialCvMatDistance3L1;
  typedef SpatialCvMatDistance<ColorDiff3L2> SpatialCvMatDistance3L2;
  typedef TemporalCvMatDistance<ColorDiff3L1> TemporalCvMatDistance3L1;
  typedef TemporalCvMatDistance<ColorDiff3L2> TemporalCvMatDistance3L2;

  // Pixel distance aggregators.
  template<class Distance1, class Distance2, class DistanceAggregator>
  class AggregatedDistance {
   public:
    AggregatedDistance(const Distance1& dist_1,
                       const Distance2& dist_2,
                       const DistanceAggregator& aggregator)
      : dist_1_(dist_1), dist_2_(dist_2), aggregator_(aggregator) {
    }

    void MoveAnchorTo(int x, int y) {
      dist_1_.MoveAnchorTo(x, y);
      dist_2_.MoveAnchorTo(x, y);
    }

    void IncrementAnchor() {
      dist_1_.IncrementAnchor();
      dist_2_.IncrementAnchor();
    }

    void MoveTestAnchorTo(int x, int y) {
      dist_1_.MoveTestAnchorTo(x ,y);
      dist_2_.MoveTestAnchorTo(x, y);
    }

    void IncrementTestAnchor() {
      dist_1_.IncrementTestAnchor();
      dist_2_.IncrementTestAnchor();
    }

    float PixelDistance(int dx, int dy) {
      return aggregator_(dist_1_.PixelDistance(dx, dy),
                         dist_2_.PixelDistance(dx, dy));
    }

   private:
    Distance1 dist_1_;
    Distance2 dist_2_;
    DistanceAggregator aggregator_;
  };

  // Linear weighting of distances.
  // Note: Weights should sum to one.
  class LinearDistanceAggregator2 {
   public:
    LinearDistanceAggregator2(float weight_1, float weight_2)
        : weight_1_(weight_1), weight_2_(weight_2) {
    }
    float operator()(float dist_1, float dist_2) {
      return weight_1_ * dist_1 + weight_2_ * dist_2;
    }
   private:
    float weight_1_;
    float weight_2_;
  };

  // Simply multiplies distances.
  class IndependentDistanceAggregator2 {
   public:
    IndependentDistanceAggregator2() { }
    float operator()(float dist_1, float dist_2) {
      return 1.0f - (1.0f - dist_1) * (1.0f - dist_2);
    }
  };
}

#endif  // PIXEL_DISTANCE_H__
