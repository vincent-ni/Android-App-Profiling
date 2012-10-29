#include <QtGui/QApplication>
#include "MainWindow.h"

#include "ui_mainWindow.h"

#include "tvl1.hpp"
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

using namespace std;
using namespace cv;

int cv_init_ = 0;
MainWindow* mainWindow_ptr;

int grab_frame(VideoCapture& capture, Mat& img, char* filename) {

  // camera/image setup
  if (!cv_init_) {
    if (filename != NULL) {
      capture.open(filename);
    } else {
      float scale = 0.52;//0.606;
      int w = 640 * scale;
      int h = 480 * scale;
      capture.open(0); //try to open
      capture.set(CV_CAP_PROP_FRAME_WIDTH, w);  capture.set(CV_CAP_PROP_FRAME_HEIGHT, h);
    }
    if (!capture.isOpened()) { cerr << "open video device fail\n" << endl; return 0; }
    capture.grab();
    capture.retrieve(img);
    if (img.empty()) { cout << "load image fail " << endl; return 0; }
    printf(" img = %d x %d \n", img.rows, img.cols);
    cv_init_ = 1;
  }

  // get frames
  capture.grab();
  capture.retrieve(img);

  if (waitKey(10) >= 0) { return 0; }
  else { return 1; }
}

void imshow_qt(int wnd, Mat& disp) {
  const uchar* qImageBuffer = (const uchar*)disp.data;
  QImage img(qImageBuffer, disp.cols, disp.rows, disp.step, QImage::Format_RGB888);
  QImage frame = img.rgbSwapped();
  switch (wnd) {
  case 1:
    mainWindow_ptr->ui->frameLabel1->setPixmap(QPixmap::fromImage(frame));
    break;
  case 2:
    mainWindow_ptr->ui->frameLabel2->setPixmap(QPixmap::fromImage(frame));
    break;
  }
  QApplication::processEvents();
}

void display_flow_qt(const Mat& u, const Mat& v) {
  Mat hsv_image(u.rows, u.cols, CV_8UC3);
  for (int i = 0; i < u.rows; ++i) {
    const float* x_ptr = u.ptr<float>(i);
    const float* y_ptr = v.ptr<float>(i);
    uchar* hsv_ptr = hsv_image.ptr<uchar>(i);
    for (int j = 0; j < u.cols; ++j, hsv_ptr += 3, ++x_ptr, ++y_ptr) {
      hsv_ptr[0] = (uchar)((atan2f(*y_ptr, *x_ptr) / M_PI + 1) * 90);
      hsv_ptr[1] = hsv_ptr[2] = (uchar) std::min<float>(
                                  sqrtf(*y_ptr * *y_ptr + *x_ptr * *x_ptr) * 20.0f, 255.0f);
    }
  }
  Mat bgr;
  cvtColor(hsv_image, bgr, CV_HSV2BGR);
  imshow_qt(2, bgr);
}


int main(int argc, char* argv[]) {
  // opencv
  VideoCapture capture;
  Mat cam_img, prev_img, disp_u, disp_v;
  int is_images = 0;

  // video file, images or usb camera
  if (argc == 2) { grab_frame(capture, prev_img, argv[1]); } // video
  else if (argc == 3) {
    prev_img = imread(argv[1]); cam_img = imread(argv[2]);
    is_images = 1;
  } else { grab_frame(capture, prev_img, NULL); } // usb camera

  // flow
  TVL1 tvl1 = TVL1();

  // opencv
  int mm = prev_img.rows;  int nn = prev_img.cols;
  disp_u = Mat::zeros(mm, nn, CV_32FC1);
  disp_v = Mat::zeros(mm, nn, CV_32FC1);
  printf("img %d x %d \n", mm, nn);

  // qt
  QApplication app(argc, argv);
  MainWindow mainWindow;
  mainWindow_ptr = &mainWindow;
  mainWindow.show();

  // process main
  if (is_images) {
    // show
    imshow_qt(1, cam_img);
    // process files
    tvl1.optical_flow_tvl1(prev_img, cam_img, disp_u, disp_v);
    // show
    display_flow_qt(disp_u, disp_v);
    imshow("cam", cam_img); waitKey(0); // pause
    // write
    tvl1.writeFlo(disp_u, disp_v);
  } else {
    // process loop
    while (grab_frame(capture, cam_img, NULL)) {
      // show
      imshow_qt(1, cam_img);
      // process
      tvl1.optical_flow_tvl1(prev_img, cam_img, disp_u, disp_v);
      // frames
      prev_img = cam_img.clone();
      // show
      display_flow_qt(disp_u, disp_v);
    }
  }

  return 0;
}
