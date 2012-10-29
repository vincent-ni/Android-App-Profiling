/*
 *  image_util.h
 *  ImageFilterLib
 *
 *  Created by Matthias Grundmann on 10/3/08.
 *  Copyright 2008 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef IMAGE_UTIL_H__
#define IMAGE_UTIL_H__

#include <cmath>

#include <boost/shared_ptr.hpp>
#include <opencv2/core/core_c.h>

namespace ImageFilter {

using boost::shared_ptr;

// Mimicks cvReleaseImage but for IplImage* instead of IplImage**,
// i.e. the input pointer will be not set to zero.
inline void cvReleaseImageShared(IplImage* img) {
  IplImage* tmp = img;
  cvReleaseImage(&tmp);
}

// Mimicks cvReleaseMat but for CvMat* instead of CvMat**,
// i.e. the input pointer will be not set to zero.
inline void cvReleaseMatShared(CvMat* mat) {
  CvMat* tmp = mat;
  cvReleaseMat(&tmp);
}

// Shared ptr creation functions.
inline shared_ptr<IplImage> cvCreateImageShared(CvSize size, int depth, int channels) {
  return shared_ptr<IplImage>(cvCreateImage(size, depth, channels), cvReleaseImageShared);
}

inline shared_ptr<IplImage> cvCreateImageShared(int width, int height, int depth, int channels) {
  return shared_ptr<IplImage>(cvCreateImage(cvSize(width, height), depth, channels),
                              cvReleaseImageShared);
}

inline shared_ptr<CvMat> cvCreateMatShared(int rows, int cols, int type) {
  return shared_ptr<CvMat>(cvCreateMat(rows, cols, type), cvReleaseMatShared);
}

// Clone functions.
inline shared_ptr<IplImage> cvCloneImageShared(const IplImage* src) {
  return shared_ptr<IplImage>(cvCloneImage(src), cvReleaseImageShared);
}

inline shared_ptr<IplImage> cvCloneImageShared(shared_ptr<IplImage> src) {
  return shared_ptr<IplImage>(cvCloneImage(src.get()), cvReleaseImageShared);
}

// Takes ownership of passed IplImage ptr.
inline shared_ptr<IplImage> cvImageToSharedPtr(IplImage* src) {
  return shared_ptr<IplImage>(src, cvReleaseImageShared);
}

// Helper functions for pointer indexing.
template <class T>
T* PtrOffset(T* t, int offset) {
  return reinterpret_cast<T*>(reinterpret_cast<uchar*>(t) + offset);
}

template <class T>
const T* PtrOffset(const T* t, int offset) {
  return reinterpret_cast<const T*>(reinterpret_cast<const uchar*>(t) + offset);
}

// RowPtr for IplImage.
template <typename T>
inline const T* RowPtr(const IplImage* img, int row) {
  return reinterpret_cast<T*>(img->imageData + row*img->widthStep); 
}

template <typename T>
inline T* RowPtr(IplImage* img, int row) {
  return reinterpret_cast<T*>(img->imageData + row*img->widthStep); 
}

template <typename T>
inline const T* RowPtr(shared_ptr<const IplImage> img, int row) {
  return reinterpret_cast<T*>(img->imageData + row*img->widthStep); 
}

template <typename T>
inline T* RowPtr(shared_ptr<IplImage> img, int row) {
  return reinterpret_cast<T*>(img->imageData + row*img->widthStep); 
}

// RowPtr for CvMat.
template <typename T>
inline const T* RowPtr(const CvMat* mat, int row) {
  return reinterpret_cast<const T*>(mat->data.ptr + row*mat->step);
}

template <typename T>
inline T* RowPtr(CvMat* mat, int row) {
  return reinterpret_cast<T*>(mat->data.ptr + row*mat->step);
}

template <typename T>
inline const T* RowPtr(shared_ptr<const CvMat> mat, int row) {
  return reinterpret_cast<const T*>(mat->data.ptr + row*mat->step);
}

template <typename T>
inline T* RowPtr(shared_ptr<CvMat> mat, int row) {
  return reinterpret_cast<T*>(mat->data.ptr + row*mat->step);
}


// Helper functions to set and reset ROI to constant IplImge.
inline void cvSetImageROIConst(const IplImage* image, CvRect rect) {
  IplImage* img = const_cast<IplImage*>(image);
  cvSetImageROI(img, rect);
}

inline void cvResetImageROIConst(const IplImage* image) {
  IplImage* img = const_cast<IplImage*>(image);
  cvResetImageROI(img);
}

// Returns cvMat* corresponding to the ROI of image. 
// Matrix holds the actual data.
inline
CvMat* cvMatFromImageROI(const IplImage* image, CvRect rect, CvMat* matrix) {
  cvSetImageROIConst(image, rect);
  CvMat* ret_val = cvGetMat(image, matrix);
  cvResetImageROIConst(image);
  return ret_val;
}

// Returns true in case image had to be reallocated.
inline bool ImageReallocate(CvSize img_size, int depth, int channels, shared_ptr<IplImage>* img) {
  if (*img == NULL ||
      (*img)->width != img_size.width || 
      (*img)->height != img_size.height ||
      (*img)->depth != depth || 
      (*img)->nChannels != channels) {
    *img = cvCreateImageShared(img_size, depth, channels);
    return true;
  } else {
    return false;
  }
}

inline bool ImageReallocate(int width, int height, int depth, int channels, shared_ptr<IplImage>* img) {
  return ImageReallocate(cvSize(width, height), depth, channels, img);
}

// Dimension checks
inline bool HasSameDimensions(const IplImage* img_1, const IplImage* img_2) {
  return (img_1->width == img_2->width && img_1->height == img_2->height);
}

inline bool HasSameDimensions(const IplImage* img, const CvMat* mat) {
  return (img->width == mat->cols && img->height == mat->rows);
}

inline bool HasSameDimensions(const CvMat* mat_1, const CvMat* mat_2) {
  return (mat_1->cols == mat_2->cols && mat_1->rows == mat_2->rows);
}

inline CvPoint trunc(const CvPoint2D32f& pt) {
  return cvPoint((int)pt.x, (int)pt.y);
}
  
inline CvPoint2D32f operator+(const CvPoint2D32f& lhs, const CvPoint2D32f& rhs) {
  return cvPoint2D32f(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline CvPoint2D32f operator-(const CvPoint2D32f& lhs, const CvPoint2D32f& rhs) {
  return cvPoint2D32f(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline CvPoint2D32f operator*(const CvPoint2D32f& lhs, float f) {
  return cvPoint2D32f(lhs.x * f, lhs.y * f);
}

inline float dot(const CvPoint2D32f& lhs, const CvPoint2D32f& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline float sq_norm(const CvPoint2D32f& lhs) {
  return dot(lhs, lhs);
}

inline float norm(const CvPoint2D32f& lhs) {
  return std::sqrt(sq_norm(lhs));
}

inline CvPoint2D32f rotate(const CvPoint2D32f& pt, float angle) {
  const float c = cos(angle);
  const float s = sin(angle);
  return cvPoint2D32f(c * pt.x - s * pt.y, s * pt.x + c * pt.y);
}

inline CvPoint2D32f normalize(const CvPoint2D32f& pt) {
  return pt * (1.0 / sqrt(sq_norm(pt)));
}

// Expects matrix to be row-major.
inline CvPoint2D32f transform(const CvPoint2D32f& pt, const float* matrix) {
  return cvPoint2D32f(matrix[0] * pt.x + matrix[1] * pt.y,
                      matrix[2] * pt.x + matrix[3] * pt.y);
}

inline CvPoint2D32f transform(const CvPoint2D32f& pt, const double* matrix) {
  return cvPoint2D32f(matrix[0] * pt.x + matrix[1] * pt.y,
                      matrix[2] * pt.x + matrix[3] * pt.y);
}

inline CvPoint3D32f homogPt(const CvPoint2D32f& pt) {
  return cvPoint3D32f(pt.x, pt.y, 1.0);
}
  
inline CvPoint2D32f homogPt(const CvPoint3D32f& pt) {
  return cvPoint2D32f(pt.x / pt.z, pt.y / pt.z);
}

inline CvPoint2D32f transform(const CvPoint2D32f& pt, const CvMat* matrix) {
  CvPoint3D32f pt_3d = homogPt(pt);
  CvMat pt_view;
  cvInitMatHeader(&pt_view, 1, 3, CV_32F, &pt_3d);
  
  CvPoint3D32f result;
  CvMat result_view;
  cvInitMatHeader(&result_view, 1, 3, CV_32F, &result);
  
  cvGEMM(&pt_view, matrix, 1.0, NULL, 0, &result_view, CV_GEMM_B_T);
  return homogPt(result);
}
  
inline CvPoint3D32f operator+(const CvPoint3D32f& lhs, const CvPoint3D32f& rhs) {
  return cvPoint3D32f(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

inline CvPoint3D32f operator-(const CvPoint3D32f& lhs, const CvPoint3D32f& rhs) {
  return cvPoint3D32f(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

inline CvPoint3D32f operator*(const CvPoint3D32f& lhs, float f) {
  return cvPoint3D32f(lhs.x * f, lhs.y * f, lhs.z * f);
}

inline float dot(const CvPoint3D32f& lhs, const CvPoint3D32f& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline float sq_norm(const CvPoint3D32f& lhs) {
  return dot(lhs, lhs);
}

inline float norm(const CvPoint3D32f& lhs) {
  return std::sqrt(sq_norm(lhs));
}

inline CvPoint3D32f transform(const CvPoint3D32f& lhs, const float matrix[9]) {
  return cvPoint3D32f(matrix[0] * lhs.x + matrix[1] * lhs.y + matrix[2] * lhs.z,
                      matrix[3] * lhs.x + matrix[4] * lhs.y + matrix[5] * lhs.z,
                      matrix[6] * lhs.x + matrix[7] * lhs.y + matrix[8] * lhs.z);
}
  
inline float clamp(float value, float a, float b) {
  return std::max<float>(a, std::min<double>(value, b));
}

inline float clamp(double value, double a, double b) {
  return std::max<double>(a, std::min<double>(value, b));
}

inline float clamp(int value, int a, int b) {
  return std::max<int>(a, std::min<int>(value, b));
}

inline bool WithinInterval(int value, int a, int b) {
  if (value >= a && value < b) {
    return true;
  }
  return false;
}
  
}  // namespace ImageFilter.

#endif
