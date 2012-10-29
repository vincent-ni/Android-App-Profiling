#include "tvl1.hpp"

using namespace cv;
using namespace std;


TVL1::TVL1() {
    pfactor = 0.7;    // scale each level by this amount
    max_plevels = MAX_PLEVELS;    // number of pyramid levels
    max_iters = 7;      // u v w update loop
    lambda = 40;      // smoothness constraint
    max_warps = 5;      // warping u v warping
    min_img_sz = 16;    // min max img in pyramid

    // misc
    plevels = max_plevels;
    localWorkSize[0] = BLOCK_SIZE_Y;
    localWorkSize[1] = BLOCK_SIZE_X;
    pr_init_ = 0;
    first_run_ = 1;

    // simple-opencl
    int found;
    sclHard* allHardware;
    found = sclGetAllHardware(&allHardware);
    hardware = sclGetFastestDevice(allHardware, found);
    for (int i = 0; i < NUM_KERNELS; ++i) { comp_flag[i] = false; }
}


TVL1::~TVL1() {
    if (pr_init_) { cleanup(); }
}


void TVL1::cleanup() {
    if (!pr_init_) { return; }
    for (int i = 0; i < plevels; ++i) {
        sclReleaseMemObject(pyr_I1[i]);
        sclReleaseMemObject(pyr_I2[i]);
        sclReleaseMemObject(pyr_U[i]);
        sclReleaseMemObject(pyr_V[i]);
        sclReleaseMemObject(pyr_W[i]);
    }
    pr_init_ = 0;
    sclReleaseMemObject(m_dt);
    sclReleaseMemObject(m_dx);
    sclReleaseMemObject(m_dy);
    sclReleaseMemObject(m_sq);
    sclReleaseMemObject(m_u0);
    sclReleaseMemObject(m_v0);
    sclReleaseMemObject(m_u_);
    sclReleaseMemObject(m_v_);
    sclReleaseMemObject(m_w_);
    for (int ndv = 0; ndv < NUM_DUALVARS; ++ndv) {
        sclReleaseMemObject(m_p[ndv]);
        sclReleaseMemObject(m_p_[ndv]);
    }
}


void TVL1::display_flow(const Mat& u, const Mat& v) {
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
    imshow("optical flow", bgr);
}


void TVL1::gen_pyramid_sizes(int M, int N) {
    float sM = M;
    float sN = N;
    // store resizing
    for (int level = 0; level <= plevels; ++level) {
        if (level == 0) {
        } else {
            sM *= pfactor;
            sN *= pfactor;
        }
        pyr_M[level] = (int)(sM + 0.5f);
        pyr_N[level] = (int)(sN + 0.5f);
        printf(" pyr %d: %d x %d \n", level, (int)sM, (int)sN);
        if (sM < min_img_sz || sN < min_img_sz) { plevels = level; break; }
    }
}


void TVL1::create_pyramids(float* im1, float* im2, int M0, int N0) {

    size_t fargsize = sizeof(float);
    size_t iargsize = sizeof(int);

    // resize kernel init
    size_t mem_size;
    if (!comp_flag[0]) {
        comp_flag[0] = true;
        software[0] = sclGetCLSoftware("kernels.cl", "resize", hardware);
    }

    if (!pr_init_) {
        pr_init_ = 1;
        // list of h,w
        gen_pyramid_sizes(M0, N0);
        // pyramids mem
        for (int level = 0; level < plevels; ++level) {
            mem_size = fargsize * pyr_M[level] * pyr_N[level];
            pyr_I1[level] = sclMalloc(hardware, CL_MEM_READ_ONLY,  mem_size);
            pyr_I2[level] = sclMalloc(hardware, CL_MEM_READ_ONLY,  mem_size);
            pyr_U [level] = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
            pyr_V [level] = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
            pyr_W [level] = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        }
        // misc core mem
        mem_size = fargsize * pyr_M[0] * pyr_N[0]; // full size, re-use
        m_dt = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_dx = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_dy = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_sq = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_u0 = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_v0 = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_u_ = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_v_ = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        m_w_ = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        // dual var mem
        for (int ndv = 0; ndv < NUM_DUALVARS; ++ndv) {
            m_p_[ndv] = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
            m_p [ndv] = sclMalloc(hardware, CL_MEM_READ_WRITE, mem_size);
        }
    }


    // pyramids fill
    for (int level = 0; level < plevels; ++level) {
        size_t globalWorkSize[] = {
            ((pyr_M[level] - 1) / localWorkSize[0] + 1)* localWorkSize[0],
            ((pyr_N[level] - 1) / localWorkSize[1] + 1)* localWorkSize[1]
        };
        if (level == 0) {
            mem_size = fargsize * pyr_M[level] * pyr_N[level];
            // pyr_I1
            sclWrite(hardware, mem_size, pyr_I1[level], im1);
            // pyr_I2
            sclWrite(hardware, mem_size, pyr_I2[level], im2);


            {
                // bilateral filter
                if (!comp_flag[10]) {
                    comp_flag[10] = true;
                    software[10] = sclGetCLSoftware("kernels.cl", "bilateral", hardware);
                }
                int radius = 2; // (2*r+1)^2
                float cdiff = 0.2; // 0-255 normalized
                // i1
                sclSetKernelArgs(software[10], " %v %v %a %a %a %a ",
                                 &pyr_I1[level], &pyr_I1[level],
                                 iargsize, &radius, fargsize, &cdiff,
                                 iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
                sclLaunchKernel(hardware, software[10], globalWorkSize, localWorkSize);
                // i2
                sclSetKernelArgs(software[10], " %v %v %a %a %a %a ",
                                 &pyr_I2[level], &pyr_I2[level],
                                 iargsize, &radius, fargsize, &cdiff,
                                 iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
                sclLaunchKernel(hardware, software[10], globalWorkSize, localWorkSize);
#if 0
                cl_mem d_img = pyr_I2[level];
                int rows = pyr_M[level]; int cols = pyr_N[level];
                size_t mem_size = sizeof(float) * rows * cols;
                Mat disp = Mat::zeros(rows, cols, CV_32FC1);
                float* h_img = (float*)malloc(mem_size);
                sclRead(hardware, mem_size, d_img, h_img);
                FloatToMat(h_img, disp);
                if (0) { disp.convertTo(disp, CV_8UC1); }
                imshow("cl test", disp);
                free(h_img);
#endif
            }



        } else {
            // pyr_I1
            sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                             &pyr_I1[level - 1], iargsize, &pyr_M[level - 1], iargsize, &pyr_N[level - 1],
                             &pyr_I1[level    ], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
            sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
            // pyr_I2
            sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                             &pyr_I2[level - 1], iargsize, &pyr_M[level - 1], iargsize, &pyr_N[level - 1],
                             &pyr_I2[level    ], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
            sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
        }
    }

}


void TVL1::tv_l1_dual(cl_mem* p, int level) {

    // vars
    float L = sqrtf(8.0f);
    float tau = 1 / L;
    float sigma = 1 / L;
    float gamma = 0.02; // beta?

    // sizes
    size_t fargsize = sizeof(float);
    size_t iargsize = sizeof(int);
    size_t globalWorkSize[] = {
        ((pyr_M[level] - 1) / localWorkSize[0] + 1)* localWorkSize[0],
        ((pyr_N[level] - 1) / localWorkSize[1] + 1)* localWorkSize[1]
    };

    // warp
    for (int j = 0; j < max_warps; ++j) {

#if 1
        // warping
        if (!comp_flag[3]) {
            comp_flag[3] = true;
            software[3] = sclGetCLSoftware("kernels.cl", "warping", hardware);
        }
        sclSetKernelArgs(software[3], " %v %v %v %v %v %v %v %a %a ",
                         &pyr_I1[level],  &pyr_I2[level],
                         &pyr_U [level],  &pyr_V [level],
                         &m_dx, &m_dy, &m_dt,
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[3], globalWorkSize, localWorkSize);
#else
        // warping
        if (!comp_flag[8]) {
            comp_flag[8] = true;
            software[8] = sclGetCLSoftware("kernels.cl", "w_interp_dt", hardware);
        }
        // dt
        sclSetKernelArgs(software[8], " %v %v %v %v %v %a %a ",
                         &pyr_I1[level],  &pyr_I2[level],
                         &pyr_U [level],  &pyr_V [level],
                         &m_dt,
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[8], globalWorkSize, localWorkSize);
        // warping
        if (!comp_flag[9]) {
            comp_flag[9] = true;
            software[9] = sclGetCLSoftware("kernels.cl", "w_interp_dxdy", hardware);
        }
        // dx dy
        sclSetKernelArgs(software[9], " %v %v %v %v %v %a %a ",
                         &pyr_I2[level],
                         &pyr_U [level],  &pyr_V [level],
                         &m_dx, &m_dy,
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[9], globalWorkSize, localWorkSize);
#endif
        // gradient squared
        if (!comp_flag[4]) {
            comp_flag[4] = true;
            software[4] = sclGetCLSoftware("kernels.cl", "magnitude", hardware);
        }
        sclSetKernelArgs(software[4], " %v %v %v %a %a %a ",
                         &m_sq, &m_dx, &m_dy, sizeof(float), &gamma,
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[4], globalWorkSize, localWorkSize);

        // save current
        if (!comp_flag[2]) {
            comp_flag[2] = true;
            software[2] = sclGetCLSoftware("kernels.cl", "copy", hardware);
        }
        // u0
        sclSetKernelArgs(software[2], " %v %v %a %a ",
                         &pyr_U[level], &m_u0, /* src, dst */
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);
        // v0
        sclSetKernelArgs(software[2], " %v %v %a %a ",
                         &pyr_V[level], &m_v0, /* src, dst */
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);
        // u_
        sclSetKernelArgs(software[2], " %v %v %a %a ",
                         &pyr_U[level], &m_u_, /* src, dst */
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);
        // v_
        sclSetKernelArgs(software[2], " %v %v %a %a ",
                         &pyr_V[level], &m_v_, /* src, dst */
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);
        // w_
        sclSetKernelArgs(software[2], " %v %v %a %a ",
                         &pyr_W[level], &m_w_, /* src, dst */
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);

        // inner loop updates
        if (!comp_flag[5]) {
            comp_flag[5] = true;
            software[5] = sclGetCLSoftware("kernels.cl", "updates", hardware);
        }
        sclSetKernelArgs(software[5],
                         " %v %v %v %v %v %v %v %v %v %v %v %v %v %v %v %v %v %v %a %a %a %a %a %a ",
                         &m_u0, &m_v0, &m_u_, &m_v_, &m_w_,
                         &pyr_U[level], &pyr_V[level], &pyr_W[level],
                         &p[0], &p[1], &p[2], &p[3], &p[4], &p[5],
                         &m_dx, &m_dy, &m_dt, &m_sq,
                         fargsize, &sigma, fargsize, &tau, fargsize, &lambda, fargsize, &gamma,
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        for (int k = 0; k < max_iters; ++k) {
            sclLaunchKernel(hardware, software[5], globalWorkSize, localWorkSize);
        }

        // median filter
        if (!comp_flag[6]) {
            comp_flag[6] = true;
            software[6] = sclGetCLSoftware("kernels.cl", "median3x3", hardware);
        }
        // u
        sclSetKernelArgs(software[6], " %v %v %a %a ",
                         &pyr_U[level], &pyr_U[level],
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[6], globalWorkSize, localWorkSize);
        // v
        sclSetKernelArgs(software[6], " %v %v %a %a ",
                         &pyr_V[level], &pyr_V[level],
                         iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
        sclLaunchKernel(hardware, software[6], globalWorkSize, localWorkSize);

    } // warp

}


void TVL1::process_pyramids(float* ou, float* ov) {

    size_t fargsize = sizeof(float);
    size_t iargsize = sizeof(int);
    size_t mem_size;

    // pyramid loop
    for (int level = plevels - 1; level >= 0; level--) {

        mem_size = fargsize * pyr_M[level] * pyr_N[level];
        size_t globalWorkSize[] = {
            ((pyr_M[level] - 1) / localWorkSize[0] + 1)* localWorkSize[0],
            ((pyr_N[level] - 1) / localWorkSize[1] + 1)* localWorkSize[1]
        };

        if (level == plevels - 1) {
            // zeros
            if (!comp_flag[1]) {
                comp_flag[1] = true;
                software[1] = sclGetCLSoftware("kernels.cl", "zeros", hardware);
            }
            sclSetKernelArg(software[1], 1, iargsize,    &pyr_M[level]);
            sclSetKernelArg(software[1], 2, iargsize,    &pyr_N[level]);
            // u
            sclSetKernelArg(software[1], 0, sizeof(cl_mem), &pyr_U[level]);
            sclLaunchKernel(hardware, software[1], globalWorkSize, localWorkSize);
            // v
            sclSetKernelArg(software[1], 0, sizeof(cl_mem), &pyr_V[level]);
            sclLaunchKernel(hardware, software[1], globalWorkSize, localWorkSize);
            // w
            sclSetKernelArg(software[1], 0, sizeof(cl_mem), &pyr_W[level]);
            sclLaunchKernel(hardware, software[1], globalWorkSize, localWorkSize);
            // p
            for (int ndv = 0; ndv < NUM_DUALVARS; ++ndv) {
                sclSetKernelArg(software[1], 0, sizeof(cl_mem), &m_p[ndv]);
                sclLaunchKernel(hardware, software[1], globalWorkSize, localWorkSize);
            }
        } else {
            // propagate
            if (!comp_flag[0]) {
                comp_flag[0] = true;
                software[0] = sclGetCLSoftware("kernels.cl", "resize", hardware);
            }
            // u
            sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                             &pyr_U[level + 1], iargsize, &pyr_M[level + 1], iargsize, &pyr_N[level + 1],
                             &pyr_U[level    ], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
            sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
            // v
            sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                             &pyr_V[level + 1], iargsize, &pyr_M[level + 1], iargsize, &pyr_N[level + 1],
                             &pyr_V[level    ], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
            sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
            // w
            sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                             &pyr_W[level + 1], iargsize, &pyr_M[level + 1], iargsize, &pyr_N[level + 1],
                             &pyr_W[level    ], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
            sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
            // p --> p_
            for (int ndv = 0; ndv < NUM_DUALVARS; ndv++) {
                sclSetKernelArgs(software[0], " %v %a %a %v %a %a ",
                                 &m_p [ndv], iargsize, &pyr_M[level + 1], iargsize, &pyr_N[level + 1],
                                 &m_p_[ndv], iargsize, &pyr_M[level    ], iargsize, &pyr_N[level    ]);
                sclLaunchKernel(hardware, software[0], globalWorkSize, localWorkSize);
            }

            // pix *= scalar
            if (!comp_flag[7]) {
                comp_flag[7] = true;
                software[7] = sclGetCLSoftware("kernels.cl", "mul_scalar", hardware);
            }
            // u *= s
            float rescale_u = pyr_N[level + 1] / (float)pyr_N[level];
            sclSetKernelArgs(software[7], " %v %a %a %a ",
                             &pyr_U[level], fargsize, &rescale_u,
                             iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
            sclLaunchKernel(hardware, software[7], globalWorkSize, localWorkSize);
            // v *= s
            float rescale_v = pyr_M[level + 1] / (float)pyr_M[level];
            sclSetKernelArgs(software[7], " %v %a %a %a ",
                             &pyr_V[level], fargsize, &rescale_v,
                             iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
            sclLaunchKernel(hardware, software[7], globalWorkSize, localWorkSize);

            // p <-- p_
            if (!comp_flag[2]) {
                comp_flag[2] = true;
                software[2] = sclGetCLSoftware("kernels.cl", "copy", hardware);
            }
            for (int ndv = 0; ndv < NUM_DUALVARS; ++ndv) {
                sclSetKernelArgs(software[2], " %v %v %a %a ",
                                 &m_p_[ndv], &m_p[ndv],
                                 iargsize, &pyr_M[level], iargsize, &pyr_N[level]);
                sclLaunchKernel(hardware, software[2], globalWorkSize, localWorkSize);
            }
        }

        // ===== core ====== //
        tv_l1_dual(m_p, level);
        // ===== ==== ====== //
    }

    // output
    mem_size = fargsize * pyr_M[0] * pyr_N[0];
    sclRead(hardware, mem_size, pyr_U[0], ou);
    sclRead(hardware, mem_size, pyr_V[0], ov);

}


void TVL1::optical_flow_tvl1(Mat& m_I0, Mat& m_I1, Mat& m_Iu, Mat& m_Iv) {

    // extract cv image
    Mat mgray1(m_I0.rows, m_I0.cols, CV_8UC1);
    cvtColor(m_I0, mgray1, CV_BGR2GRAY);
    mgray1.convertTo(mgray1, CV_32FC1);
    //GaussianBlur(mgray1, mgray1, Size(), 1, 1);
    //bilateralFilter(mgray1.clone(), mgray1, 0, 15, 2);
    mgray1 /= 255.0f;//                                     imshow("bilat1", mgray1);
    float* h_img1 = (float*)mgray1.data;

    // extract cv image
    Mat mgray2(m_I1.rows, m_I1.cols, CV_8UC1);
    cvtColor(m_I1, mgray2, CV_BGR2GRAY);
    mgray2.convertTo(mgray2, CV_32FC1);
    //GaussianBlur(mgray2, mgray2, Size(), 1, 1);
    //bilateralFilter(mgray2.clone(), mgray2, 0, 15, 2);
    mgray2 /= 255.0f;//                                     imshow("bilat2",mgray2);
    float* h_img2 = (float*)mgray2.data;

    // sizes
    unsigned int numel = m_I0.cols * m_I0.rows;
    size_t mem_numel = sizeof(float) * numel;
    int width = m_I0.cols, height = m_I0.rows;

    // allocate host memory for result
    float* h_u = (float*) malloc(mem_numel);
    float* h_v = (float*) malloc(mem_numel);

#if TIMING
    // runs
    int nruns = 6;
    // warmup
    create_pyramids(h_img1, h_img2, height, width);
    process_pyramids(h_u, h_v);
    // timing
    start_timer(0);
    for (int i = 0; i < nruns; ++i) {
        create_pyramids(h_img1, h_img2, height, width);
        process_pyramids(h_u, h_v);
    }
    printf("fps: %f \n", 1.0f / (elapsed_time(0) / 1000.0f / (float)nruns));
#else
    // timing
    start_timer(0);
    // get pyramids
    create_pyramids(h_img1, h_img2, height, width);
    // get flow
    process_pyramids(h_u, h_v);
    // timing
    printf("fps: %f \n", 1.0f / (elapsed_time(0) / 1000.0f));
#endif

    // output
    FloatToMat(h_u, m_Iu);
    FloatToMat(h_v, m_Iv);

    // cleanup
    free(h_u);
    free(h_v);

}



void TVL1::MatToFloat(const Mat& thing, float* thing2) {
    int tmp = 0;
    for (int i = 0; i < thing.rows; ++i) {
        const float* fptr = thing.ptr<float>(i);
        for (int j = 0; j < thing.cols; ++j)
            { thing2[tmp++] = fptr[j]; }
    }
}

void TVL1::FloatToMat(float const* thing, Mat& thing2) {
    int tmp = 0;
    for (int i = 0; i < thing2.rows; ++i) {
        float* fptr = thing2.ptr<float>(i);
        for (int j = 0; j < thing2.cols; ++j)
            { fptr[j] = thing[tmp++]; }
    }
}



// #include "imageLib.h"
// #include "flowIO.h"
void TVL1::writeFlo(Mat& oflow_u, Mat& oflow_v) {
//     CFloatImage img(oflow_u.cols, oflow_u.rows, 2);
//     for (int i = 0; i < oflow_u.rows; i++) {
//         float* fptr_u = oflow_u.ptr<float>(i);
//         float* fptr_v = oflow_v.ptr<float>(i);
//         for (int j = 0; j < oflow_u.cols; j++) {
//             img.Pixel(j, i, 0) = fptr_u[j];
//             img.Pixel(j, i, 1) = fptr_v[j];
//         }
//     }
//     WriteFlowFile(img, "tvl1-flow.flo");
}

