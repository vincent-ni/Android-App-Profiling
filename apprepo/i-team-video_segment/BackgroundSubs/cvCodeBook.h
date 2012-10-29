#ifndef _CVCODEBOOK_H_
#define _CVCODEBOOK_H_

#include <algorithm>
#include <iostream>
#include <cmath>

#include "cv.h"
#include "cxmisc.h"


typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

struct CodeWord
{
	CodeWord* next;

	uint	firstAccessTime, lastAccessTime;
	uint	frequency, maxNegativeRunLength;
	double	minIllumination, maxIllumination;
	double	red, green, blue;
};

struct CodeBookModel
{
	CodeWord**	codeBooks;
	uint		frameCount;
	double		colorThreshold;
	double		alpha;
	double		beta;
	CvSize		imageSize;

	CodeBookModel( const CvSize& imageSize, double colorThreshold = 8, double alpha = 0.7, double beta = 1.3 );
	CodeBookModel( const IplImage** images, uint imageCount, double colorThreshold = 15, uint maxNegativeRunLengthThreshold = 0, double alpha = 0.55, double beta = 1.3  );
	virtual ~CodeBookModel( );
	void train( const IplImage* image, const IplImage* maskOutput );
	void detect( const IplImage* image, const IplImage* maskOutput );
	void detectNonAdapting( const IplImage* image, const IplImage* maskOutput );
	void finishTraining( uint maxNegativeRunLengthThreshold = 0, double RLEThreshold = 0.5 );
};

#endif /* _CVCODEBOOK_H_ */
