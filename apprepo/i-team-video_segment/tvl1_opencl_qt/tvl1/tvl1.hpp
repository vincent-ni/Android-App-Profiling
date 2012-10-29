#ifndef TVL1_H_
#define TVL1_H_

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "simpleCL.h"
#include "timer.hpp"

#include <cv.h>
#include <cxcore.h>
#include <highgui.h>

using namespace std;
using namespace cv;

#define BLOCK_SIZE_X  64
#define BLOCK_SIZE_Y   1
#define TIMING         0
#define MAX_PLEVELS   15
#define NUM_KERNELS   12
#define NUM_DUALVARS   6

class TVL1 {

private:
    // control
    float pfactor;    // scale each level by this amount
    int max_plevels;    // number of pyramid levels
    int max_iters;      // u v w update loop
    float lambda;      // smoothness constraint
    int max_warps;      // warping u v warping
    int min_img_sz;    // min mxn img in pyramid

    // misc
    int plevels;
    sclHard hardware;
    size_t localWorkSize[2];
    int pr_init_;
    int first_run_;
    int pyr_M[MAX_PLEVELS + 1];
    int pyr_N[MAX_PLEVELS + 1];

    // mem
    cl_mem pyr_I1[MAX_PLEVELS];
    cl_mem pyr_I2[MAX_PLEVELS];
    cl_mem pyr_U[MAX_PLEVELS];
    cl_mem pyr_V[MAX_PLEVELS];
    cl_mem pyr_W[MAX_PLEVELS];
    cl_mem m_dt;
    cl_mem m_dx;
    cl_mem m_dy;
    cl_mem m_sq;
    cl_mem m_u0;
    cl_mem m_v0;
    cl_mem m_u_;
    cl_mem m_v_;
    cl_mem m_w_;
    cl_mem m_p [NUM_DUALVARS];
    cl_mem m_p_[NUM_DUALVARS];

    // kernels
    sclSoft software[NUM_KERNELS];
    bool comp_flag[NUM_KERNELS];

    // functions
    void gen_pyramid_sizes(int M, int N);
    void create_pyramids(float* im1, float* im2, int M0, int N0);
    void process_pyramids(float* ou, float* ov);
    void tv_l1_dual(cl_mem* p, int level);
    void MatToFloat(const Mat& thing, float* thing2);
    void FloatToMat(float const* thing, Mat& thing2);


public:
    TVL1();
    virtual ~TVL1();
    void cleanup();
    void optical_flow_tvl1(Mat& m_I0, Mat& m_I1, Mat& m_Iu, Mat& m_Iv);
    void display_flow(const Mat& u, const Mat& v);
    void writeFlo(Mat& oflow_u, Mat& oflow_v);

};

#endif /*TVL1_H_*/
