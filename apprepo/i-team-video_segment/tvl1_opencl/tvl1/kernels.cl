#define MIN(a, b) (a <= b ? a : b)
#define MAX(a, b) (a >= b ? a : b)
#define IMG2D(img, x, y, width) (img[(int)(y*width+x)])
#define INTERP_MODE_WARP   1  // 1 bilinear, 0 bicubic | matlab=bicubic
#define INTERP_MODE_RESIZE 1  // 1 bilinear, 0 bicubic | matlab=bilinear

//////////////////////////////////////////////////////////////////////////////////////
// { "resize", "zeros", "copy", "warping", "magnitude", "updates", "median3x3",
//       0        1        2        3           4           5          6
//   "mul_scalar", "w_interp_dt", "w_interp_dxdy", "bilateral" }
//        7              8               9            10
//////////////////////////////////////////////////////////////////////////////////////


// bicubic interpolation reference:
//  http://www.naic.edu/~phil/hardware/nvidia/doc/src/bicubicTexture/

// w0, w1, w2, and w3 are the four cubic B-spline basis functions
float w0(const float a) { return (1.0f / 6.0f) * (a * (a * (-a + 3.0f) - 3.0f) + 1.0f); }
float w1(const float a) { return (1.0f / 6.0f) * (a * a * (3.0f * a - 6.0f) + 4.0f); }
float w2(const float a) { return (1.0f / 6.0f) * (a * (a * (-3.0f * a + 3.0f) + 3.0f) + 1.0f); }
float w3(const float a) { return (1.0f / 6.0f) * (a * a * a); }

// g0 and g1 are the two amplitude functions
float g0(const float a) { return w0(a) + w1(a); }
float g1(const float a) { return w2(a) + w3(a); }

// h0 and h1 are the two offset functions
float h0(const float a) { return -1.0f + w1(a) / (w0(a) + w1(a)) + 0.5f; }
float h1(const float a) { return  1.0f + w3(a) / (w2(a) + w3(a)) + 0.5f; }

// filter 4 values using cubic splines
float cubicFilter(float x, float c0, float c1, float c2, float c3) {
    float r;
    r =  c0 * w0(x); r += c1 * w1(x);
    r += c2 * w2(x); r += c3 * w3(x);
    return r;
}

// bicubic using 16 lookups, slow
float bicubic_interp(__global const float* img, float y, float x, const int height, const int width) {
    x = x - 0.5f >= 0 ? x - 0.5f : x;
    y = y - 0.5f >= 0 ? y - 0.5f : y;
    const float px = floor(x);
    const float py = floor(y);
    const float fx = x - px;
    const float fy = y - py;

    const float yU  = py + 1 <  height ? py + 1 : y;
    const float yD  = py - 1 >= 0      ? py - 1 : y;
    const float xR  = px + 1 <  width  ? px + 1 : x;
    const float xL  = px - 1 >= 0      ? px - 1 : x;
    const float yUU = yU + 1 <  height ? yU + 1 : yD;
    const float xRR = xR + 1 <  width  ? xR + 1 : xR;

    return cubicFilter(fy,
                       cubicFilter(fx,
                                   IMG2D(img, xL, yD, width), IMG2D(img,  px, yD, width),
                                   IMG2D(img, xR, yD, width), IMG2D(img, xRR, yD, width)),
                       cubicFilter(fx,
                                   IMG2D(img, xL, py, width), IMG2D(img,  px, py, width),
                                   IMG2D(img, xR, py, width), IMG2D(img, xRR, py, width)),
                       cubicFilter(fx,
                                   IMG2D(img, xL, yU, width), IMG2D(img,  px, yU, width),
                                   IMG2D(img, xR, yU, width), IMG2D(img, xRR, yU, width)),
                       cubicFilter(fx,
                                   IMG2D(img, xL, yUU, width), IMG2D(img,  px, yUU, width),
                                   IMG2D(img, xR, yUU, width), IMG2D(img, xRR, yUU, width))
                      );
}


// bilinear resampling
float bilinear_interp(__global const float* img, const int yy, const int xx, const int height, const int width) {
    const int id  = yy * width + xx;

    const int xR = xx + 1 <  width  ? xx + 1 : xx;
    const int xL = xx - 1 >= 0      ? xx - 1 : xx;
    const int yU = yy + 1 <  height ? yy + 1 : yy;
    const int yD = yy - 1 >= 0      ? yy - 1 : yy;

    const float nw = img[yU * width + xL];
    const float nn = img[yU * width + xx];
    const float ne = img[yU * width + xR];

    const float ww = img[yy * width + xL];
    const float cc = img[id             ];
    const float ee = img[yy * width + xR];

    const float sw = img[yD * width + xL];
    const float ss = img[yD * width + xx];
    const float se = img[yD * width + xR];

    return
        nw * 0.09f + nn * 0.12f + ne * 0.09f +
        ww * 0.12f + cc * 0.16f + ee * 0.12f +
        sw * 0.09f + ss * 0.12f + se * 0.09f ;
}




//  image rescaling
__kernel void
resize(__global const float* d_I, const int ny_I, const int nx_I,
       __global float* d_O, const int ny_O, const int nx_O) {

    const int yO = get_global_id(0);
    const int xO = get_global_id(1);
    const int yI = (int)(yO * (float)ny_I / ny_O);
    const int xI = (int)(xO * (float)nx_I / nx_O);

    if (xO >= nx_O || yO >= ny_O) { return; }
    if (xI >= nx_I || yI >= ny_I) { return; }

#if INTERP_MODE_RESIZE
    d_O[xO + yO * nx_O] = bilinear_interp(d_I, yI, xI, ny_I, nx_I);
#else
    d_O[xO + yO * nx_O] = bicubic_interp(d_I, yI, xI, ny_I, nx_I);
#endif
}


// fill with zeros
__kernel void
zeros(__global float* out, const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    out[gid] = 0;
}


// copy two images
__kernel void
copy(__global const float* in, __global float* out, const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    out[gid] = in[gid];
}


// dual-pt1
__kernel void
warping(__global const float* i1, __global const float* i2,
        __global const float* u , __global const float* v ,
        __global float* dx, __global float* dy, __global float* dt,
        const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    const float ugid = u[gid];
    const float vgid = v[gid];

    // dt
    const int x0  = MAX(0, MIN(xx + ugid,  width - 1));
    const int y0  = MAX(0, MIN(yy + vgid, height - 1));
#if INTERP_MODE_WARP
    dt[gid] = bilinear_interp(i2, y0, x0, height, width) - bilinear_interp(i1, yy, xx, height, width);
#else
    dt[gid] = bicubic_interp(i2, y0, x0, height, width) - bicubic_interp(i1, yy, xx, height, width);
#endif

    // dx
    const int xm  = MAX(0, MIN(xx + ugid - 1, width - 1));
    const int xp  = MAX(0, MIN(xx + ugid + 1, width - 1));
#if INTERP_MODE_WARP
    dx[gid] = bilinear_interp(i2, yy, xp, height, width) - bilinear_interp(i2, yy, xm, height, width);
#else
    dx[gid] = bicubic_interp(i2, yy, xp, height, width) - bicubic_interp(i2, yy, xm, height, width);
#endif

    // dy
    const int ym  = MAX(0, MIN(yy + vgid - 1, height - 1));
    const int yp  = MAX(0, MIN(yy + vgid + 1, height - 1));
#if INTERP_MODE_WARP
    dy[gid] = bilinear_interp(i2, yp, xx, height, width) - bilinear_interp(i2, ym, xx, height, width);
#else
    dy[gid] = bicubic_interp(i2, yp, xx, height, width) - bicubic_interp(i2, ym, xx, height, width);
#endif

}


// dual-pt2
__kernel void
magnitude(__global float* out, __global const float* ix, __global const float* iy,
          const float gamma, const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    // load
    const float dx = ix[gid];
    const float dy = iy[gid];

    // squared
    const float s = dx * dx + dy * dy + gamma * gamma;
    out[gid] = MAX(1e-6, s);

}


// dual-pt3
__kernel void
updates(__global const float* u0, __global const float* v0,
        __global float* u , __global float* v , __global float* w ,
        __global float* u_, __global float* v_, __global float* w_,
        __global float* p0, __global float* p1, __global float* p2,
        __global float* p3, __global float* p4, __global float* p5,
        __global const float* dx, __global const float* dy,
        __global const float* dt, __global const float* igsq,
        const float sigma, const float tau, const float lambda, const float gamma,
        const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    // load ======================================

    float gid_u_ = u_[gid], gid_v_ = v_[gid], gid_w_ = w_[gid];
    float gid_u  =  u[gid], gid_v  =  v[gid], gid_w  =  w[gid];
    float gid_p0 = p0[gid], gid_p1 = p1[gid], gid_p2 = p2[gid];
    float gid_p3 = p3[gid], gid_p4 = p4[gid], gid_p5 = p5[gid];

    // dual ======================================

    // shifts
    float u_x, v_x, w_x;
    int sidx = (xx + 1) < width ? xx + 1 : xx;
    sidx = yy * width + sidx;
    u_x = u_[sidx] - gid_u_;
    v_x = v_[sidx] - gid_v_;
    w_x = w_[sidx] - gid_w_;

    float u_y, v_y, w_y;
    int sidy = (yy + 1) < height ? yy + 1 : yy;
    sidy = sidy * width + xx;
    u_y = u_[sidy] - gid_u_;
    v_y = v_[sidy] - gid_v_;
    w_y = w_[sidy] - gid_w_;

    // update dual
    float epsilon = 0.01f;
    gid_p0 = (gid_p0 + sigma * u_x) / (1 + epsilon);
    gid_p1 = (gid_p1 + sigma * u_y) / (1 + epsilon);
    gid_p2 = (gid_p2 + sigma * v_x) / (1 + epsilon);
    gid_p3 = (gid_p3 + sigma * v_y) / (1 + epsilon);
    gid_p4 = (gid_p4 + sigma * w_x) / (1 + epsilon);
    gid_p5 = (gid_p5 + sigma * w_y) / (1 + epsilon);

    // normalize
    float reprojection, p_sq;
    p_sq = sqrt(gid_p0 * gid_p0 + gid_p1 * gid_p1 + gid_p2 * gid_p2 + gid_p3 * gid_p3);
    reprojection = 1.0f / MAX(1.0f, p_sq);

    gid_p0 *= reprojection;
    gid_p1 *= reprojection;
    gid_p2 *= reprojection;
    gid_p3 *= reprojection;

    p_sq = sqrt(gid_p4 * gid_p4 + gid_p5 * gid_p5);
    reprojection = 1.0f / MAX(1.0f, p_sq);

    gid_p4 *= reprojection;
    gid_p5 *= reprojection;

    // primal ====================================

    // divergence
    float div_u, div_v, div_w;
    if (xx == 0) {
        div_u  = gid_p0 - 0;       div_v  = gid_p2 - 0;       div_w  = gid_p4 - 0;
    } else if (xx == width - 1) {
        const int alt = yy * width + (xx - 1);
        div_u  = 0 - p0[alt];      div_v  = 0 - p2[alt];      div_w  = 0 - p4[alt];
    } else {
        const int alt = yy * width + (xx - 1);
        div_u  = gid_p0 - p0[alt]; div_v  = gid_p2 - p2[alt]; div_w  = gid_p4 - p4[alt];
    }

    if (yy == 0) {
        div_u += gid_p1 - 0;       div_v += gid_p3 - 0;       div_w += gid_p5 - 0;
    } else if (yy == height - 1) {
        const int alt = (yy - 1) * width + xx;
        div_u += 0 - p1[alt];      div_v += 0 - p3[alt];      div_w += 0 - p5[alt];
    } else {
        const int alt = (yy - 1) * width + xx;
        div_u += gid_p1 - p1[alt]; div_v += gid_p3 - p3[alt]; div_w += gid_p5 - p5[alt];
    }

    // save old
    gid_u_ = gid_u;
    gid_v_ = gid_v;
    gid_w_ = gid_w;

    // update
    gid_u += tau * div_u;
    gid_v += tau * div_v;
    gid_w += tau * div_w;

    // indexing
    const float rho = dt[gid] + (gid_u - u0[gid]) * dx[gid] + (gid_v - v0[gid]) * dy[gid] + gamma * gid_w;
    const float thr = tau * lambda * igsq[gid];
    if (rho < -thr) {
        gid_u += tau * lambda * dx[gid];
        gid_v += tau * lambda * dy[gid];
        gid_w += tau * lambda * gamma;
    } else if (rho > thr) {
        gid_u -= tau * lambda * dx[gid];
        gid_v -= tau * lambda * dy[gid];
        gid_w -= tau * lambda * gamma;
    } else if (fabs(rho) <= thr) {
        const float denom = 1.0f / igsq[gid];
        gid_u -= rho * dx[gid] * denom;
        gid_v -= rho * dy[gid] * denom;
        gid_w -= rho * gamma   * denom;
    }

    // propagate
    gid_u_ = 2 * gid_u - gid_u_;
    gid_v_ = 2 * gid_v - gid_v_;
    gid_w_ = 2 * gid_w - gid_w_;

    // save ======================================

    u_[gid] = gid_u_;     u[gid] = gid_u;
    v_[gid] = gid_v_;     v[gid] = gid_v;
    w_[gid] = gid_w_;     w[gid] = gid_w;
    p0[gid] = gid_p0;    p1[gid] = gid_p1;
    p2[gid] = gid_p2;    p3[gid] = gid_p3;
    p4[gid] = gid_p4;    p5[gid] = gid_p5;

}


// adapted from:   'median.cl'
//  http://code.google.com/p/socles/
#define cas(a, b)                               \
        do {                                    \
                const float x = a;              \
                const int c = a > b;            \
                a = c ? b : a;                  \
                b = c ? x : b;                  \
        } while (0);
// median filter
__kernel void
median3x3(__global const float* src, __global float* dst, const int M, const int N) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int id = yy * N + xx;

    if (xx >= N || yy >= M) { return; }

    const int xR = xx + 1 <  N ? xx + 1 : xx;
    const int xL = xx - 1 >= 0 ? xx - 1 : xx;
    const int yU = yy + 1 <  M ? yy + 1 : yy;
    const int yD = yy - 1 >= 0 ? yy - 1 : yy;

    float s0 = src[yD * N + xL];
    float s1 = src[yD * N + xx];
    float s2 = src[yD * N + xR];
    float s3 = src[yy * N + xL];
    float s4 = src[id         ];
    float s5 = src[yy * N + xR];
    float s6 = src[yU * N + xL];
    float s7 = src[yU * N + xx];
    float s8 = src[yU * N + xR];

    cas(s1, s2);
    cas(s4, s5);
    cas(s7, s8);

    cas(s0, s1);
    cas(s3, s4);
    cas(s6, s7);

    cas(s1, s2);
    cas(s4, s5);
    cas(s7, s8);

    cas(s3, s6);
    cas(s4, s7);
    cas(s5, s8);
    cas(s0, s3);

    cas(s1, s4);
    cas(s2, s5);
    cas(s3, s6);

    //cas(s5, s8);
    cas(s4, s7);
    cas(s1, s3);

    cas(s2, s6);
    //cas(s5, s7);
    cas(s2, s3);
    cas(s4, s6);

    cas(s3, s4);
    //cas(s5, s6);

    dst[id] = s4;
}


// d_a *= h_b
__kernel void
mul_scalar(__global float* out, const float scalar, const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    out[gid] *= scalar;
}


// warping dt
__kernel void
w_interp_dt(__global const float* i1, __global const float* i2,
            __global const float* u , __global const float* v ,
            __global float* dt,
            const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    const float ugid = u[gid];
    const float vgid = v[gid];

    const int x0 = MAX(0, MIN(xx + ugid,  width - 1));
    const int y0 = MAX(0, MIN(yy + vgid, height - 1));

#if INTERP_MODE_WARP
    dt[gid] = bilinear_interp(i2, y0, x0, height, width) - bilinear_interp(i1, yy, xx, height, width);
#else
    dt[gid] = bicubic_interp(i2, y0, x0, height, width) - bicubic_interp(i1, yy, xx, height, width);
#endif

}


// warping dx and dy
__kernel void
w_interp_dxdy(__global const float* i2,
              __global const float* u , __global const float* v ,
              __global float* dx, __global float* dy,
              const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    const float ugid = u[gid];
    const float vgid = v[gid];

    // dx
    const int xm  = MAX(0, MIN(xx + ugid - 1, width - 1));
    const int xp  = MAX(0, MIN(xx + ugid + 1, width - 1));

#if INTERP_MODE_WARP
    dx[gid] = bilinear_interp(i2, yy, xp, height, width) - bilinear_interp(i2, yy, xm, height, width) ;
#else
    dx[gid] = bicubic_interp(i2, yy, xp, height, width) - bicubic_interp(i2, yy, xm, height, width) ;
#endif

    // dy
    const int ym  = MAX(0, MIN(yy + vgid - 1, height - 1));
    const int yp  = MAX(0, MIN(yy + vgid + 1, height - 1));

#if INTERP_MODE_WARP
    dy[gid] = bilinear_interp(i2, yp, xx, height, width) - bilinear_interp(i2, ym, xx, height, width) ;
#else
    dy[gid] = bicubic_interp(i2, yp, xx, height, width) - bicubic_interp(i2, ym, xx, height, width) ;
#endif

}


__kernel void bilateral(__global const float* imageIn,
                        __global float* imageOut,
                        const int radius, const float sigmaS,
                        const int height, const int width) {

    const int xx  = get_global_id(1);
    const int yy  = get_global_id(0);
    const int gid = yy * width + xx;

    if (xx >= width || yy >= height) { return; }

    float wp     = 0.f;
    float sum    = 0.f;
    float sigmaR = pow(2.0f * radius + 1, 2);
    float center = imageIn[gid];

    for (int i = -radius; i <= radius; ++i) {
        for (int j = -radius; j <= radius; ++j) {

            int lx = min(max(xx + j, 0), width - 1);
            int ly = min(max(yy + i, 0), height - 1);

            float curr   = imageIn[ly * width + lx];
            float intens = center - curr;

            float delta  = sqrt((float)(i * i + j * j));
            float sdiff  = exp(-0.5f * pow(delta  / sigmaR, 2));
            float factor = exp(-0.5f * pow(intens / sigmaS, 2)) * sdiff;

            wp += factor * curr;
            sum += factor;
        }
    }

    imageOut[gid] = (wp / sum);
}
