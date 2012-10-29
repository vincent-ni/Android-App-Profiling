/*
 *  MainWindow.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/2/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include <QtGui>
#include <QString>
#include <iostream>
#include <string>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "main_window.h"
#include "image_util.h"

#include <fstream>
#include <opencv2/highgui/highgui_c.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <clapack.h>
#endif

using namespace ImageFilter;

void FixedHomography(const vector<QPointF>& poly, float* H) {
  vector<QPointF> origs(4);
  origs[0] = QPointF(0, 0);
  origs[1] = QPointF(0, 1);
  origs[2] = QPointF(1, 1);
  origs[3] = QPointF(1, 0);

  int a_step;

  float* a_mat = new float[8 * 9 * sizeof(float)];
  a_step = 9 * sizeof(float);
  memset(a_mat, 0, a_step * 8);

  int idx = 0;
  for (int i =0; i < 4; ++i, idx +=2) {
    float* row_1 = PtrOffset(a_mat, idx * a_step);
    float* row_2 = PtrOffset(a_mat, (idx + 1) * a_step);

    CvPoint2D32f loc = cvPoint2D32f(origs[i].x(), origs[i].y());
    CvPoint2D32f prev_loc = cvPoint2D32f(poly[i].x(), poly[i].y());

    row_1[0] = row_1[1] = row_1[2] = 0.0;

    row_1[3] = -loc.x;
    row_1[4] = -loc.y;
    row_1[5] = -1.0;

    row_1[6] = loc.x * prev_loc.y;
    row_1[7] = loc.y * prev_loc.y;
    row_1[8] = prev_loc.y;

    row_2[0] = loc.x;
    row_2[1] = loc.y;
    row_2[2] = 1.0;

    row_2[3] = row_2[4] = row_2[5] = 0.0;

    row_2[6] = -loc.x * prev_loc.x;
    row_2[7] = -loc.y * prev_loc.x;
    row_2[8] = -prev_loc.x;
  }

  // Calculate SVD of A^T.
  // We need the last column.
  int svd_m = 9;
  int svd_n = 8;
  int lda = a_step / sizeof(float);
  ASSERT_LOG(a_step % sizeof(float) == 0) << "Step must be multiple of sizeof(float)\n";
  float singular_values[9];
  int lwork = (5 * svd_m + svd_n) * 100;
  float* work = new float[lwork];
  int info;

  shared_array<float> U(new float[9 * 9]);
  int nine = 9;

  sgesvd_("A",   // jobu
          "N",   // jobvt
          &svd_m,
          &svd_n,
          a_mat,
          &lda,
          singular_values,
          &U[0],   // u
          &nine,   // ldu
          0,   // vt
          &lda,   // ldvt
          work,
          &lwork,
          &info);

  if (info != 0) {
    std::cerr << "Error in SVD!\n";
  }

  delete [] work;

  float* h = &U[8*9];

  // Some basic normalization and output.
  for (int i = 0; i < 9; ++i) {
    H[i] = h[i];
  }
}

void EstimateHomography(const vector<std::pair<CvPoint2D32f, CvPoint2D32f> >& matches,
                        float* H) {
  typedef vector<std::pair<CvPoint2D32f, CvPoint2D32f> > MatchList;

  // Compute normalization transform for feature points.
  CvPoint2D32f loc_centroid = cvPoint2D32f(0.f, 0.f);
  CvPoint2D32f prev_loc_centroid = cvPoint2D32f(0.f, 0.f);

  for (MatchList::const_iterator feat = matches.begin();
       feat != matches.end();
       ++feat) {
    loc_centroid = loc_centroid + feat->first;
    prev_loc_centroid = loc_centroid + feat->second;
  }

  loc_centroid = loc_centroid * (1.0 / matches.size());
  prev_loc_centroid = prev_loc_centroid * (1.0 / matches.size());

  float loc_scale = 0;
  float prev_loc_scale = 0;

  for (MatchList::const_iterator feat = matches.begin();
       feat != matches.end();
       ++feat) {
    CvPoint2D32f diff = feat->first - loc_centroid;
    loc_scale += sq_norm(diff);

    diff = feat->second - prev_loc_centroid;
    prev_loc_scale += sq_norm(diff);
  }

  loc_scale *= sqrt(2.0f) / matches.size();
  prev_loc_scale *= sqrt(2.0f) / matches.size();


  float loc_transform[9] = {1.f / loc_scale, 0, -loc_centroid.x / loc_scale,
    0, 1.f / loc_scale,  -loc_centroid.y / loc_scale,
    0, 0, 1 };

  float prev_loc_transform[9] = {1.f / prev_loc_scale, 0, -prev_loc_centroid.x / prev_loc_scale,
    0, 1.f / prev_loc_scale, -prev_loc_centroid.y / prev_loc_scale,
    0, 0, 1 };

  // Compute homography from features. (H * prev_location = location)
  int a_step;

  float* a_mat = new float[2 * matches.size() * 9 * sizeof(float)];
  a_step = 9 * sizeof(float);
  memset(a_mat, 0, a_step * 2 * matches.size());

  int idx = 0;

  for (MatchList::const_iterator feat = matches.begin(); feat != matches.end(); ++feat, idx +=2) {
    float* row_1 = PtrOffset(a_mat, idx * a_step);
    float* row_2 = PtrOffset(a_mat, (idx + 1) * a_step);

    CvPoint2D32f loc = homogPt(transform(homogPt(feat->first), loc_transform)); // feat->first;
    CvPoint2D32f prev_loc = homogPt(transform(homogPt(feat->second), prev_loc_transform));  // feat->second;

    //    CvPoint2D32f loc = feat->first;
    //    CvPoint2D32f prev_loc = feat->second;

    row_1[0] = row_1[1] = row_1[2] = 0.0;

    row_1[3] = -loc.x;
    row_1[4] = -loc.y;
    row_1[5] = -1.0;

    row_1[6] = loc.x * prev_loc.y;
    row_1[7] = loc.y * prev_loc.y;
    row_1[8] = prev_loc.y;

    row_2[0] = loc.x;
    row_2[1] = loc.y;
    row_2[2] = 1.0;

    row_2[3] = row_2[4] = row_2[5] = 0.0;

    row_2[6] = -loc.x * prev_loc.x;
    row_2[7] = -loc.y * prev_loc.x;
    row_2[8] = -prev_loc.x;
  }


  // Calculate SVD of A^T.
  // We need the last column.
  int svd_m = 9;
  int svd_n = 2 * matches.size();
  int lda = a_step / sizeof(float);
  ASSERT_LOG(a_step % sizeof(float) == 0) << "Step must be multiple of sizeof(float)\n";
  float singular_values[9];
  int lwork = (5 * svd_m + svd_n) * 100;
  float* work = new float[lwork];
  int info;

  int nine = 9;

  shared_array<float> U(new float[9 * 9]);

  sgesvd_("A",   // jobu
          "N",   // jobvt
          &svd_m,
          &svd_n,
          a_mat,
          &lda,
          singular_values,
          &U[0],   // u
          &nine,   // ldu
          0,   // vt
          &lda,   // ldvt
          work,
          &lwork,
          &info);

  if (info != 0) {
    std::cerr << "Error in SVD!\n";
  }

  delete [] work;

  float* h = &U[8*9];

  CvMat prev_trans;
  cvInitMatHeader(&prev_trans, 3, 3, CV_32FC1, prev_loc_transform);

  CvMat* prev_trans_inv = cvCreateMat(3, 3, CV_32FC1);
  cvInvert(&prev_trans, prev_trans_inv);

  CvMat trans;
  cvInitMatHeader(&trans, 3, 3, CV_32FC1, loc_transform);

  CvMat homog;
  cvInitMatHeader(&homog, 3, 3, CV_32FC1, h);

  CvMat dest;
  cvInitMatHeader(&dest, 3, 3, CV_32FC1, H);
  CvMat* tmp_mat = cvCreateMat(3, 3, CV_32FC1);

  cvMatMulAdd(&homog, &trans, 0, tmp_mat);
  cvMatMulAdd(prev_trans_inv, tmp_mat, 0, &dest);

  cvReleaseMat(&prev_trans_inv);
  cvReleaseMat(&tmp_mat);

  delete [] a_mat;
}

void EstimateHomographyRobust(const vector<std::pair<CvPoint2D32f, CvPoint2D32f> >& matches,
                   float* H) {

  float curH[9];

  vector<int> best_inliers;
  vector<int> inliers;

  for (int run = 0; run < 20; ++run) {
    // Pick sample.
    vector< std::pair<CvPoint2D32f, CvPoint2D32f> > sample;
    vector<int> sample_idx;
    while (sample_idx.size() < 8) {
      int n = rand() % matches.size();
      if (std::find(sample_idx.begin(), sample_idx.end(), n) == sample_idx.end()) {
        sample_idx.push_back(n);
        sample.push_back(matches[n]);
      }
    }

    EstimateHomography(sample, curH);

    // Get inliers.
    inliers.clear();
    for (int m = 0; m < matches.size(); ++m) {
      if (sq_norm(matches[m].second - homogPt(transform(homogPt(matches[m].first), curH)))
          < 0.1) {
        inliers.push_back(m);
      }
    }

    if (inliers.size() > best_inliers.size())
      best_inliers.swap(inliers);
  }

  // Get best matches.
  vector< std::pair<CvPoint2D32f, CvPoint2D32f> > sample;
  for (int n = 0; n < best_inliers.size(); ++n)
    sample.push_back(matches[best_inliers[n]]);

  std::cout << "Using " << (float)best_inliers.size() / matches.size() << " of all matches.\n";

  EstimateHomography(sample, H);
}

PlayUnit::PlayUnit(const string& vid_stream_name,
                   const string& flow_stream_name,
                   const string& poly_stream_name,
                   VideoWidget* widget)
    : vid_stream_name_(vid_stream_name),
      flow_stream_name_(flow_stream_name),
      polygon_stream_name_(poly_stream_name),
    widget_(widget) {
  play_timer_.reset(new QTimer());
  connect(play_timer_.get(), SIGNAL(timeout()), this, SLOT(PlayLoop()));
  retrack_ = true;
}


void PlayUnit::ResetRect() {
  vector<QPointF> standard;
  standard.push_back(QPointF(frame_width_ / 3, frame_height_ / 3));
  standard.push_back(QPointF(frame_width_ / 3, frame_height_ * 2 / 3));
  standard.push_back(QPointF(frame_width_ * 2 / 3, frame_height_ * 2/ 3));
  standard.push_back(QPointF(frame_width_ * 2 / 3, frame_height_ / 3));

  outlines_[frame_number_ - 1] = standard;
  widget_->SetOutline(outlines_[frame_number_ - 1]);
  widget_->repaint();
}

bool PlayUnit::OpenStreams(StreamSet* set) {
  vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);
  if (vid_stream_idx_ < 0) {
    std::cerr << "PlayUnit: VideoStream not found.\n";
    return false;
  }

  flow_stream_idx_ = FindStreamIdx(flow_stream_name_, set);
  if (flow_stream_idx_ < 0) {
    std::cerr << "PlayUnit: FlowStream not found.\n";
    return false;
  }

  // Get video stream info.
  const VideoFramework::VideoStream* vid_stream =
  dynamic_cast<const VideoFramework::VideoStream*>(set->at(vid_stream_idx_).get());

  ASSURE_LOG(vid_stream);

  frame_width_ = vid_stream->get_frame_width();
  frame_height_ = vid_stream->get_frame_height();
  frame_step_ = vid_stream->get_width_step();

  widget_->SetFrameSize(frame_width_, frame_height_, frame_step_);
  frame_number_ = 0;

  VideoFramework::DataStream* poly_stream = new VideoFramework::DataStream(polygon_stream_name_);
  set->push_back(shared_ptr<VideoFramework::DataStream>(poly_stream));

  return true;
}

void PlayUnit::TogglePlay() {
  if (play_timer_->isActive()) {
    play_timer_->stop();
  } else {
    play_timer_->start(20);
  }
}

void PlayUnit::PlayLoop() {
  if (!RootUnit()->NextFrame())
    play_timer_->stop();
}

void PlayUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {
  // Get optical flow.
  const VideoFramework::OpticalFlowFrame* flow_frame =
  dynamic_cast<const VideoFramework::OpticalFlowFrame*>(input->at(flow_stream_idx_).get());

  typedef VideoFramework::OpticalFlowFrame::TrackedFeature Feature;

  vector<Feature> features;
  if (frame_number_ > 0)
    flow_frame->FillVector(&features);

  const VideoFramework::VideoFrame* frame =
  dynamic_cast<const VideoFramework::VideoFrame*>(input->at(vid_stream_idx_).get());
  ASSERT_LOG(frame);

  ++frame_number_;
  if (frame_number_ > frame_map_.size())
    frame_map_.push_back(frame->get_pts());

  if (frame_number_ == 1) {
    vector<QPointF> standard;
    standard.push_back(QPointF(frame_width_ / 3, frame_height_ / 3));
    standard.push_back(QPointF(frame_width_ / 3, frame_height_ * 2 / 3));
    standard.push_back(QPointF(frame_width_ * 2 / 3, frame_height_ * 2/ 3));
    standard.push_back(QPointF(frame_width_ * 2 / 3, frame_height_ / 3));
    outlines_.push_back(standard);
  } else if (frame_number_ > outlines_.size() || retrack_) {
    // Try to track from prev. frame.
    if (features.size() > 0) {
      // Try to estimate transformation from flow.
      std::cerr << "Features: " << features.size() << "\n";

      vector<CvPoint2D32f> outline(outlines_[frame_number_ - 2].size() + 1);
      for (int o = 0; o < outlines_[frame_number_ - 2].size(); ++o)
        outline[o] = cvPoint2D32f(outlines_[frame_number_ - 2][o].x(),
                                  frame_height_ - 1 - outlines_[frame_number_ - 2][o].y());
      outline.back() = outline[0];
      IplImage tmp_view;
      frame->ImageView(&tmp_view);

      vector<std::pair<CvPoint2D32f, CvPoint2D32f> > matches;

      // First find all feature points within the last polygon.
      for (vector<Feature>::const_iterator f = features.begin();
           f != features.end();
           ++f) {
        CvPoint2D32f prev_pt = f->loc + f->vec;
        CvPoint2D32f test_pt = cvPoint2D32f(prev_pt.x, frame_height_ - 1 - prev_pt.y);
        CvPoint2D32f cur_pt = f->loc;
        if (VideoFramework::WithinConvexPoly(test_pt, outline, 5)) {

          cvLine(&tmp_view, cvPoint(prev_pt.x, prev_pt.y), cvPoint(cur_pt.x, cur_pt.y),
                 cvScalar(255, 0, 0), 0);
          matches.push_back(std::make_pair(prev_pt, cur_pt));
        }
      }

      // Just copy old solution in case no matches are available
      vector<QPointF> trans_outline(outlines_[frame_number_ - 2]);
      if (matches.size() > 10) {
        float H[9] = {1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0};
        EstimateHomographyRobust(matches, H);

        // Transform previous points and insert.
        for (vector<QPointF>::iterator p = trans_outline.begin(); p != trans_outline.end(); ++p) {
          CvPoint3D32f p_trans = cvPoint3D32f(p->x(), p->y(), 1.0);
          p_trans = transform(p_trans, H);
          *p = QPointF(p_trans.x / p_trans.z, p_trans.y / p_trans.z);
        }
      }

      if (outlines_.size() == frame_number_ - 1)
        outlines_.push_back(trans_outline);
      else
        outlines_[frame_number_ - 1] = trans_outline;
    }
  }

  if (frame_number_ == 1) {
    input->push_back(shared_ptr<VideoFramework::PolygonFrame>());
  } else {
    VideoFramework::PolygonFrame* poly_frame = new VideoFramework::PolygonFrame();
    vector<CvPoint2D32f> outline;
    for (vector<QPointF>::const_iterator p = outlines_[frame_number_ - 1].begin();
         p != outlines_[frame_number_ - 1].end();
         ++p)
      outline.push_back(cvPoint2D32f(p->x(), p->y()));

    poly_frame->SetPoints(outline);
    input->push_back(shared_ptr<VideoFramework::PolygonFrame>(poly_frame));
  }

  memcpy(widget_->BackBuffer(), frame->get_data(), frame->get_size());
  widget_->SwapBuffer();
  widget_->SetOutline(outlines_[frame_number_ - 1]);
  widget_->repaint();

   output->push_back(input);
}

void PlayUnit::PostProcess(list<FrameSetPtr>* append) {
  emit(PlayingEnded());
}

bool PlayUnit::SeekImpl(int64_t pts) {
  vector<int64_t>::const_iterator i = std::lower_bound(frame_map_.begin(),
                                                       frame_map_.end(),
                                                       pts);
  if (i != frame_map_.end() && *i >= pts) {
    frame_number_ = i - frame_map_.begin();
    return true;
  } else {
    QMessageBox::critical(0, "", "Seek to unvisited frame! Inconsitency in result!");
    return false;
  }
}

ExportUnit::ExportUnit(const string& poly_file,
                       const string& export_file,
                       const string& vid_stream_name,
                       const vector<vector< QPointF> >& outlines,
                       VideoWidget* widget) :
vid_stream_name_(vid_stream_name),
outlines_(outlines),
widget_(widget) {
  ofs_poly_.open(poly_file.c_str());
  ofs_checker_.open(export_file.c_str());
}

ExportUnit::~ExportUnit() {
  ofs_poly_.close();
  ofs_checker_.close();
}

bool ExportUnit::OpenStreams(StreamSet* set) {
  vid_stream_idx_ = FindStreamIdx(vid_stream_name_, set);
  if (vid_stream_idx_ < 0) {
    std::cerr << "PlayUnit: VideoStream not found.\n";
    return false;
  }

  // Get video stream info.
  const VideoFramework::VideoStream* vid_stream =
  dynamic_cast<const VideoFramework::VideoStream*>(set->at(vid_stream_idx_).get());

  ASSURE_LOG(vid_stream);

  frame_width_ = vid_stream->get_frame_width();
  frame_height_ = vid_stream->get_frame_height();
  frame_step_ = vid_stream->get_width_step();

  mask_img_ = cvCreateImageShared(frame_width_, frame_height_, IPL_DEPTH_8U, 1);
  frame_number_ = 0;
  lines_ = 0;

  cvNamedWindow("awesome-o");

  return true;
}

void ExportUnit::ProcessFrame(FrameSetPtr input, list<FrameSetPtr>* output) {

  const VideoFramework::VideoFrame* frame =
  dynamic_cast<const VideoFramework::VideoFrame*>(input->at(vid_stream_idx_).get());
  ASSERT_LOG(frame);

  ++frame_number_;

  vector<CvPoint2D32f> outline_invert;

  // Output polygon.
  if (frame_number_ > 1)
    ofs_poly_ << "\n";

  for (vector<QPointF>::const_iterator p = outlines_[frame_number_ - 1].begin();
       p != outlines_[frame_number_ - 1].end();
       ++p) {
    if (p != outlines_[frame_number_ - 1].begin())
      ofs_poly_ << " ";
    ofs_poly_ << p->x() << " " << p->y();

    outline_invert.push_back(cvPoint2D32f(p->x(), frame_height_ - 1 - p->y()));
  }
  outline_invert.push_back(outline_invert[0]);

  cvZero(mask_img_.get());

  // Sample checkers.
  float H[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  FixedHomography(outlines_[frame_number_ - 1], H);

  vector< vector<CvPoint2D32f> > checkers;

  // Create checker pattern.
  float sz_x = 1.0f / checker_col_ * checker_size_ / 20.0;
  float sz_y = 1.0f / checker_row_ * checker_size_ / 20.0;

  float border_x = (1.0f - sz_x * checker_col_ - space_x_ * (checker_col_ - 1)) / 2;
  float border_y = (1.0f - sz_y * checker_row_ - space_y_ * (checker_row_ - 1)) / 2;

  for (int i = 0, idx = 0; i < checker_row_; ++i) {
    for (int j = 0; j < checker_col_; ++j, ++idx) {
      vector<CvPoint2D32f> polygon;
      vector<CvPoint> polygon_u;

      polygon.push_back(cvPoint2D32f(border_x + space_x_ * j + sz_x * j,
                                     border_y + space_y_ * i + sz_y * i));

      polygon.push_back(cvPoint2D32f(border_x + space_x_ * j + sz_x * j,
                                     border_y + space_y_ * i + sz_y * (i+1)));


      polygon.push_back(cvPoint2D32f(border_x + space_x_ * j + sz_x * (j+1),
                                     border_y + space_y_ * i + sz_y * (i+1)));

      polygon.push_back(cvPoint2D32f(border_x + space_x_ * j + sz_x * (j+1),
                                     border_y + space_y_ * i + sz_y * i));

      // Transform each pt.
      for (int k = 0; k < 4; ++k) {
        CvPoint2D32f p = homogPt(transform(homogPt(polygon[k]), H));
        polygon[k] = cvPoint2D32f(p.x, frame_height_ - 1 - p.y);
        polygon_u.push_back(cvPoint(p.x + 0.5, p.y + 0.5));
      }

      cvFillConvexPoly(mask_img_.get(), &polygon_u[0], 4, cvScalar(idx*10 + 10));

      polygon.push_back(polygon[0]);
      checkers.push_back(polygon);
    }
  }

  cvShowImage("awesome-o", mask_img_.get());
  cvWaitKey(10);

  IplImage rgb_img;
  frame->ImageView(&rgb_img);

  vector< vector<int> > samples(checkers.size());

  for (int i = 0; i < frame_height_; ++i) {
    uchar* src_ptr = RowPtr<uchar>(&rgb_img, i);
    uchar* mask_ptr = RowPtr<uchar>(mask_img_.get(), i);

    for (int j = 0; j < frame_width_; ++j, src_ptr += 3, ++mask_ptr) {
      // Boundary to test for incidence.
      if (mask_ptr[0]) {
        samples[(mask_ptr[0]-10)/10].push_back(src_ptr[0]);
        samples[(mask_ptr[0]-10)/10].push_back(src_ptr[1]);
        samples[(mask_ptr[0]-10)/10].push_back(src_ptr[2]);
      }
    }
  }

  // Output samples.
  for (int c = 0; c < samples.size(); ++c) {
    if (lines_ > 0)
      ofs_checker_ << "\n";

    ofs_checker_ << ++lines_ << " " << samples[c].size() << " ";

    std::copy(samples[c].begin(), samples[c].end(),
              std::ostream_iterator<int>(ofs_checker_, " "));
  }
  ofs_checker_.flush();

  memcpy(widget_->BackBuffer(), frame->get_data(), frame->get_size());
  widget_->SwapBuffer();
  widget_->SetOutline(outlines_[frame_number_ - 1]);
  widget_->repaint();
  qApp->processEvents();

  output->push_back(input);
}

void ExportUnit::SetCheckerConstraints(int num_row, int num_col, int sz, float space_x,
                                       float space_y) {
  checker_row_ = num_row;
  checker_col_ = num_col;
  checker_size_ = sz;
  space_x_ = space_x;
  space_y_ = space_y;
}


VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent), play_unit_(0) {
  setMinimumSize(640, 480);
  resize(640, 480);

  QPalette pal = palette();
  pal.setColor(backgroundRole(), Qt::black);
  setPalette(pal);
  setAutoFillBackground(true);

  frame_width_ = 640;
  frame_height_ = 480;

  checker_row_ = 4;
  checker_col_ = 6;
  checker_size_ = 10;
  space_x_ = 5 * 0.005;
  space_y_ = 5 * 0.005;
}

void VideoWidget::SetFrameSize(int width, int height, int width_step) {
  frame_width_ = width;
  frame_height_ = height;
  buffered_image_.reset(new BufferedImage(width, height, width_step));
  Rescale();
}

void VideoWidget::resizeEvent(QResizeEvent* re) {
  QWidget::resizeEvent(re);
  Rescale();
}

void VideoWidget::Rescale() {
  float s1 = (float)width() / frame_width_;
  float s2 = (float)height() / frame_height_;

  if (s1 <= s2) {
    scale = s1;
    offset = QPoint(0, (height() - frame_height_ * scale) / 2 );
  } else {
    scale = s2;
    offset = QPoint((width() - frame_width_ * scale) / 2, 0);
  }
}

void VideoWidget::SetOutline(const vector<QPointF>& outline) {
  outline_ = outline;
}

void VideoWidget::CheckerRowNum(int num) {
  checker_row_ = num;
  repaint();
}

void VideoWidget::CheckerColNum(int num) {
  checker_col_ = num;
  repaint();
}

void VideoWidget::CheckerSize(int num) {
  checker_size_ = num;
  repaint();
}

void VideoWidget::CheckerSpaceX(int num) {
  space_x_ = num * 0.005;
  repaint();
}

void VideoWidget::CheckerSpaceY(int num) {
  space_y_ = num * 0.005;
  repaint();
}

void VideoWidget::paintEvent(QPaintEvent* pe) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setRenderHint(QPainter::Antialiasing);

  if (buffered_image_) {
    shared_ptr<QImage> img = buffered_image_->NewFrontView();
    painter.translate(offset.x(), offset.y());
    painter.scale(scale, scale);

    painter.drawImage(QPoint(0,0), *img);

    // Draw outline.
    painter.setPen(QPen(Qt::yellow, 1));
    vector<QPointF> closed_line(outline_);
    closed_line.push_back(closed_line[0]);
    painter.drawPolyline(&closed_line[0], closed_line.size());
    painter.setBrush(QBrush());

    // Draw ellipse handles.
    for (int i = 0; i < outline_.size(); ++i) {
      painter.drawEllipse(outline_[i].x() - 5,
                          outline_[i].y() - 5,
                          10,
                          10);
    }

    // Draw pattern.
    float H[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    FixedHomography(outline_, H);

    float sz_x = 1.0f / checker_col_ * checker_size_ / 20.0;
    float sz_y = 1.0f / checker_row_ * checker_size_ / 20.0;

    float border_x = (1.0f - sz_x * checker_col_ - space_x_ * (checker_col_ - 1)) / 2;
    float border_y = (1.0f - sz_y * checker_row_ - space_y_ * (checker_row_ - 1)) / 2;

    for (int i = 0; i < checker_row_; ++i) {
      for (int j = 0; j < checker_col_; ++j) {
        vector<QPointF> polygon;
        polygon.push_back(QPointF(border_x + space_x_ * j + sz_x * j,
                                  border_y + space_y_ * i + sz_y * i));

        polygon.push_back(QPointF(border_x + space_x_ * j + sz_x * j,
                                  border_y + space_y_ * i + sz_y * (i+1)));


        polygon.push_back(QPointF(border_x + space_x_ * j + sz_x * (j+1),
                                  border_y + space_y_ * i + sz_y * (i+1)));

        polygon.push_back(QPointF(border_x + space_x_ * j + sz_x * (j+1),
                                  border_y + space_y_ * i + sz_y * i));

        // Transform each pt.
        for (int k = 0; k < 4; ++k) {
          CvPoint2D32f p = homogPt(transform(cvPoint3D32f(polygon[k].x(), polygon[k].y(), 1.0), H));
          polygon[k] = QPointF(p.x, p.y);
        }

        painter.setPen(QPen());
        painter.setBrush(QBrush(QColor(255, 255, 255, 100)));
        painter.drawPolygon(&polygon[0], 4);
      }
    }

  }
}

void VideoWidget::mousePressEvent(QMouseEvent* event) {
  // Determine handle.
  pt_selected_ = -1;
  for (int i = 0; i < outline_.size(); ++i) {
    // Convert mouse pt.
    QPointF pos = (QPointF(event->pos()) - offset)  / scale;
    QPointF diff = (pos - outline_[i]);

    if (diff.x() * diff.x() + diff.y() * diff.y() <= 25) {
      pt_selected_ = i;
    }
  }

  setFocus();
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event) {
  if (event->buttons() & Qt::LeftButton) {
    if (pt_selected_ >= 0) {
      // Convert mouse pt.
      QPointF pos = (QPointF(event->pos()) - offset)  / scale;
      outline_[pt_selected_] = pos;

      if (play_unit_)
        play_unit_->UpdateOutline(outline_);

      repaint();
    }
  } else {
    pt_selected_ = -1;
  }

}

MainWindow::MainWindow(const string& video_file,
                       const string& config_file,
                       QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Patch Track");

  QPalette pal = palette();
  pal.setColor(backgroundRole(), Qt::darkGray);
  setPalette(pal);

  QWidget* horiz_sep = new QWidget;
  QHBoxLayout* horiz_layout = new QHBoxLayout;
  horiz_sep->setLayout(horiz_layout);

  QWidget* control_list = new QWidget;
  QVBoxLayout* control_layout = new QVBoxLayout;
  control_list->setLayout(control_layout);

  video_widget_ = new VideoWidget(this);
  horiz_layout->addWidget(video_widget_, 10);
  horiz_layout->addWidget(control_list);

  QPushButton* load_video = new QPushButton("Load video");
  control_layout->addWidget(load_video);
  connect(load_video, SIGNAL(clicked()), this, SLOT(LoadVideo()));

  QPushButton* step_button = new QPushButton("Next Frame");
  control_layout->addWidget(step_button);
  connect(step_button, SIGNAL(clicked()), this, SLOT(StepVideo()));

  QPushButton* reset_button = new QPushButton("Reset Rect");
  control_layout->addWidget(reset_button);
  connect(reset_button, SIGNAL(clicked()), this, SLOT(ResetRect()));

  QPushButton* init_button = new QPushButton("Save Initialization");
  control_layout->addWidget(init_button);
  connect(init_button, SIGNAL(clicked()), this, SLOT(SaveInitialization()));

  QSpinBox* row_spin = new QSpinBox(this);
  row_spin->setValue(4);
  row_spin->setMinimum(2);
  control_layout->addWidget(new QLabel("Row checkers:"));
  control_layout->addWidget(row_spin);
  connect(row_spin, SIGNAL(valueChanged(int)), video_widget_, SLOT(CheckerRowNum(int)));

  QSpinBox* col_spin = new QSpinBox(this);
  col_spin->setValue(6);
  control_layout->addWidget(new QLabel("Column checkers:"));
  control_layout->addWidget(col_spin);
  col_spin->setMinimum(2);
  connect(col_spin, SIGNAL(valueChanged(int)), video_widget_, SLOT(CheckerColNum(int)));

  QSpinBox* size_spin = new QSpinBox(this);
  size_spin->setValue(10);
  control_layout->addWidget(new QLabel("Checker size:"));
  control_layout->addWidget(size_spin);
  size_spin->setMinimum(2);
  connect(size_spin, SIGNAL(valueChanged(int)), video_widget_, SLOT(CheckerSize(int)));

  QSpinBox* space_x_spin = new QSpinBox(this);
  space_x_spin->setValue(5);
  control_layout->addWidget(new QLabel("Checker space_x:"));
  control_layout->addWidget(space_x_spin);
  space_x_spin->setMinimum(0);
  connect(space_x_spin, SIGNAL(valueChanged(int)), video_widget_, SLOT(CheckerSpaceX(int)));

  QSpinBox* space_y_spin = new QSpinBox(this);
  space_y_spin->setValue(5);
  control_layout->addWidget(new QLabel("Checker space_y:"));
  control_layout->addWidget(space_y_spin);
  space_y_spin->setMinimum(0);
  connect(space_y_spin, SIGNAL(valueChanged(int)), video_widget_, SLOT(CheckerSpaceY(int)));

  retrack_ = new QCheckBox("Retrack", this);
  retrack_->setCheckState(Qt::Checked);
  control_layout->addWidget(retrack_);
  connect(retrack_, SIGNAL(stateChanged(int)), this, SLOT(Retrack(int)));

  QPushButton* export_button = new QPushButton("Export", this);
  control_layout->addWidget(export_button);
  connect(export_button, SIGNAL(clicked()), this, SLOT(ExportResult()));

  control_layout->addStretch();

  setCentralWidget(horiz_sep);
  video_widget_->setFocus();

  if (!video_file.empty()) {
    LoadVideoImpl(video_file);
  }

  if(!config_file.empty()) {
    LoadInitialization(config_file);
    if (video_reader_) {
      connect(play_unit_.get(), SIGNAL(PlayingEnded()), this, SLOT(ExportResult()));
      connect(this, SIGNAL(ExportingDone()), qApp, SLOT(quit()));
      play_unit_->TogglePlay();
    }
  }
}


MainWindow::~MainWindow() {

}

void MainWindow::LoadVideoImpl(const string& video_file) {
  video_file_ = video_file;
  video_reader_.reset(new VideoFramework::VideoReaderUnit(video_file_,
                                                          0,
                                                          "VideoStream",
                                                          VideoFramework::PIXEL_FORMAT_RGB24));

  lum_unit_.reset(new VideoFramework::LuminanceUnit());

  flow_unit_.reset(new VideoFramework::OpticalFlowUnit(0.3,     // feature density. = 0.3
                                                       1,       // frames tracked.
                                                       1,
                                                       "LuminanceStream",
                                                       "OpticalFlowStream",
                                                       "PolyStream"));
  lum_unit_->AttachTo(video_reader_.get());
  flow_unit_->AttachTo(lum_unit_.get());

  play_unit_.reset(new PlayUnit("VideoStream", "OpticalFlowStream",
                                "PolyStream", video_widget_));
  play_unit_->AttachTo(flow_unit_.get());
  play_unit_->FeedbackResult(flow_unit_.get(), "PolyStream");

  if (!video_reader_->PrepareProcessing()) {
    QMessageBox::critical(this, "", "Error opening streams.");
    return;
  }

  video_widget_->setFocus();
  video_widget_->SetPlayUnit(play_unit_.get());
  video_reader_->NextFrame();
}

void MainWindow::LoadVideo() {
  QString video_file = QFileDialog::getOpenFileName(this, tr("Load Video"), "");

  if (!video_file.isEmpty()) {
    LoadVideoImpl(video_file.toStdString());
  }
}

void MainWindow::StepVideo() {
  if (!video_reader_->NextFrame()) {
    QMessageBox::information(this, "", "Reached end of video.");
  }
}

void MainWindow::ResetRect() {
  if (play_unit_)
    play_unit_->ResetRect();
}

void MainWindow::Retrack(int state) {
  retrack_->setCheckState(Qt::CheckState(state));
  if (play_unit_)
    play_unit_->EnableRetrack(Qt::CheckState(state) == Qt::Checked);
}

void MainWindow::ExportResult() {
  if (video_reader_) {
    VideoFramework::VideoReaderUnit reader(video_file_,
                                           0,
                                           "VideoStream",
                                           VideoFramework::PIXEL_FORMAT_RGB24);

    // Start export.
    vector<vector<QPointF> > outlines = play_unit_->GetOutlines();
    string base_file = video_file_.substr(0, video_file_.find_last_of("."));
    ExportUnit export_unit(base_file + ".poly",
                           base_file + ".checker",
                           "VideoStream",
                           outlines,
                           video_widget_);

    export_unit.SetCheckerConstraints(video_widget_->get_checker_row(),
                                      video_widget_->get_checker_col(),
                                      video_widget_->get_checker_size(),
                                      video_widget_->get_space_x(),
                                      video_widget_->get_space_y());

    export_unit.AttachTo(&reader);
    reader.PrepareProcessing();

    // Process export.
    while (reader.NextFrame());
    emit(ExportingDone());

    // Reset other units.
    video_reader_->Seek();
    video_reader_->NextFrame();
  }
}

void MainWindow::LoadInitialization(const string& file) {
  std::ifstream ifs(file.c_str());
  ASSURE_LOG(ifs) << "Could not load config file.";

  vector<QPointF> outline(4);
  for (int i = 0; i < 4; ++i) {
    float x, y;
    ifs >> x >> y;
    outline[i].setX(x);
    outline[i].setY(y);
  }

  video_widget_->SetOutline(outline);
  play_unit_->UpdateOutline(outline);

  int row, col, sz;
  float space_x, space_y;
  ifs >> row >> col >> sz >> space_x >> space_y;
  video_widget_->set_checker_row(row);
  video_widget_->set_checker_col(col);
  video_widget_->set_checker_size(sz);
  video_widget_->set_space_x(space_x);
  video_widget_->set_space_y(space_y);
}

void MainWindow::SaveInitialization() {
  string config_file = video_file_.substr(0, video_file_.find_last_of(".")) + ".config";
  std::ofstream ofs(config_file.c_str());
  // Save patch corners.
  const vector<QPointF>& outline = video_widget_->GetOutline();
  ASSURE_LOG(outline.size() == 4);
  for (int i = 0; i < 4; ++i) {
    ofs << outline[i].x() << " " << outline[i].y() << "\n";
  }

  // Save grid settings.
  ofs << video_widget_->get_checker_row() << "\n";
  ofs << video_widget_->get_checker_col() << "\n";
  ofs << video_widget_->get_checker_size() << "\n";
  ofs << video_widget_->get_space_x() << "\n";
  ofs << video_widget_->get_space_y() << "\n";

  ofs.close();

  std::cerr << "Initialization saved.";
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  switch(event->key()) {
    case Qt::Key_Right:
      if (video_reader_)
        StepVideo();
      break;

    case Qt::Key_Left:
      if (video_reader_)
        video_reader_->PrevFrame();
      break;

    case Qt::Key_Space:
      if (video_reader_)
        play_unit_->TogglePlay();
      break;

    case Qt::Key_Up:
      if (video_reader_)
        video_reader_->SkipFrames(10);
      break;

    case Qt::Key_R:
      Retrack(Qt::Checked - retrack_->checkState());
      break;

    case Qt::Key_Down:
      if (video_reader_)
        video_reader_->SkipFrames(-10);
      break;

    default:
      QMainWindow::keyPressEvent(event);
  }
}
