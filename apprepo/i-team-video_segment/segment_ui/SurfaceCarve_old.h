#ifndef SURFACECARVE_H
#define SURFACECARVE_H

#include "VideoData.h"
#include <QThread>

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

	float* img(int i) { return _data+_ld*_sy*i; }
  const float* img(int i) const { return _data+_ld*_sy*i; }
	float* at(int x, int y, int t) { return _data + _ld*(_sy*t + y) +x; };
  const float* at(int x, int y, int t) const { return _data + _ld*(_sy*t + y) +x; };

private:
	Matrix3D ( const Matrix3D& rhs) {};
	Matrix3D& operator= (const Matrix3D& rhs) { return *this; }

protected:

	float*	_data;
	
	int  _step;
	int  _ld;

	int _sx;
	int _sy;
	int _st;
};

class SurfaceCarve : public QThread {
	Q_OBJECT

public:
	SurfaceCarve ( const VideoData* data, int percentage );
	~SurfaceCarve ();
	int getSeams () { return _seams; }
	void run ();

	unsigned char* getCarvedImg (int which, int seams);
	int getStep () { return _steps; }

public slots:
	void cancelJob();

signals:
	void done(int);

protected:
	int _seams;
	const VideoData* _video;

protected:
	void getSeam ( const Matrix3D& C, Matrix3D& S, Matrix3D& M, Matrix3D& I, Matrix3D& T, int width, int seam);
	void resolveDisconts ( Matrix3D& S, Matrix3D& R, const Matrix3D& C, int seam);
	void removeSeam ( Matrix3D& video, const Matrix3D& S, int width, int seam);


	float min3 ( float a, float b, float c) { return std::min (a,std::min(b,c)); } 

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

	Matrix3D		_thresholdMap;
	unsigned char*	_tmpImg;
	unsigned char*	_mask;
	int				_maskStep;
	int				_steps;
	bool			_isCanceled;

};


#endif