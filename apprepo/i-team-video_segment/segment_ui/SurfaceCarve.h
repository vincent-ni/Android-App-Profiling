#ifndef SURFACECARVE_H
#define SURFACECARVE_H

#include "VideoData.h"
#include <QThread>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

#include <vector>
using std::vector;

#include <utility>
using std::pair;

#include <cv.h>

class MovieWidget;

class Matrix3D
{
public:
	Matrix3D ( int x, int y, int t);
	~Matrix3D();

	int sx() const { return _sx; }
	int sy() const { return _sy; }
	int st() const { return _st; }

	const float* d() const { return _data; }
  float* d() { return _data; }
  
	int ld() const { return _ld; }
	int step() const { return _step; }

	const float* img(int i) const { return _data+_ld*_sy*i; }
  float* img(int i) { return _data+_ld*_sy*i; }
  
	const float* at(int x, int y, int t) const { return _data + _ld*(_sy*t + y) +x; };
  float* at(int x, int y, int t) { return _data + _ld*(_sy*t + y) +x; };
  
  // Assumes sx to be composed of cn interlaced channels.
  void ImageView(IplImage* ipl_img, int t = 0, int cn = 1, int max_width = 0) const {
    if (max_width == 0) max_width = _sx;
    cvInitImageHeader(ipl_img, cvSize(max_width / cn, _sy), IPL_DEPTH_32F, cn);
    cvSetData(ipl_img, (void*) img(t), _step);
  }

private:
	Matrix3D (const Matrix3D& rhs) {};
	Matrix3D& operator= (const Matrix3D& rhs) { return *this; }

protected:

	float*	_data;
	
	int  _step;
	int  _ld;

	int _sx;
	int _sy;
	int _st;
};

struct FlowSeamElem {
  float x;
  float y;
  float magn;
};

typedef vector<FlowSeamElem> FlowSeam;

typedef shared_ptr<Matrix3D> Matrix3DPtr;

class SurfaceCarve : public QThread {
	Q_OBJECT

public:
	SurfaceCarve (const VideoData* data, int percentage, float temporal_weight,
                float forward_weight, float saliency_weight, bool blend, int n_window,
                const QString& flow_file);
	~SurfaceCarve ();
	int getSeams () const { return _seams; }
	void run ();

	unsigned char* getCarvedImg (int which, int seams);
	int getStep () { return _steps; }
  
  void SetSaliencyMap(const VideoData* saliency) { _saliency_map = saliency; }
  void SetUserInput(MovieWidget* _movieWidget) { _user_input = _movieWidget; }

public slots:
	void cancelJob();

signals:
	void done(int);
  
protected:
  void ComputeNewSeams(const Matrix3D& frame_data,
                       Matrix3D& saliency,
                       int cur_width,
                       const Matrix3D& constrain_frame,
                       bool enforce_temporal_coherence,
                       Matrix3D& Tmp,
                       vector<int>& cur_seam,
                       const FlowSeam& flow_seam,
                       IplImage* temporal_coherence_mask = 0);
  
  void ComputeDiscontSeams(const Matrix3D& video_data,
                           const Matrix3D& saliency,
                           int cur_width,
                           const Matrix3D& constrain_frame,
                           bool enforce_temporal_coherence,
                           Matrix3D& Tmp,
                           vector<int>& cur_seam,
                           IplImage* temporal_coherence_mask = 0);
  
  // Masks out a neighborhood of at least diam around the current seam.
  // Neighborhood might be actually larger than diam to accommodate  
  // previous seam within window if specified. Zero can be passed if such a 
  // constrained is not desired.
  pair<int,int> UpscaleSeam(const Matrix3D& color_frame, const Matrix3D& saliency,
                            const vector<int>& seam, const vector<int>* prev_seam,
                            const int diam, int cur_width,
                            float scale, Matrix3D* new_color_frame,
                            Matrix3D* new_saliency) const;
  
  void DownSampleSeam(const float* src_seam, int src_length,
                      float* dst_seam, int dst_length, float scale);
  
  
	void getSeam(const Matrix3D& C, Matrix3D& S, Matrix3D& M, Matrix3D& I, Matrix3D& T, 
               int width, int seam, int first, int last);
	void resolveDisconts(Matrix3D& S, Matrix3D& R, const Matrix3D& C, int seam, 
                      int first, int last);
  // Removes Seam with number seam from mat assuming seam is frame frame_number.
	void removeSeam(Matrix3D& mat, const Matrix3D& S, int width, int seam,
                  int frame_number, int idx);

  // Same as above with new interface.
  void RemoveSeam(const Matrix3D& input, 
                  int frame,
                  int cur_width,
                  int colors,
                  const vector<int>& old_seam,
                  Matrix3D& output,
                  int out_frame = 0);
  
  void removeSeamBlend(Matrix3D& mat, const Matrix3D& S, int width, int seam,
                  int frame_number, int idx);
  
  void DisplaceSeamWithFlow(const IplImage* flow_x, 
                            const IplImage* flow_y,
                            int width,
                            int height,
                            const vector<int>& seam,
                            FlowSeam* flow_seam) const;
  
  void MarkFlowSeam(float* coherence_map,
                    int width,
                    int width_step,
                    int height,
                    int rad,
                    const FlowSeam& flow_seam);

	float min3 ( float a, float b, float c) { return std::min (a,std::min(b,c)); } 
  
  float MinVecIdx(float* vec, int sz, int& idx);

	float min3idx ( float a, float b, float c, int& idx);
	float min2idx ( float a, float b, int& idx);

	class NotNeighborTo
	{
	public:
		NotNeighborTo ( int i ) : _i(i) {}
		bool operator() (int j) { return std::abs(j-_i) > 1; }
	protected:
		int _i;
	};
  
protected:
	int _seams;
	const VideoData* _video;
  const VideoData* _saliency_map;
  MovieWidget* _user_input;
  
  bool blend_mode_;
  
  float temporal_weight_;
  float forward_weight_;
  float saliency_weight_;
  
  float temporal_threshold_;
  float min_temporal_weight_;
  float max_temporal_weight_;
  
  int neighbor_window_;
  QString flow_file_;

	Matrix3D		_thresholdMap;
  Matrix3DPtr _computed_seams;
  Matrix3D    _blendImg; 
  
	unsigned char*	_tmpImg;
	unsigned char*	_mask;
	int				_maskStep;
	int				_steps;
	bool			_isCanceled;

	int				_surfLength;
};


#endif