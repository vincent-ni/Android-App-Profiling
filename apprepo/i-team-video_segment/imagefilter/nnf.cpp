/*
 *  nnf.cpp
 *  ImageFilterLib
 *
 *  Created by Matthias Grundmann on 5/24/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include "nnf.h"
#include "image_util.h"
#include "assert_log.h"

#include <vector>
#include <boost/circular_buffer.hpp>
#include <fstream>
#include <limits>

#include <string>
using std::string;

#include <boost/random/uniform_int.hpp>
#include <boost/random/additive_combine.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/lexical_cast.hpp>

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

typedef boost::variate_generator<boost::mt19937&, boost::uniform_int<> >
  RandomGen;

// Good job! :)
#undef max
#undef min

namespace ImageFilter {

  // Adjust to 1 for big endian architectures.
  #define ALPHA_SHIFT 0

  // Returns half of the squared distance between patch src and patch ref minus
  // patch src squared.
  // Use this function to initialize patch distances with prior or random information.
  /*
  inline
  int PatchDiff(const uchar* p_src,
                const uchar* p_ref,
                const int ref_squared,
                const int src_width_step,
                const int ref_width_step,
                const int patch_rad) {
    p_src -= patch_rad * src_width_step;
    p_ref -= patch_rad * ref_width_step;

    const uchar* src_local, *ref_local;
    int ret_val = ref_squared >> 1;

    for (int i = -patch_rad;
         i <= patch_rad;
         ++i, p_src += src_width_step, p_ref += ref_width_step) {
      src_local = p_src - 4 * patch_rad;
      ref_local = p_ref - 4 * patch_rad;

      for (int j = -patch_rad; j <= patch_rad; ++j, src_local+=4, ref_local+=4)
        ret_val -= (int)src_local[ALPHA_SHIFT + 0] * (int)ref_local[ALPHA_SHIFT + 0] +
                   (int)src_local[ALPHA_SHIFT + 1] * (int)ref_local[ALPHA_SHIFT + 1] +
                   (int)src_local[ALPHA_SHIFT + 2] * (int)ref_local[ALPHA_SHIFT + 2];
    }
    return ret_val;
  }

  */

  inline
  int PatchDiff(const uchar* p_src,
                const uchar* p_ref,
                const int src_width_step,
                const int ref_width_step,
                const int patch_rad) {

    p_src -= patch_rad * src_width_step;
    p_ref -= patch_rad * ref_width_step;

    const uchar* src_local, *ref_local;
    int ret_val = 0;
    int r,g,b;

    for (int i = -patch_rad;
         i <= patch_rad;
         ++i, p_src += src_width_step, p_ref += ref_width_step) {
      src_local = p_src - 4 * patch_rad;
      ref_local = p_ref - 4 * patch_rad;

      for (int j = -patch_rad; j <= patch_rad; ++j, src_local+=4, ref_local+=4) {
        r = (int)src_local[ALPHA_SHIFT + 0] - (int)ref_local[ALPHA_SHIFT + 0];
        g = (int)src_local[ALPHA_SHIFT + 1] - (int)ref_local[ALPHA_SHIFT + 1];
        b = (int)src_local[ALPHA_SHIFT + 2] - (int)ref_local[ALPHA_SHIFT + 2];
        ret_val += r*r + g*g + b*b;
      }
    }

    return ret_val;
  }

  // Returns L2 norm of difference between the two descriptors.
  inline float DescriptorDiff(const SIFT_DESC& desc_1, const SIFT_DESC& desc_2) {
    ASSERT_LOG(desc_1.size() == desc_2.size());

    float min_val = 0;
    for (SIFT_DESC::const_iterator i = desc_1.begin(), j = desc_2.begin();
         i != desc_1.end();
         ++i, ++j) {
      float diff = *i - *j;
      min_val += fabs(diff); // * diff;
    }

    return min_val;
  }

  // Returns L2 norm of difference between the two descriptors.
  // Iteratively processes chunks of chunk_sz and terminates if norm is above cur_min.
  inline float DescriptorDiffEarlyTerminate(const SIFT_DESC& desc_1, const SIFT_DESC& desc_2,
                                            const int chunk_sz, const int num_chunks,
                                            const float cur_min, float min_val = 0) {

    ASSERT_LOG(desc_1.size() == chunk_sz * num_chunks);
    ASSERT_LOG(cur_min >= 0);

    const float* p1 = &desc_1[0];
    const float* p2 = &desc_2[0];

    for (int c = 0; c < num_chunks; ++c) {
      for (int cz = 0; cz < chunk_sz; ++cz, ++p1, ++p2) {
        float diff = *p1 - *p2;
        min_val += fabs(diff); // * diff;
      }

      // Early termination.
      if (min_val > cur_min) {
        ASSERT_LOG(min_val >= 0);
      //  ASSERT_LOG(min_val <= 128.f * 2.f + 0.01);
        return min_val;
      }
    }

    // Sq. norm of diff. vector is within [0 .. dim(SIFT_DESC) * 2.0].
    ASSERT_LOG(min_val >= 0);
    ASSERT_LOG(min_val <= 128.f * 2.f + 0.01);
    return min_val;
  }

  // Returns half of the squared distance between patch src and patch ref minus
  // patch src squared.
  // Expects the current minimum and returns a value larger than cur_minimum if patch distance is
  // larger than the current minimum using early termination.
    /*
  inline
  int PatchDiffEarlyTerminate(const uchar* p_src,
                              const uchar* p_ref,
                              const int ref_squared,
                              const int src_width_step,
                              const int ref_width_step,
                              const int patch_rad,
                              const int cur_minimum) {
    if(ref_squared == std::numeric_limits<int>::max())
      return ref_squared;

    p_src -= patch_rad * src_width_step;
    p_ref -= patch_rad * ref_width_step;

    const uchar* src_local, *ref_local;
    int ret_val = ref_squared >> 1;

    for (int i = -patch_rad;
         i <= patch_rad;
         ++i, p_src += src_width_step, p_ref += ref_width_step) {

      // Early terminate.
      // TODO: Think about how this could be done faster.
//      if (ret_val > cur_minimum)
        //return ret_val;

      src_local = p_src - 4 * patch_rad;
      ref_local = p_ref - 4 * patch_rad;

      for (int j = -patch_rad; j <= patch_rad; ++j, src_local+=4, ref_local+=4)
        ret_val -= (int)src_local[ALPHA_SHIFT + 0] * (int)ref_local[ALPHA_SHIFT + 0] +
                   (int)src_local[ALPHA_SHIFT + 1] * (int)ref_local[ALPHA_SHIFT + 1] +
                   (int)src_local[ALPHA_SHIFT + 2] * (int)ref_local[ALPHA_SHIFT + 2];

    }

    return ret_val;
  }
     */

    inline
    int PatchDiffEarlyTerminate(const uchar* p_src,
                                const uchar* p_ref,
                                const int ref_squared,
                                const int src_width_step,
                                const int ref_width_step,
                                const int patch_rad,
                                const int cur_minimum) {

      if(ref_squared == std::numeric_limits<int>::max())
        return ref_squared;

      p_src -= patch_rad * src_width_step;
      p_ref -= patch_rad * ref_width_step;

      const uchar* src_local, *ref_local;
      int r,g,b;
      int ret_val = 0;

      for (int i = -patch_rad;
           i <= patch_rad;
           ++i, p_src += src_width_step, p_ref += ref_width_step) {

        // Early terminate.
        if (ret_val > cur_minimum)
          return ret_val;

        src_local = p_src - 4 * patch_rad;
        ref_local = p_ref - 4 * patch_rad;

        for (int j = -patch_rad; j <= patch_rad; ++j, src_local+=4, ref_local+=4) {
          r = (int)src_local[ALPHA_SHIFT + 0] - (int)ref_local[ALPHA_SHIFT + 0];
          g = (int)src_local[ALPHA_SHIFT + 1] - (int)ref_local[ALPHA_SHIFT + 1];
          b = (int)src_local[ALPHA_SHIFT + 2] - (int)ref_local[ALPHA_SHIFT + 2];
          ret_val += r*r + g*g + b*b;
        }
      }
      return ret_val;
    }

  // Propagates good patch matches by determining adjacent neighbors.
  // This function relies on the fact that patch offsets outside the valid range
  // are valid in ref_patch and assigned MAX_INT.
  void PropagatePatch(const CvMat* src, const CvMat* ref, const CvMat* ref_patch,
                      CvMat* offset, CvMat* min_value, const int patch_rad, bool bottom_up) {

    const int ref_ld = ref->step / 4;

    if(!bottom_up) {
      const uchar* src_ptr = RowPtr<uchar>(src, 0);
      int* offset_ptr = RowPtr<int>(offset, 0);
      int* min_value_ptr = RowPtr<int>(min_value, 0);
      int test_idx, test_val;

      // First row.
      for (int j = 1; j < src->width; ++j) {

        // Left neighbor.
        test_idx = offset_ptr[j-1] + 1;
        test_val = PatchDiffEarlyTerminate(src_ptr + 4*j,
                                           (uchar*)(ref->data.i + test_idx),
                                           ref_patch->data.i[test_idx],
                                           src->step,
                                           ref->step,
                                           patch_rad,
                                           min_value_ptr[j]);
        if (test_val < min_value_ptr[j]) {
          min_value_ptr[j] = test_val;
          offset_ptr[j] = test_idx;
        }
      }

      // Remaining rows.
      for (int i = 1; i < src->height; ++i) {
        src_ptr = RowPtr<uchar>(src, i);
        int* prev_offset_ptr = RowPtr<int>(offset, i-1);
        offset_ptr = RowPtr<int>(offset, i);
        min_value_ptr = RowPtr<int>(min_value, i);

        // First element only checks top element.
        test_idx = prev_offset_ptr[0] + ref_ld;
        test_val = PatchDiffEarlyTerminate(src_ptr,
                                           (uchar*)(ref->data.i + test_idx),
                                           ref_patch->data.i[test_idx],
                                           src->step,
                                           ref->step,
                                           patch_rad,
                                           *min_value_ptr);
        if (test_val < *min_value_ptr) {
          *min_value_ptr = test_val;
          *offset_ptr = test_idx;
        }

        ++prev_offset_ptr;
        ++offset_ptr;
        src_ptr += 4;
        ++min_value_ptr;

        // Remaining columns.
        for (int j = 1;
             j < src->width;
             ++j, ++prev_offset_ptr, ++offset_ptr, src_ptr += 4, ++min_value_ptr) {
          // Left neighbor.
          test_idx = offset_ptr[-1] + 1;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }

          // Top neighbor.
          test_idx = prev_offset_ptr[0] + ref_ld;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }
          /*
          // Top-left neighbor.
          test_idx = prev_offset_ptr[-1] + ref_ld - 1;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }
           */
        }
      }
    } else {      // Bottom up.
      const uchar* src_ptr = RowPtr<uchar>(src, src->height-1);
      int* offset_ptr = RowPtr<int>(offset, offset->height-1);
      int* min_value_ptr = RowPtr<int>(min_value, min_value->height-1);
      int test_idx, test_val;

      // Last row.
      for (int j = src->width-2; j >= 0; --j) {
        // Right neighbor.
        test_idx = offset_ptr[j+1] - 1;
        test_val = PatchDiffEarlyTerminate(src_ptr + 4*j,
                                           (uchar*)(ref->data.i + test_idx),
                                           ref_patch->data.i[test_idx],
                                           src->step,
                                           ref->step,
                                           patch_rad,
                                           min_value_ptr[j]);
        if (test_val < min_value_ptr[j]) {
          min_value_ptr[j] = test_val;
          offset_ptr[j] = test_idx;
        }
      }

      // Remaining rows.
      for (int i = src->height-2; i >= 0; --i) {
        src_ptr = RowPtr<uchar>(src, i) + 4 * (src->width - 1);
        int* next_offset_ptr = RowPtr<int>(offset, i+1) + offset->width - 1;
        offset_ptr = RowPtr<int>(offset, i) + offset->width - 1;
        min_value_ptr = RowPtr<int>(min_value, i) + min_value->width - 1;

        // First element only checks bottom element.
        test_idx = next_offset_ptr[0] - ref_ld;
        test_val = PatchDiffEarlyTerminate(src_ptr,
                                           (uchar*)(ref->data.i + test_idx),
                                           ref_patch->data.i[test_idx],
                                           src->step,
                                           ref->step,
                                           patch_rad,
                                           *min_value_ptr);
        if (test_val < *min_value_ptr) {
          *min_value_ptr = test_val;
          *offset_ptr = test_idx;
        }

        --offset_ptr;
        --next_offset_ptr;
        --min_value_ptr;
        src_ptr -=4;

        // Remaining columns.
        for (int j = src->width - 2;
             j >= 0;
             --j, --next_offset_ptr, --offset_ptr, src_ptr -= 4, --min_value_ptr) {

          // Right neighbor.
          test_idx = offset_ptr[1] - 1;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }

          // Bottom neighbor.
          test_idx = next_offset_ptr[0] - ref_ld;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }

          /*
          // Bottom-right neighbor.
          test_idx = next_offset_ptr[1] - ref_ld - 1;
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + test_idx),
                                             ref_patch->data.i[test_idx],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_value_ptr);
          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            offset_ptr[0] = test_idx;
          }
           */
        }
      }
    }
  }

  // Uses a two-fold apprach for finding better matches.
  // a) Global Random matches are tested.
  // b) Matches within patch_rad distance to current best match are tested.
  // Goal: Faster and more accurate implementation than RandomSearch.
  void RandomSearchFast(const CvMat* src, const CvMat* ref, const CvMat* ref_patch,
                        CvMat* offset, CvMat* min_value, const int patch_rad,
                        RandomGen& rand_width, RandomGen& rand_height, RandomGen& rand_patch_rad,
                        const int global_probes, const int local_probes) {
    const int ref_ld = ref->step / 4;

    #pragma omp parallel for
    for (int i = 0; i < src->height; ++i) {
      int random_patch, test_val;
      const uchar* src_ptr = RowPtr<uchar>(src, i);
      int* offset_ptr = RowPtr<int>(offset, i);
      int* min_ptr = RowPtr<int>(min_value, i);

      for (int j = 0;
           j < src->width;
           ++j, src_ptr +=4, ++offset_ptr, ++min_ptr) {

        for (int gp = 0; gp < global_probes; ++gp) {

          // Global testing.
          random_patch = rand() % ref->width * ref_ld + rand() % ref->height;
               // rand_height() * ref_ld + rand_width();
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + random_patch),
                                             ref_patch->data.i[random_patch],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_ptr);

          if (test_val < *min_ptr) {
            *min_ptr = test_val;
            *offset_ptr = random_patch;
          }
        }

        // Local testing.
        for (int lp = 0; lp < local_probes; ++lp) {
          random_patch = *offset_ptr + (rand() % (2 * patch_rad) - patch_rad) * ref_ld +
                          (rand() % (2 * patch_rad) - patch_rad);
          //*offset_ptr + rand_patch_rad() * ref_ld + rand_patch_rad();
          test_val = PatchDiffEarlyTerminate(src_ptr,
                                             (uchar*)(ref->data.i + random_patch),
                                             ref_patch->data.i[random_patch],
                                             src->step,
                                             ref->step,
                                             patch_rad,
                                             *min_ptr);

          if (test_val < *min_ptr) {
            *min_ptr = test_val;
            *offset_ptr = random_patch;
          }
        }
      }
    }
  }

  // Test location is (x,y)
  inline float FlowCost(CvMat* offset, const int test_offset, const int width, const int height,
                       const int x, const int y, const int ref_ld, const int patch_rad) {
    int neighbor_x;
    int neighbor_y;

    int* offset_ptr = RowPtr<int>(offset, y) + x;

    float cost = 0;
    const int test_x = test_offset % ref_ld - patch_rad;
    const int test_y = test_offset / ref_ld - patch_rad;
    const float max_flow_diff = 20;
    const int test_rad = 1;

    // Get neighbors flow.
    for (int k = -test_rad; k <= test_rad; ++k) {
      for (int l = -test_rad; l <= test_rad; ++l) {
        if (y + k >= 0 && y + k < height &&
            x + l >= 0 && x + l < width && abs(k) + abs(l) < 2) {
          neighbor_x = offset_ptr[k*width + l] % ref_ld - patch_rad;
          neighbor_y = offset_ptr[k*width + l] / ref_ld - patch_rad;
          cost += std::min<float>(max_flow_diff, 2.f * fabs((float)(neighbor_x - l - test_x)) +
                                                 2.f * fabs((float)(neighbor_y - k - test_y)));
        }
      }
    }

    /*
    if (x - k >= 0) {
      neighbor_x = offset_ptr[-k] % ref_ld - patch_rad;
      neighbor_y = offset_ptr[-k] / ref_ld - patch_rad;
      cost += std::min<float>(max_flow_diff, fabs(neighbor_x + k - test_x) +
                              fabs(neighbor_y - test_y));
    }

    if (x + k < width) {
      neighbor_x = offset_ptr[k] % ref_ld - patch_rad;
      neighbor_y = offset_ptr[k] / ref_ld - patch_rad;
      cost += std::min<float>(max_flow_diff, fabs(neighbor_x - k - test_x) +
                              fabs(neighbor_y - test_y));
    }

    if (y - k >= 0) {
      neighbor_x = offset_ptr[-k*width] % ref_ld - patch_rad;
      neighbor_y = offset_ptr[-k*width] / ref_ld - patch_rad;
      cost += std::min<float>(max_flow_diff, fabs(neighbor_x - test_x) +
                              fabs(neighbor_y + k - test_y));
    }

    if (y + k < height) {
      neighbor_x = offset_ptr[k*width] % ref_ld - patch_rad;
      neighbor_y = offset_ptr[k*width] / ref_ld - patch_rad;
      cost += std::min<float>(max_flow_diff, fabs(neighbor_x - test_x) +
                              fabs(neighbor_y - k - test_y));
    }
*/

    return 0.25 * cost +
      ((test_x -x) * (test_x - x) + (test_y -y) * (test_y -y)) * 0.001;
   // return 0;
  };

  void RandomSearchFast(const vector<SIFT_DESC>& src, const vector<SIFT_DESC>& ref,
                        CvMat* offset, CvMat* min_value, int ref_width, int ref_height,
                        int patch_rad,
                        RandomGen& rand_width, RandomGen& rand_height, RandomGen& rand_patch,
                        const int global_probes, const int local_probes) {
    const int width = min_value->width;
    const int height = min_value->height;
    const int ref_ld = ref_width + 2 * patch_rad;
    int g_switched = 0;
    int l_switched = 0;

    #pragma omp parallel for
    for (int i = 0; i < height; ++i) {
      int random_desc;
      float test_val;

      const SIFT_DESC* src_ptr = &src[i*width];
      int* offset_ptr = RowPtr<int>(offset, i);
      float* min_ptr = RowPtr<float>(min_value, i);

      for (int j = 0;
           j < width;
           ++j, ++src_ptr, ++offset_ptr, ++min_ptr) {

        // Global testing.
        for (int gp = 0; gp < global_probes; ++gp) {
          // No border case here.
          int test_y = rand() % ref_height;
          int test_x = rand() % ref_width;

          random_desc = (test_y + patch_rad) * ref_ld + test_x + patch_rad;
          float test_cost = FlowCost(offset, random_desc, width, height,
                                     j, i, ref_ld, patch_rad);

          test_val = DescriptorDiffEarlyTerminate(*src_ptr, ref[random_desc], 32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_ptr, test_cost);

          if (test_val < *min_ptr) {
            *min_ptr = test_val;
            *offset_ptr = random_desc;
            ++g_switched;
          }
        }

        // Local testing.
        int x = *offset_ptr % ref_ld - patch_rad;
        int y = *offset_ptr / ref_ld - patch_rad;

        for (int lp = 0; lp < local_probes; ++lp) {
          // No border case here either.
          random_desc = (std::max(0, std::min(y + rand() % 11 - 5,
                                            height - 1)) + patch_rad) * ref_ld +
                         std::max(0, std::min(x + rand() % 11 - 5,
                                              width - 1)) + patch_rad;

          float test_cost = FlowCost(offset, random_desc, width, height,
                                     j, i, ref_ld, patch_rad);

          test_val = DescriptorDiffEarlyTerminate(*src_ptr, ref[random_desc], 32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_ptr, test_cost);
          if (test_val < *min_ptr) {
            *min_ptr = test_val;
            *offset_ptr = random_desc;
            ++l_switched;
          }
        }
      }
    }

    std::cout << "Random Search switched " << g_switched << " (g), " << l_switched << "(l)\n";
  }

  void PropagatePatch(const vector<SIFT_DESC>& src, const vector<SIFT_DESC>& ref, int ref_ld,
                      int patch_rad, CvMat* offset, CvMat* min_value, bool bottom_up) {
    const int width = offset->width;
    const int height = offset->height;

    if(!bottom_up) {
      const SIFT_DESC* src_ptr = &src[0];
      int* offset_ptr = RowPtr<int>(offset, 0);
      float* min_value_ptr = RowPtr<float>(min_value, 0);
      int test_idx = 0;
      float test_val = 1e10;
      int num_changed = 0;

      // First row.
      for (int j = 1; j < width; ++j) {
        // Left neighbor.
        test_idx = offset_ptr[j-1] + 1;
        float test_cost = FlowCost(offset, test_idx, width, height,
                                   j, 0, ref_ld, patch_rad);

        test_val = DescriptorDiffEarlyTerminate(src_ptr[j],
                                                ref[test_idx],
                                                32,
                                                SIFT_DESC::static_size / 32,
                                                min_value_ptr[j],
                                                test_cost);

        if (test_val < min_value_ptr[j]) {
          min_value_ptr[j] = test_val;
          offset_ptr[j] = test_idx;
          ++num_changed;
        }
      }

      // Remaining rows.
      for (int i = 1; i < height; ++i) {
        src_ptr = &src[i * width];
        int* prev_offset_ptr = RowPtr<int>(offset, i-1);
        offset_ptr = RowPtr<int>(offset, i);
        min_value_ptr = RowPtr<float>(min_value, i);

        // First element only checks top element.
        test_idx = prev_offset_ptr[0] + ref_ld;
        float test_cost = FlowCost(offset, test_idx, width, height,
                                   0, i, ref_ld, patch_rad);
        test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                ref[test_idx],
                                                32,
                                                SIFT_DESC::static_size / 32,
                                                *min_value_ptr,
                                                test_cost);

        if (test_val < *min_value_ptr) {
          *min_value_ptr = test_val;
          *offset_ptr = test_idx;
          ++num_changed;
        }

        ++prev_offset_ptr;
        ++offset_ptr;
        ++src_ptr;
        ++min_value_ptr;

        // Remaining columns.
        for (int j = 1;
             j < width;
             ++j, ++prev_offset_ptr, ++offset_ptr, ++src_ptr, ++min_value_ptr) {

          // Left neighbor.
          test_idx = offset_ptr[-1] + 1;
          float test_cost = FlowCost(offset, test_idx, width, height,
                                     j, i, ref_ld, patch_rad);
          test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                  ref[test_idx],
                                                  32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_value_ptr,
                                                  test_cost);

          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            *offset_ptr = test_idx;
          }

          // Top neighbor.
          test_idx = prev_offset_ptr[0] + ref_ld;
          test_cost = FlowCost(offset, test_idx, width, height,
                                     j, i, ref_ld, patch_rad);
          test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                  ref[test_idx],
                                                  32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_value_ptr,
                                                  test_cost);

          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            *offset_ptr = test_idx;
            ++num_changed;
          }
        }
      }
      std::cout << "Propagate Patch, Top-Down: " << (float)num_changed / src.size() << "\n";
    } else {
      // Bottom up-case.
      const SIFT_DESC* src_ptr = &src[(height - 1) * width];
      int* offset_ptr = RowPtr<int>(offset, height-1);
      float* min_value_ptr = RowPtr<float>(min_value, height-1);
      int test_idx;
      float test_val;
      int num_changed = 0;

      // Last row.
      for (int j = width-2; j >= 0; --j) {
        // Right neighbor.
        test_idx = offset_ptr[j+1] - 1;
        float test_cost = FlowCost(offset, test_idx, width, height,
                                   j, height - 1, ref_ld, patch_rad);
        test_val = DescriptorDiffEarlyTerminate(src_ptr[j],
                                                ref[test_idx],
                                                32,
                                                SIFT_DESC::static_size / 32,
                                                min_value_ptr[j],
                                                test_cost);

        if (test_val < min_value_ptr[j]) {
          min_value_ptr[j] = test_val;
          offset_ptr[j] = test_idx;
          ++num_changed;
        }
      }

      // Remaining rows.
      for (int i = height - 2; i >= 0; --i) {
        src_ptr = &src[i * width + width - 1];
        int* next_offset_ptr = RowPtr<int>(offset, i+1) + width - 1;
        offset_ptr = RowPtr<int>(offset, i) + width - 1;
        min_value_ptr = RowPtr<float>(min_value, i) + width - 1;

        // Last element only checks bottom element.
        test_idx = next_offset_ptr[0] - ref_ld;
        float test_cost = FlowCost(offset, test_idx, width, height,
                                   width - 1, i, ref_ld, patch_rad);
        test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                ref[test_idx],
                                                32,
                                                SIFT_DESC::static_size / 32,
                                                *min_value_ptr,
                                                test_cost);

        if (test_val < *min_value_ptr) {
          *min_value_ptr = test_val;
          *offset_ptr = test_idx;
          ++num_changed;
        }

        --offset_ptr;
        --next_offset_ptr;
        --min_value_ptr;
        --src_ptr;

        // Remaining columns.
        for (int j = width - 2;
             j >= 0;
             --j, --next_offset_ptr, --offset_ptr, --src_ptr, --min_value_ptr) {

          // Right neighbor.
          test_idx = offset_ptr[1] - 1;
          float test_cost = FlowCost(offset, test_idx, width, height,
                                     j, i, ref_ld, patch_rad);
          test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                  ref[test_idx],
                                                  32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_value_ptr,
                                                  test_cost);

          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            *offset_ptr = test_idx;
          }

          // Bottom neighbor.
          test_idx = next_offset_ptr[0] - ref_ld;
          test_cost = FlowCost(offset, test_idx, width, height,
                                     j, i, ref_ld, patch_rad);
          test_val = DescriptorDiffEarlyTerminate(*src_ptr,
                                                  ref[test_idx],
                                                  32,
                                                  SIFT_DESC::static_size / 32,
                                                  *min_value_ptr,
                                                  test_cost);

          if (test_val < *min_value_ptr) {
            *min_value_ptr = test_val;
            *offset_ptr = test_idx;
            ++num_changed;
          }
        }
      }

      std::cout << "Propagate Patch, Bottom-Up: " << (float)num_changed / src.size() << "\n";
    }
  }


// Tests random offsets within a (1/2)^k > 3 distance for better matches.
void RandomSearch(const CvMat* src, const CvMat* ref, const CvMat* ref_patch,
                  CvMat* offset, CvMat* min_value, const int patch_rad,
                  RandomGen& rand_width, RandomGen& rand_height, int probes) {
    const int ref_ld = ref->step / 4;

    int test_x, test_y, test_idx, test_val, best_val, best_idx;

    vector<int> width_fracs(1, ref->width);
    while (width_fracs.back() > 3)
      width_fracs.push_back(width_fracs.back() >> 1);

    vector<int> height_fracs(1, ref->height);
    while (height_fracs.back() > 3)
      height_fracs.push_back(height_fracs.back() >> 1);

    const int frac_steps = std::min<int>(width_fracs.size(), height_fracs.size());

    for (int i = 0; i < src->height; ++i) {
      const uchar* src_ptr = RowPtr<uchar>(src, i);
      int* offset_ptr = RowPtr<int>(offset, i);
      int* min_ptr = RowPtr<int>(min_value, i);

      for (int j = 0;
           j < src->width;
           ++j, src_ptr +=4, ++offset_ptr, ++min_ptr) {

        int cur_y = *offset_ptr / ref_ld;
        int cur_x = *offset_ptr % ref_ld;

        ASSERT_LOG(cur_x >= 0 && cur_y >= 0);
        ASSERT_LOG(cur_x < ref->width && cur_y < ref->height);

        best_val = *min_ptr;
        best_idx = *offset_ptr;

        for (int k = 1; k < frac_steps; ++k) {
          for (int p = 0; p < probes; ++p) {
            test_x = cur_x + (rand_width() >> k); // - std::min(cur_x, width_fracs[k+1]);
            test_y = cur_y + (rand_height() >> k); // - std::min(cur_y, height_fracs[k+1]);

            // Different from modulo.
            if (test_x < 0 ) test_x += width_fracs[k];
            if (test_y < 0 ) test_y += height_fracs[k];
            if (test_x >= ref->width) test_x -= width_fracs[k];
            if (test_y >= ref->height) test_y -= height_fracs[k];

            ASSERT_LOG(test_x >= 0 && test_y >= 0);
            ASSERT_LOG(test_x < ref->width && test_y < ref->height);

            test_idx = test_y * ref_ld + test_x;
            test_val = PatchDiffEarlyTerminate(src_ptr,
                                               (uchar*)(ref->data.i + test_idx),
                                               ref_patch->data.i[test_idx],
                                               src->step,
                                               ref->step,
                                               patch_rad,
                                               *min_ptr);
            if (test_val < best_val) {
              best_val = test_val;
              best_idx = test_idx;
            }
          }
        }

        // Update.
        *min_ptr = best_val;
        *offset_ptr = best_idx;
      }
    }
  }

  // Compute for each patch q in src sum(q^2)
  void ComputePatchSquared(const CvMat* src, const int patch_rad, CvMat* output) {
    shared_ptr<CvMat> tmp_image = cvCreateMatShared(src->height + 2 * patch_rad, src->width,
                                                    CV_32S);

  //  #pragma omp parallel
    {
      boost::circular_buffer<int> last_results(2 * patch_rad + 1);

      // Process rows first.
      #pragma omp for
      for (int i = -patch_rad; i < src->height + patch_rad; ++i) {
        const uchar* src_ptr = RowPtr<uchar>(src, i);
        int* dst_ptr = RowPtr<int>(tmp_image, i + patch_rad);
        last_results.clear();

        int val = 0;
        *dst_ptr = 0;

        // Calculate first patch in row.
        for (int r = -patch_rad; r <= patch_rad; ++r) {
          const int r4 = 4*r;
          val = (int)src_ptr[r4 + ALPHA_SHIFT] * (int)src_ptr[r4 + ALPHA_SHIFT] +
                (int)src_ptr[r4 + ALPHA_SHIFT + 1] * (int)src_ptr[r4 + ALPHA_SHIFT + 1] +
                (int)src_ptr[r4 + ALPHA_SHIFT + 2] * (int)src_ptr[r4 + ALPHA_SHIFT + 2];
          last_results.push_back(val);
          *dst_ptr += val;
        }

        // First patch processed.
        ++dst_ptr;
        // Point to unvisited element.
        src_ptr += 4 * (patch_rad+1);

        for (int j = 1; j < src->width; ++j, ++dst_ptr, src_ptr += 4) {
          // Get previous value, remove last elem and add new one.
          val = (int)src_ptr[ALPHA_SHIFT + 0] * (int)src_ptr[ALPHA_SHIFT + 0] +
                (int)src_ptr[ALPHA_SHIFT + 1] * (int)src_ptr[ALPHA_SHIFT + 1] +
                (int)src_ptr[ALPHA_SHIFT + 2] * (int)src_ptr[ALPHA_SHIFT + 2];
          *dst_ptr = dst_ptr[-1] - last_results[0] + val;
          last_results.push_back(val);
        }
      }
    }

   // #pragma omp parallel
    {
      boost::circular_buffer<int> last_results(2 * patch_rad + 1);

      // Columns.
      #pragma omp for
      for (int i = 0; i < src->width; ++i) {
        const int* src_ptr = RowPtr<int>(tmp_image, patch_rad)+i;
        int* dst_ptr = output->data.i + i;
        last_results.clear();

        *dst_ptr = 0;

        // Calculate first patch in column.
        for (int r = -patch_rad; r <= patch_rad; ++r) {
          const int* val_ptr = PtrOffset(src_ptr, r * tmp_image->step);
          *dst_ptr += *val_ptr;
          last_results.push_back(*val_ptr);
        }

        // First patch processed.
        const int* prev_dst_ptr = dst_ptr;
        dst_ptr = PtrOffset(dst_ptr, output->step);
        // Point to unvisited element.
        src_ptr = PtrOffset(src_ptr, (patch_rad + 1) * tmp_image->step);

        for (int j = 1;
             j < src->height;
             ++j, dst_ptr = PtrOffset(dst_ptr, output->step),
             src_ptr = PtrOffset(src_ptr, tmp_image->step)) {
          *dst_ptr = *prev_dst_ptr - last_results[0] + src_ptr[0];
          last_results.push_back(src_ptr[0]);
          prev_dst_ptr = dst_ptr;
        }
      }
    }
  }


  void VisualizeOffsetMap(string filename, const CvMat* offset, const CvMat* ref) {

    shared_ptr<IplImage> hsv_vis = cvCreateImageShared(offset->width, offset->height,
                                                       IPL_DEPTH_8U, 3);
    shared_ptr<IplImage> rgb_vis = cvCreateImageShared(offset->width, offset->height,
                                                       IPL_DEPTH_8U, 3);
    int ref_width_step = ref->step / 4;
    const float diam = std::sqrt((float)(ref->width * ref->width + ref->height * ref->height)) / 2.f;

    for (int i = 0; i < offset->height; ++i) {
      const int* offset_ptr = RowPtr<int>(offset, i);
      uchar* out_ptr = RowPtr<uchar>(hsv_vis, i);

      for (int j = 0; j < offset->width; ++j, ++offset_ptr, out_ptr += 3) {
        float dx = *offset_ptr % ref_width_step - j; // offset->width/2 - j;
        float dy = *offset_ptr / ref_width_step - i; // offset->height/2 - i;

        out_ptr[0] = ((atan2(dy,dx) / M_PI + 1) * 90);
        out_ptr[1] = 255 - sqrt(dx*dx + dy*dy) / diam * 255;
        out_ptr[2] = 255;
      }
    }

    cvCvtColor(hsv_vis.get(), rgb_vis.get(), CV_HSV2BGR);
    cvSaveImage(filename.c_str(), rgb_vis.get());
  }


  void ComputeFlowColor(float fx, float fy, uchar *pix);

  void VisualizeOffsetMap(string filename, const CvMat* offset, const int ref_ld,
                          const int patch_rad, float ref_diam) {

    shared_ptr<IplImage> rgb_vis = cvCreateImageShared(offset->width, offset->height,
                                                       IPL_DEPTH_8U, 3);
    const float diam = ref_diam;

    // Compute max. rad.
    float max_rad = 0;
    for (int i = 0; i < offset->height; ++i) {
      const int* offset_ptr = RowPtr<int>(offset, i);
      for (int j = 0; j < offset->width; ++j, ++offset_ptr) {
        float dx = *offset_ptr % ref_ld - patch_rad - j; // offset->width/2 - j;
        float dy = *offset_ptr / ref_ld - patch_rad - i; // offset->height/2 - i;
        max_rad = std::max(max_rad, std::sqrt(dx * dx + dy * dy));
      }
    }

    const float max_rad_inv = 1.f / max_rad * 2.f;

    for (int i = 0; i < offset->height; ++i) {
      const int* offset_ptr = RowPtr<int>(offset, i);
      uchar* out_ptr = RowPtr<uchar>(rgb_vis, i);

      for (int j = 0; j < offset->width; ++j, ++offset_ptr, out_ptr += 3) {
        float dx = *offset_ptr % ref_ld - patch_rad - j; // offset->width/2 - j;
        float dy = *offset_ptr / ref_ld - patch_rad - i; // offset->height/2 - i;

       ComputeFlowColor(-dx * max_rad_inv, -dy * max_rad_inv, out_ptr);

//        out_ptr[0] = ((atan2(dy,dx) / M_PI + 1) * 90);
 //       out_ptr[1] = 255 - sqrt(dx*dx + dy*dy) / diam * 255;
 //       out_ptr[2] = 255;

      }
    }

  //  cvCvtColor(hsv_vis.get(), rgb_vis.get(), CV_HSV2BGR);
    cvSaveImage(filename.c_str(), rgb_vis.get());
  }

  void ComputeNNF(const IplImage* src_img,
                  const IplImage* ref_img,
                  int patch_rad,
                  int iterations,
                  CvMat* offset_mat,
                  vector<const uchar*>* offset_vec,
                  CvMat* min_values) {

    // Input check.
    if (offset_mat) {
      ASSURE_LOG(src_img->width - 2 * patch_rad == offset_mat->width &&
                 src_img->height - 2 * patch_rad == offset_mat->height)
          << "offset matrix must be of same size as image src minus two times the patch radius.";
    }

    if (offset_vec) {
      ASSURE_LOG((src_img->width - 2 * patch_rad) * (src_img->height - 2 * patch_rad)
                 == offset_vec->size())
          << "offset_vec vector must be of same size as image src minus two times the patch radius.";
    }

    ASSURE_LOG(patch_rad <= 40) << "Maximum supported patch size is 80x80 pixels.";

    ASSURE_LOG(src_img->nChannels == 4 && src_img->nChannels == 4)
        << "Source and reference image must be of type RGB32.";

    ASSURE_LOG(src_img->widthStep == src_img->width * 4 &&
               ref_img->widthStep == ref_img->width * 4)
        << "Source and reference image must be border free. Try multiple of 4 for image width.";

    CvMat ref_mat;
    CvMat* ref = cvMatFromImageROI(ref_img, cvRect(patch_rad,
                                                   patch_rad,
                                                   ref_img->width - 2 * patch_rad,
                                                   ref_img->height - 2 * patch_rad), &ref_mat);


    shared_ptr<CvMat> tmp_image = cvCreateMatShared(ref->height + 2 * patch_rad, ref->width,
                                                    CV_32S);

    // Create a border of size patch_rad around ref to use same adressing like for ref.
    shared_ptr<IplImage> ref_patch_img = cvCreateImageShared(ref_img->width, ref_img->height,
                                                             IPL_DEPTH_32S, 1);

    // Initialize to MAX_INT.
    // Marks out of bounds offsets as too costly and prevents evaluation of those.
    cvSet(ref_patch_img.get(), cvScalar(std::numeric_limits<int>::max()));

    CvMat ref_patch_mat;
    CvMat* ref_patch = cvMatFromImageROI(ref_patch_img.get(), cvRect(patch_rad, patch_rad,
                                                               ref->width, ref->height),
                                         &ref_patch_mat);
    cvSet(ref_patch, cvScalar(0));
    //ComputePatchSquared(ref, patch_rad, ref_patch);

    /*
    std::ofstream ofs("squared_test.txt");
    for (int i = 0; i < ref_patch->height; ++i) {
      int* src = RowPtr<int>(ref_patch, i);
      std::copy(src, src + ref_patch->width, std::ostream_iterator<int>(ofs, " "));
      if ( i < ref_patch->height -1 )
        ofs << "\n";
    }
    */
    // Create random offset field.
    // TODO: offer assignment based on prior information.
    shared_ptr<CvMat> my_offset_matrix;
    CvMat* offset = offset_mat;
    if (!offset) {
      my_offset_matrix = cvCreateMatShared(src_img->height - 2 * patch_rad,
                                           src_img->width - 2 * patch_rad,
                                           CV_32S);
      offset = my_offset_matrix.get();
    }

    shared_ptr<CvMat> min_matrix;
    if (!min_values) {
      min_matrix = cvCreateMatShared(offset->rows, offset->cols, CV_32S);
      min_values = min_matrix.get();
    }

    // Random initialization.
    CvMat src_mat;
    CvMat* src = cvMatFromImageROI(src_img, cvRect(patch_rad,
                                                   patch_rad,
                                                   src_img->width - 2 * patch_rad,
                                                   src_img->height - 2 * patch_rad), &src_mat);

    // Uniform distributed random reference offset.
    boost::uniform_int<> int_width_dist(0, ref->width-1);
    boost::uniform_int<> int_height_dist(0, ref->height-1);
    boost::uniform_int<> int_patch_rad_dist(-patch_rad, patch_rad);
    boost::mt19937 generator_width;
    boost::mt19937 generator_height;
    boost::mt19937 generator_patch_rad;
    RandomGen random_width(generator_width, int_width_dist);
    RandomGen random_height(generator_height, int_height_dist);
    RandomGen random_patch_rad(generator_patch_rad, int_patch_rad_dist);

    for (int i = 0, idx = 0; i < src->height; ++i) {
      int* off_local = RowPtr<int>(offset, i);
      int* min_value = RowPtr<int>(min_values, i);
      const uchar* src_value = RowPtr<uchar>(src, i);

      for (int j = 0;
           j < src->width;
           ++j, ++idx, ++off_local, ++min_value, src_value += 4) {
        *off_local = //rand() % ref->height * ref_img->width + rand() % ref->width;
                    random_height() * ref_img->width + random_width();
        *min_value = PatchDiff(src_value, (uchar*)(ref->data.i + *off_local),
                               src->step, ref->step, patch_rad);
      }
    }

    RandomSearchFast(src, ref, ref_patch, offset, min_values, patch_rad, random_width,
                     random_height, random_patch_rad, 0, 2*patch_rad);
    const float ref_diam = std::sqrt((float)(ref_img->width * ref_img->width +
                                             ref_img->height * ref_img->height));
    // Iterate.
    for (int i = 0; i < iterations; ++i) {
      VisualizeOffsetMap(string("offset_map") + boost::lexical_cast<string>(i) + ".png",
                         offset, ref); //ref_img->widthStep / 4, ref_diam);

      std::cout << "ComputeNNF: Iteration " << i+1 << " of " << iterations << "\n";

      PropagatePatch(src, ref, ref_patch, offset, min_values, patch_rad, false);

      RandomSearchFast(src, ref, ref_patch, offset, min_values, patch_rad, random_width,
                       random_height, random_patch_rad, 4, 4);
      PropagatePatch(src, ref, ref_patch, offset, min_values, patch_rad, true);
    }

    // Convert offset matrix to direct pointers
    if (offset_vec) {
      const uchar** dst_ptr = &(*offset_vec)[0];
      for (int i = 0; i < src->height; ++i) {
        int* src_ptr = RowPtr<int>(offset, i);

        for (int j = 0; j < src->width; ++j, ++dst_ptr, ++src_ptr) {
          *dst_ptr = (uchar*)(ref->data.i + *src_ptr);
        //  *dst_ptr = RowPtr<uchar>(src, i) + 4*j;
        }
      }
    }
  }

  void ComputeNNF(const int width,
                  const int height,
                  const vector<SIFT_DESC>& src,
                  const int ref_width,
                  const int ref_height,
                  const vector<SIFT_DESC>& ref,
                  const int patch_rad,
                  int iterations,
                  CvMat* offset_mat,
                  CvMat* min_values) {
    if (offset_mat)
      ASSURE_LOG(offset_mat->width == width &&
                 offset_mat->height == height) << "Offset matrix dim. differ from specified ones.\n";

    if (min_values) {
      ASSURE_LOG(min_values->width == width &&
                 min_values->height == height) << "Min value matrix dim. differ from specified ones.\n";
    //  ASSURE_LOG(min_values->depth == IPL_DEPTH_32F) << "Min value matrix must hold floating pt. values.\n";
    }

    shared_ptr<CvMat> min_matrix;
    if (!min_values) {
      min_matrix = cvCreateMatShared(offset_mat->rows, offset_mat->cols, CV_32F);
      min_values = min_matrix.get();
    }

    cvSet(min_values, cvScalar(1e5));

    // Uniform distributed random reference offset.
    boost::uniform_int<> int_width_dist(0, ref_width-1);
    boost::uniform_int<> int_height_dist(0, ref_height-1);
    boost::uniform_int<> int_patch_rad_dist(-5, 5);
    boost::mt19937 generator_width;
    boost::mt19937 generator_height;
    boost::mt19937 generator_patch_rad;
    RandomGen random_width(generator_width, int_width_dist);
    RandomGen random_height(generator_height, int_height_dist);
    RandomGen random_patch_rad(generator_patch_rad, int_patch_rad_dist);
    const int ref_ld = ref_width + 2 * patch_rad;

    if (offset_mat->step != 4 * width)
      std::cerr << "\n\nERROR: offset_mat stepping is off.\n\n";

    for (int i = 0; i < height; ++i) {
      int* off_local = RowPtr<int>(offset_mat, i);
      float* min_value = RowPtr<float>(min_values, i);
      const SIFT_DESC* src_value = &src[i*width];

      for (int j = 0;
           j < width;
           ++j, ++off_local, ++min_value, ++src_value) {
        const int x = random_width(); //rand() % ref_width;
        const int y = random_height(); //rand() % ref_height;
        *off_local = (y + patch_rad) * ref_ld + x + patch_rad;
        // No boundary cases here.
        *min_value = DescriptorDiff(*src_value, ref[*off_local]);
        ASSERT_LOG(*min_value >= 0);
        ASSERT_LOG(*min_value <= 128.0f * 2.f + 0.1f);   // SIFT DESC is normalized.
      }
    }

    // Add flow cost.
    for (int i = 0; i < height; ++i) {
      int* off_local = RowPtr<int>(offset_mat, i);
      float* min_value = RowPtr<float>(min_values, i);

      for (int j = 0;
           j < width;
           ++j, ++off_local, ++min_value) {
        *min_value += FlowCost(offset_mat, *off_local, width, height,
                               j, i, ref_ld, patch_rad);
      }
    }

    RandomSearchFast(src, ref, offset_mat, min_values, ref_width, ref_height, patch_rad,
                     random_width, random_height, random_patch_rad, 100, 30);

    const int ref_diam = std::sqrt((float)(ref_width * ref_width + ref_height * ref_height));

    // Iterate.
    for (int i = 0; i < iterations; ++i) {
      VisualizeOffsetMap(string("sift_offset_map") + boost::lexical_cast<string>(i) + ".png",
                         offset_mat, ref_ld, patch_rad, ref_diam);

      std::cout << "ComputeNNF: Iteration " << i+1 << " of " << iterations << "\n";
      std::cout << "Current flow error: " << cvSum(min_values).val[0] << "\n";
      PropagatePatch(src, ref, ref_ld, patch_rad, offset_mat, min_values, false);

      RandomSearchFast(src, ref, offset_mat, min_values, ref_width, ref_height, patch_rad,
                       random_width, random_height, random_patch_rad, 50, 10);

      PropagatePatch(src, ref, ref_ld, patch_rad, offset_mat, min_values, true);
    }

    PropagatePatch(src, ref, ref_ld, patch_rad, offset_mat, min_values, false);
    PropagatePatch(src, ref, ref_ld, patch_rad, offset_mat, min_values, true);
    VisualizeOffsetMap(string("sift_offset_map_final.png"),
                       offset_mat, ref_ld, patch_rad, ref_diam);
  }

  void AveragePatches(const IplImage* src,
                      const vector<const uchar*>& offset,
                      int patch_rad,
                      CvMat* min_values,
                      IplImage* output) {

    ASSURE_LOG((output->width - 2 * patch_rad) * (output->height - 2 * patch_rad)
               == offset.size()) << "Offset and output image are of inconsistent size.";

    shared_ptr<IplImage> sum_img = cvCreateImageShared(output->width, output->height,
                                                       IPL_DEPTH_32F, 3);
    cvSet(sum_img.get(), cvScalar(0));

    CvMat sum_mat;
    CvMat* sum = cvMatFromImageROI(sum_img.get(), cvRect(patch_rad, patch_rad,
                                                         output->width - 2 * patch_rad,
                                                         output->height - 2 * patch_rad),
                                   &sum_mat);

    shared_ptr<IplImage> weight_img = cvCreateImageShared(output->width, output->height,
                                                       IPL_DEPTH_32F, 1);
    cvSet(weight_img.get(), cvScalar(0));

    CvMat weight_mat;
    CvMat* weight = cvMatFromImageROI(weight_img.get(), cvRect(patch_rad, patch_rad,
                                                        output->width - 2 * patch_rad,
                                                        output->height - 2 * patch_rad),
                                      &weight_mat);
    float cur_weight;

    for (int i = 0; i < sum->height; ++i) {
      float* sum_ptr = RowPtr<float>(sum, i);
      float* weight_ptr = RowPtr<float>(weight, i);
      int* min_ptr = RowPtr<int>(min_values, i);
      const uchar* const* offset_ptr = &offset[i * (output->width - 2*patch_rad)];

      for(int j = 0; j < sum->width; ++j, ++offset_ptr, ++weight_ptr, ++min_ptr, sum_ptr+=3) {
        // Add patch
        for (int k = -patch_rad; k <= patch_rad; ++k) {
          const uchar* off_local = *offset_ptr + k * src->widthStep - 4 * patch_rad;
          float* sum_local = PtrOffset(sum_ptr, k * sum->step) - 3 * patch_rad;
          float* weight_local = PtrOffset(weight_ptr, k * weight->step) - patch_rad;
          int* min_local = PtrOffset(min_ptr, k * min_values->step) - patch_rad;

          for (int l = -patch_rad;
               l <= patch_rad;
               ++l, off_local +=4, ++weight_local, ++min_local, sum_local +=3) {
            cur_weight = 1.0 / ((float)*min_local + 1e-6);
            *weight_local += cur_weight;
            sum_local[0] += (float)off_local[ALPHA_SHIFT + 0] * cur_weight;
            sum_local[1] += (float)off_local[ALPHA_SHIFT + 1] * cur_weight;
            sum_local[2] += (float)off_local[ALPHA_SHIFT + 2] * cur_weight;
          }
        }
      }
    }

    // TODO: convert output to 32 bit image.
    // right now: 24 for testing purposes.

    for (int i = 0; i < sum_img->height; ++i) {
      float* sum_ptr = RowPtr<float>(sum_img, i);
      const float* weight_ptr = RowPtr<float>(weight_img, i);
      uchar* out_ptr = RowPtr<uchar>(output, i);
      for (int j = 0; j < sum_img->width; ++j, sum_ptr += 3, ++weight_ptr, out_ptr += 3) {
        cur_weight = 1.0 / *weight_ptr;
        out_ptr[0] = sum_ptr[0]  *cur_weight;
        out_ptr[1] = sum_ptr[1]  *cur_weight;
        out_ptr[2] = sum_ptr[2]  *cur_weight;
      }
    }
  }

  // colorcode.cpp
  //
  // Color encoding of flow vectors
  // adapted from the color circle idea described at
  //   http://members.shaw.ca/quadibloc/other/colint.htm
  //
  // Daniel Scharstein, 4/2007
  // added tick marks and out-of-range coding 6/05/07

  #include <stdlib.h>
  #include <math.h>
  typedef unsigned char uchar;

  int ncols = 0;
  #define MAXCOLS 60
  int colorwheel[MAXCOLS][3];


  void setcols(int r, int g, int b, int k)
  {
    colorwheel[k][0] = r;
    colorwheel[k][1] = g;
    colorwheel[k][2] = b;
  }

  void makecolorwheel()
  {
    // relative lengths of color transitions:
    // these are chosen based on perceptual similarity
    // (e.g. one can distinguish more shades between red and yellow
    //  than between yellow and green)
    int RY = 15;
    int YG = 6;
    int GC = 4;
    int CB = 11;
    int BM = 13;
    int MR = 6;
    ncols = RY + YG + GC + CB + BM + MR;
    //printf("ncols = %d\n", ncols);
    if (ncols > MAXCOLS)
      exit(1);
    int i;
    int k = 0;
    for (i = 0; i < RY; i++) setcols(255,	   255*i/RY,	 0,	       k++);
    for (i = 0; i < YG; i++) setcols(255-255*i/YG, 255,		 0,	       k++);
    for (i = 0; i < GC; i++) setcols(0,		   255,		 255*i/GC,     k++);
    for (i = 0; i < CB; i++) setcols(0,		   255-255*i/CB, 255,	       k++);
    for (i = 0; i < BM; i++) setcols(255*i/BM,	   0,		 255,	       k++);
    for (i = 0; i < MR; i++) setcols(255,	   0,		 255-255*i/MR, k++);
  }

  void ComputeFlowColor(float fx, float fy, uchar *pix) {
    if (ncols == 0)
      makecolorwheel();

    float rad = sqrt(fx * fx + fy * fy);
    float a = atan2(-fy, -fx) / M_PI;
    float fk = (a + 1.0) / 2.0 * (ncols-1);
    int k0 = (int)fk;
    int k1 = (k0 + 1) % ncols;
    float f = fk - k0;
    //f = 0; // uncomment to see original color wheel
    for (int b = 0; b < 3; b++) {
      float col0 = colorwheel[k0][b] / 255.0;
      float col1 = colorwheel[k1][b] / 255.0;
      float col = (1 - f) * col0 + f * col1;
      if (rad <= 1)
        col = 1 - rad * (1 - col); // increase saturation with radius
      else
        col *= .75; // out of range
      pix[2-b] = (int)(255.0 * col);
    }
  }
}
