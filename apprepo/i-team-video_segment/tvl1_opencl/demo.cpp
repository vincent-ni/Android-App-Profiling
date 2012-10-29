//
// Chris McClanahan & Brian Hrolenok - 2011
//
// Adapted from: http://gpu4vision.icg.tugraz.at/index.php?content=downloads.php
//   "An Improved Algorithm for TV-L1 Optical Flow"
//
//  DEMO
//

#include "tvl1.hpp"
#include <opencv/cv.h>
#include <opencv/cxcore.h>
#include <opencv/highgui.h>

using namespace std;
using namespace cv;

int cv_init_ = 0;

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
    imshow("cam", img);

    if (waitKey(10) >= 0) { return 0; }
    else { return 1; }
}


int main(int argc, char** argv) {

    VideoCapture capture;
    // video file or usb camera
    Mat cam_img, prev_img, disp_u, disp_v;
    int is_images = 0;
    if (argc == 2) { grab_frame(capture, prev_img, argv[1]); } // video
    else if (argc == 3) {
        prev_img = imread(argv[1]); cam_img = imread(argv[2]);
        is_images = 1;
    } else { grab_frame(capture, prev_img, NULL); } // usb camera
    TVL1 tvl1 = TVL1();
    // results
    int mm = prev_img.rows;  int nn = prev_img.cols;
    disp_u = Mat::zeros(mm, nn, CV_32FC1);
    disp_v = Mat::zeros(mm, nn, CV_32FC1);
    printf("img %d x %d \n", mm, nn);

    // process main
    if (is_images) {
        // show
        imshow("i", cam_img);
        // process files
        tvl1.optical_flow_tvl1(prev_img, cam_img, disp_u, disp_v);
        // show
        tvl1.display_flow(disp_u, disp_v);
        waitKey(0);
    } else {
        // process loop
        while (grab_frame(capture, cam_img, NULL)) {
            // process
            tvl1.optical_flow_tvl1(prev_img, cam_img, disp_u, disp_v);
            // frames
            prev_img = cam_img.clone();
            // show
            tvl1.display_flow(disp_u, disp_v);
        }
    }

    // cleanup
    tvl1.cleanup();
    return 0;
}
