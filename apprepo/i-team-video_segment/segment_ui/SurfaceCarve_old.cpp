#include "SurfaceCarve.h"

#include <ippi.h>
#include <ippcv.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

SurfaceCarve::SurfaceCarve(const VideoData *data, int percentage) : _video(data),
	_thresholdMap(data->getWidth(), data->getHeight(), data->getFrames() )
{
	_seams = percentage * _video->getWidth () / 100;
	_tmpImg = ippiMalloc_8u_C4 ( data->getWidth() + _seams, data->getHeight(), &_steps);
	_mask = ippiMalloc_8u_C1 ( data->getWidth(), data->getHeight(), &_maskStep);
}

SurfaceCarve::~SurfaceCarve()
{
	ippiFree (_tmpImg);
	ippiFree (_mask);
}

void SurfaceCarve::cancelJob()
{
	_isCanceled = true;
}

void SurfaceCarve::run()
{
	_isCanceled = false;

	// allocate 3D Matrix
	const int sx = _video->getWidth();
	const int sy = _video->getHeight();
	const int st = _video->getFrames ();

	Matrix3D myVideo ( sx, sy, st);

	// convert video to gray float video
	Matrix3D rgb (sx, sy, 3);

	Ipp8u* plane[4];
	int	   planeStep[4];
	IppiSize imgSz = { sx, sy };
	float  colCoeffs [] = { 0.2989, 0.5870, 0.1140 };

	for ( int i=0; i < 4; ++i )
	{
		plane[i] = ippiMalloc_8u_C1 ( sx, sy, &planeStep[i] );
	}

	for ( int t =0; t< st; ++t) {
		ippiCopy_8u_C4P4R ( (Ipp8u*) _video->getData()[t]->getData(), sx*sizeof(int), plane, planeStep[0], imgSz );
		
		// convert to float 
		for (int col=0; col < 3; ++col) {
			ippiConvert_8u32f_C1R ( plane[2-col], planeStep[0], rgb.img(col), rgb.step(), imgSz ); 
			ippiMulC_32f_C1IR ( colCoeffs[col], rgb.img(col), rgb.step(), imgSz);
		}

		// add for grayscale
		ippiAdd_32f_C1IR ( rgb.img(1), rgb.step(), rgb.img(0), rgb.step(), imgSz);
		ippiAdd_32f_C1R (rgb.img(0), rgb.step(), rgb.img(2), rgb.step(), myVideo.img(t), myVideo.step(), imgSz );
	}

	for (int i=0; i < 4; ++i) ippiFree (plane[i]);

	// myVideo holds grayscale video data --> use from now on

	Matrix3D	graX(sx,sy,st);
	Matrix3D	graY(sx,sy,st);
	Matrix3D	graT(sx,sy,st);

	int bufferSzPre[2];

	IppiSize testSz = { sx, std::max(sy,st) };

	ippiFilterSobelVertGetBufferSize_32f_C1R ( imgSz, ippMskSize3x3, &bufferSzPre[0] );
	ippiFilterSobelHorizGetBufferSize_32f_C1R ( testSz, ippMskSize3x3, &bufferSzPre[1] );

	int bufferSz = std::max ( bufferSzPre[0], bufferSzPre[1] );
	int dummy;
	Ipp8u* graBuffer = ippiMalloc_8u_C1 ( bufferSz, 1, &dummy );

	Matrix3D	S (sy, st, _seams);
	Matrix3D	STmp (sy, st, 1);

	Matrix3D	M (sx, sy, st);
	Matrix3D	I (sx,sy,st);
	Matrix3D	Tmp ( sx, sy, 2);

	for ( int s = 0; s < _seams; ++s )
	{
		emit done(s);

		if ( _isCanceled )
		{
			ippiFree (graBuffer);
			return;
		}

		//compute derivative in X,Y and T and add
		IppiSize roi = { sx-s, sy };
		IppiSize timeSz = { sx-s, st };
		IppiSize matSz = { sx-s, sy*st };

		for ( int t =0; t< st; ++t) {
			
			ippiFilterSobelVertBorder_32f_C1R ( myVideo.img(t), myVideo.step(), graX.img(t), graX.step (), roi,  
				ippMskSize3x3, ippBorderRepl, 0, graBuffer);

			ippiFilterSobelHorizBorder_32f_C1R ( myVideo.img(t), myVideo.step(), graY.img(t), graY.step (), roi,  
				ippMskSize3x3, ippBorderRepl, 0, graBuffer);
    }
    
    for ( int y=0; y < sy; ++y ) {
      ippiFilterSobelHorizBorder_32f_C1R ( myVideo.d() + myVideo.ld() * y, myVideo.step()*myVideo.sy(),
                                          graT.d() + graT.ld()*y, graT.step()*graT.sy(), timeSz, ippMskSize3x3, ippBorderRepl, 0, graBuffer );
    }
    
    ippiAbs_32f_C1IR ( graX.d(), graX.step(), matSz );
		ippiAbs_32f_C1IR ( graY.d(), graY.step(), matSz );
		ippiAbs_32f_C1IR ( graT.d(), graT.step(), matSz );

		ippiAdd_32f_C1IR ( graY.d(), graY.step(), graX.d(), graX.step(), matSz );
		ippiAdd_32f_C1IR ( graT.d(), graT.step(), graX.d(), graX.step(), matSz );

		// graX holds cost information

		// compute Seam
		getSeam ( graX, S, M, I, Tmp, sx-s, s );
    std::cout << "before: ";
    std::copy(S.img(s), S.img(s)+6, std::ostream_iterator<float>(std::cout, " "));
    std::cout << std::endl;
    std::copy(S.img(s)+S.ld(), S.img(s)+S.ld()+6, std::ostream_iterator<float>(std::cout, " "));
    std::cout << std::endl; 
		resolveDisconts (S,STmp,graX,s);
    std::cout << "after: ";
    std::copy(S.img(s), S.img(s)+6, std::ostream_iterator<float>(std::cout, " "));
    std::cout << "-----------" << std::endl;
		removeSeam ( myVideo, S, sx-s, s);

	}

	ippiFree (graBuffer);

	// convert Seams to video indexing 

	// first adjust seam offsets
	for ( int s = _seams-2; s >=0; --s )
	{
		for ( int r = s+1; r < _seams; ++r)
		{
			for ( int t = 0; t<S.sy(); ++t)
				for (int j=0; j < S.sx(); ++j)
				{
					if ( *S.at(j,t,s) <= *S.at(j,t,r) )
						++(*S.at(j,t,r));
				}
		}
	}

	// create thresholdMap
	IppiSize vidSz = { sx, sy*st };
	ippiSet_32f_C1R (-1, _thresholdMap.d(), _thresholdMap.ld(), vidSz );
	for ( int s = 0; s < _seams; ++s )
		for ( int t=0; t<st; ++t) 
		{
			float* seam = S.at(0,t,s);

			for ( int i = 0; i < sy; ++i )
				*_thresholdMap.at(*(seam+i), i, t) = sx-s;
		}

	emit done(_seams);
	
}

unsigned char* SurfaceCarve::getCarvedImg (int which, int seam)
{
	IppiSize imgRoi = { _thresholdMap.sx(), _thresholdMap.sy() };
	bool enlarge = false;
	if ( seam > 0 )
	{
		seam *= -1;
		enlarge = true;
	}

	ippiCompareC_32f_C1R ( _thresholdMap.img(which), _thresholdMap.step(), _thresholdMap.sx()+seam, _mask, _maskStep, imgRoi, ippCmpLessEq );

	if ( !enlarge )
	{
		// traverse mask and copy everything that is 1
		for ( int i =0; i < _video->getHeight(); ++i)
		{
			Ipp8u* curMaskRow = _mask + _maskStep * i;
			int* curSrcRow = _video->getData()[which]->getData() + _video->getWidth()*i;
			int* curDstRow = (int*) (_tmpImg + _steps * i);

			for ( int j=0; j < _video->getWidth(); ++j)
			{
				if ( *(curMaskRow+j) )
					*(curDstRow++) = *(curSrcRow+j);
			}
		}
	}
	else
	{
		for ( int i =0; i < _video->getHeight(); ++i)
		{
			Ipp8u* curMaskRow = _mask + _maskStep * i;
			int* curSrcRow = _video->getData()[which]->getData() + _video->getWidth()*i;
			int* curDstRow = (int*) (_tmpImg + _steps * i);

			for ( int j=0; j < _video->getWidth(); ++j)
			{
				*(curDstRow++) = *(curSrcRow+j);
				if ( ! *(curMaskRow+j) )
					*(curDstRow++) = *(curSrcRow+j);
			}
		}
	}

	return _tmpImg;
}

void SurfaceCarve::removeSeam ( Matrix3D& video, const Matrix3D& S, int width, int seam)
{
	for ( int t = 0 ; t < video.st(); ++t )
	{
		for ( int i=0; i < video.sy(); ++ i)
		{
			int val = *S.at(i,t,seam);
			if ( val > 0 && val < width-1 )
			{
				memcpy ( video.at(val,i,t), video.at(val+1,i,t), (width-1-val)*sizeof(float) );
			}
		}
	}
}

void SurfaceCarve::resolveDisconts (Matrix3D& S, Matrix3D& R, const Matrix3D& C, int seam)
{
	IppiSize seamH = { S.sx()-1, S.sy() };
	IppiSize seamV = { S.sx(), S.sy()-1 };
	int runs =0;
	float maxH, maxV;
	const int max_runs = 1;

	for (  ;runs < max_runs; ++runs )
	{
		// first test if we are done
		ippiSub_32f_C1R ( S.img(seam), S.step(), S.img(seam)+1, S.step(), R.d(), R.step(), seamH );
		ippiAbs_32f_C1IR ( R.d(), R.step (), seamH);
		ippiMax_32f_C1R ( R.d(), R.step(), seamH, &maxH);

		ippiSub_32f_C1R ( S.img(seam), S.step(), S.img(seam)+S.ld(), S.step(), R.d(), R.step(), seamV);
		ippiAbs_32f_C1IR ( R.d(), R.step (), seamV);
		ippiMax_32f_C1R ( R.d(), R.step(), seamV, &maxV);

		if ( maxH <=1 && maxV <=1 )
		{
			std::cout << "Relaxation stopped with " << runs+1 << " iterations.\n";
			break;
		}
		
		for ( int i=0; i < S.sy(); ++i) {
			for (int j =0; j < S.sx(); ++j) {
				int num =0;

				const float val = *S.at(j,i,seam);
        
				float weights[5];
				float vals[5];
				float min_val = 1e30;;
				float max_val = -1e30;
          
				if ( i > 0 ) {
          const float t = *S.at(j,i-1,seam);
          if (j<=1) 
            std::cout << "row up: " << t << "\n";
					if (std::abs(t - val) > 1 ) {
						weights[num] = 1.0 / *C.at ( t, j, i-1);
						vals[num] = t;
						++num;
					}
        }

				if ( i < S.sy()-1 ) {
          const float t = *S.at(j,i+1, seam);
          if (j<=1) 
            std::cout << "row below: " << t << "\n";
					if ( std::abs(t - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j, i+1);
						vals[num] = t;
						++num;
					}
        }

				if ( j > 0 ) {
          const float t =  *S.at((j-1),i,seam);
          if (j<=1) {
            if (S.d() > S.at((j-1),i,seam)) 
              std::cout << "WTF?\n";
          }
          
					if ( std::abs(t - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j-1, i);
						vals[num] = t;
						
						//sum += t; 
						++num;
					}
        }

				if ( j < S.sx()-1 ) {
          const float t= *S.at(j+1,i, seam);
          if (j<=1) 
            std::cout << "elem right: " << t << "\n";
					if ( std::abs (t - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j+1, i);
						vals[num] = t;

						//sum +=t; 
						++num;
						
					}
        }

				if ( num > 0 ) {
					weights[num] = 1.0 / *C.at ( val, j, i);
					vals[num] = val;
					++num;

					// normalize weights to be in range 1 .. 5;
					for ( int k=0; k < num; ++k )
					{
						// cap values
						if ( weights[k] < 1e-4 )
							weights[k] = 1e-4;
						if ( weights[k] > 1e4 )
							weights[k] = 1e4;

						if ( weights[k] < min_val )
							min_val = weights[k];
						if ( weights[k] > max_val )
							max_val = weights[k];
					}

					float sum =0;
          float summed_weight =0;
					
					for ( int k=0; k < num; ++k )
					{
						if ( std::fabs(max_val - min_val) > 1e-2 )
							weights[k] = (weights[k] - min_val) / (max_val - min_val) + 1.0;
						else
							weights[k] = 1.0;

						summed_weight += weights[k];
						sum += vals[k] * weights[k];
					}
          
					*R.at(j,i,0) = std::floor ( sum / summed_weight + 0.5 );
          if( j == 1 && i == 0) {
            std::cout <<	*R.at(j,i,0) << ": ";
            std::copy(&weights[0], &weights[num], std::ostream_iterator<float>(std::cout, " "));
            std::copy(&vals[0], &vals[num], std::ostream_iterator<float>(std::cout, " "));
            std::cout << " >\n";
          }
					
					//*R.at(j,i,0) = std::floor ( (val + sum) / num + 0.5);
				}
				else {
					*R.at(j,i,0) = val;
				}
			}
		}
		
		IppiSize imgRoi = { S.sx(), S.sy() };
		ippiCopy_32f_C1R ( R.d(), R.step(), S.img(seam), S.step(), imgRoi );
	}

	if (runs == max_runs )
	{
		std::cout << "!!! Relaxation has not stopped! Rest: " << maxH << "   " << maxV << "\n";
	}
}

void SurfaceCarve::getSeam ( const Matrix3D& C, Matrix3D& S, Matrix3D& M, Matrix3D& I, Matrix3D& T, int width, int seam)
{
	// set M and I zero
	IppiSize roi3D = { width, C.sy() * C.st() };
	IppiSize roiImg = { width, C.sy () };
  
	ippiSet_32f_C1R (0, M.d(), M.step(), roi3D );
	ippiSet_32f_C1R (0, I.d(), I.step(), roi3D );
  
	// initialize M and I
	ippiSet_32f_C1R (-1, I.img(0), I.step(), roiImg);
  
	memcpy ( M.d(), C.d(), width*sizeof(float) );
  
	// compute forward seams in first image
  for ( int i =1; i < C.sy(); ++i )
  {
    float* McurRow = M.img(0)+M.ld()*i;
    float* MprevRow = McurRow - M.ld();
    const float* CcurRow = C.img(0)+C.ld()*i;
    
    // first elem
    *McurRow = std::min ( *MprevRow, *(MprevRow+1)) + *CcurRow;
    
    for ( int j =1; j < width-1; ++j )
    {
      *(McurRow+j) = min3 ( *(MprevRow+j-1), *(MprevRow+j), *(MprevRow+j+1))
      + *(CcurRow+j);
    }
    
    //last elem
    *(McurRow+width-1) = std::min ( *(MprevRow+width-2), *(MprevRow+width-1) ) + 
    *(CcurRow+width-1);
    
  }
  
	// compute backward seams
  
	ippiSet_32f_C1R (0, T.d(), T.step(), roiImg );
	memcpy ( T.d() + T.ld() * (T.sy()-1), C.d() + C.ld() * ( C.sy()-1 ), width*sizeof(float));
  
  for ( int i =C.sy()-2; i >= 0; --i )
  {
    float* TcurRow = T.d()+T.ld()*i;
    float* TprevRow = TcurRow + T.ld();
    const float* CcurRow = C.img(0)+C.ld()*i;
    
    // first elem
    *TcurRow = std::min ( *TprevRow, *(TprevRow+1)) + *CcurRow;
    
    for ( int j =1; j < width-1; ++j )
    {
      *(TcurRow+j) = min3 ( *(TprevRow+j-1), *(TprevRow+j), *(TprevRow+j+1))
      + *(CcurRow+j);
    }
    
    //last elem
    *(TcurRow+width-1) = std::min ( *(TprevRow+width-2), *(TprevRow+width-1) ) + 
    *(CcurRow+width-1);
    
  }
  
	// complete seam costs
	ippiAdd_32f_C1IR ( T.d(), T.step(), M.d(), M.step(), roiImg );
	ippiSub_32f_C1IR ( C.d(), C.step(), M.d(), M.step(), roiImg );
	//new
	ippiDivC_32f_C1IR ( C.sy(), M.d(), M.step(), roiImg ); 
  
	// traverse images
  int idx;
  float elem;
  float minValAbove;
  int	  minElemAbove;
  std::vector<int> tmpMask;
  tmpMask.reserve(3);
  
  for ( int t=1; t < C.st(); ++t )
  {
    // first row
    {
      elem = min2idx ( *M.img(t-1), *(M.img(t-1)+1), idx ); // min2idx ( M.at(1,1,t-1), M.at(2,1,t-1) )
      *I.img(t) = idx;
      //				*M.img(t) = 0.5*elem + *C.img(t);
      *M.img(t) = elem + *C.img(t);
    }
    
    for ( int j =1; j < width-1; ++j)
    {
      elem = min3idx ( *(M.img(t-1)+j-1), *(M.img(t-1)+j), *(M.img(t-1)+j+1), idx );
      *(I.img(t)+j) = j+idx;
      //*(M.img(t)+j) = 0.5*elem +*(C.img(t)+j);
      *(M.img(t)+j) = elem +*(C.img(t)+j);
    }
    
    elem = min2idx ( *(M.img(t-1)+width-2), *(M.img(t-1)+width-1), idx ); 
    *(I.img(t)+width-1) = width-2+idx;
    //*(M.img(t)+width-1) = 0.5*elem + *(C.img(t)+width-1);
    *(M.img(t)+width-1) = elem + *(C.img(t)+width-1);
    
    for ( int i =1; i < C.sy(); ++i )
    {
      float* McurRow = M.img(t)+M.ld()*i;
      float* MprevRow = McurRow - M.ld();
      //float* CcurRow = C.img(t)+C.ld()*i;
      
      // first elem
      for ( int j =0; j < width; ++j )
      {
        //min from above
        if (j == 0)
        {
          elem = min2idx ( *MprevRow, *(MprevRow+1), idx );
          minElemAbove = idx;
        }
        else if ( j == width-1)
        {
          elem = min2idx ( *(MprevRow+width-2), *(MprevRow+width-1), idx );
          minElemAbove = width-2+idx;
        }
        else
        {
          elem = min3idx ( *(MprevRow+j-1), *(MprevRow+j), *(MprevRow+j+1), idx );
          minElemAbove = idx+j;
        }
        
        minValAbove = elem;
        
        //get sideElem
        int minSide = *(I.at(minElemAbove,i-1,t));
        int lower = std::max(0, minSide-1);
        int upper = std::min(minSide+1, width-1);
        
        tmpMask.clear ();
        
        if ( lower+1 < upper )
        {
          tmpMask.push_back(lower); tmpMask.push_back(lower+1); tmpMask.push_back(lower+2);
        }
        else
        {
          tmpMask.push_back(lower); tmpMask.push_back(lower+1);
        }
        
        // remove violation constraint
        std::vector<int>::iterator end = std::remove_if ( tmpMask.begin(), tmpMask.end(), NotNeighborTo(j) );
        
        assert ( !tmpMask.empty () );
        
        //get min
        elem = 1e30;
        for ( std::vector<int>::const_iterator k = tmpMask.begin(); k != end; ++k)
        {
          if ( *(M.at(*k,i,t-1)) < elem)
          {
            elem = *(M.at(*k,i,t-1));
            idx = *k;
          }
        }
        
        *I.at(j,i,t) = idx;
        //*M.at(j,i,t) = minValAbove + *C.at(j,i,t);
        *M.at(j,i,t) = minValAbove + *C.at(j,i,t) + elem;
        
      }
    }
    
    // last row
    float* Mlastrow = M.at(0,M.sy()-1,t-1);
    const float* Clastrow = C.at(0,C.sy()-1,t);
    float* Tlastrow = T.at(0,T.sy()-1,0);
    float* IVallastrow = T.at (0, T.sy()-1, 1);
    float* IlastRow = I.at(0,I.sy()-1, t);
    
    //elem = std::min( *Mlastrow, *(Mlastrow+1) ); // min2idx ( M.at(1,1,t-1), M.at(2,1,t-1) )
    //*Tlastrow = 0.5*elem + *Clastrow;
    *IVallastrow = *(Mlastrow+(int)*IlastRow);
    *Tlastrow    = *IVallastrow + *Clastrow;
    
    for ( int j =1; j < width-1; ++j)
    {
      //elem = min3 ( *(Mlastrow+j-1), *(Mlastrow+j), *(Mlastrow+j+1) );
      //*(Tlastrow+j) = 0.5*elem +*(Clastrow+j);
      *(IVallastrow+j) = *(Mlastrow+(int)(*(IlastRow+j)));
      *(Tlastrow+j)    = *(IVallastrow+j) +*(Clastrow+j);
    }
    
    //elem = std::min( *(Mlastrow+width-2), *(Mlastrow+width-1)); 
    //			*(Tlastrow+width-1) = 0.5*elem + *(Clastrow+width-1);
    *(IVallastrow+width-1) = *(Mlastrow+(int)(*(IlastRow+width-1)));
    *(Tlastrow+width-1)    = *(IVallastrow+width-1) +*(Clastrow+width-1);
    //*(Tlastrow+width-1) = elem + *(Clastrow+width-1);
    
    for ( int i =C.sy()-2; i >= 0; --i )
    {
      float* TcurRow = T.d()+T.ld()*i;
      float* TprevRow = TcurRow + T.ld();
      const float* CcurRow = C.img(t)+C.ld()*i;
      float* IcurRow = I.img(t) + I.ld()*i;
      float* MprevRow = M.img(t-1) + M.ld()*i;
      float* IValcurRow = T.img(1) + T.ld() * i;
      
      // first elem
      //*TcurRow = std::min ( *TprevRow, *(TprevRow+1)) + *CcurRow ;
      *IValcurRow = *(MprevRow+(int)(*IcurRow));
      *TcurRow = std::min ( *TprevRow, *(TprevRow+1)) + *CcurRow + *IValcurRow; 
      
      for ( int j =1; j < width-1; ++j )
      {
        *(IValcurRow+j) = *(MprevRow+(int)(*(IcurRow+j)));
        *(TcurRow+j) = min3 ( *(TprevRow+j-1), *(TprevRow+j), *(TprevRow+j+1))
				//					+ *(CcurRow+j);
        + *(CcurRow+j) + *(IValcurRow+j);
      }
      
      //last elem
      *(IValcurRow+width-1) = *(MprevRow+(int)(*(IcurRow+width-1)));
      *(TcurRow+width-1) = std::min ( *(TprevRow+width-2), *(TprevRow+width-1) ) + 
      //*(CcurRow+width-1);
      + *(CcurRow+width-1) + *(IValcurRow+width-1);
    }
    
    // complete seam costs
    ippiAdd_32f_C1IR ( T.d(), T.step(), M.img(t), M.step(), roiImg );
    ippiSub_32f_C1IR ( C.img(t), C.step(), M.img(t), M.step(), roiImg );
    ippiSub_32f_C1IR ( T.img(1), T.step(), M.img(t), M.step(), roiImg );
    //new
    ippiDivC_32f_C1IR ( C.sy()*2.0, M.img(t), M.step(), roiImg);
  }
  
  // compose Seam
  // get minimum
  
  float* start = M.at(0, M.sy()-1, M.st()-1);
	float* end = M.at(width-1, M.sy()-1, M.st()-1);
  
	int minIdx = std::min_element ( start, end ) - start;
	
	*S.at ( S.sx()-1, S.sy()-1,seam ) = minIdx;
	*S.at ( S.sx()-1, S.sy()-2,seam ) = *I.at(minIdx, I.sy()-1, I.st()-1);
  
	int idxOffset;
	float* Mcur;
  
	for ( int i =C.sy()-2; i >=0; --i )
	{
		if ( minIdx == 0 )
		{
			min2idx ( *M.at(0,i,M.st()-1), *M.at(1,i,M.st()-1), minIdx );
		}
		else if ( minIdx == width-1)
		{
			min2idx ( *M.at(width-2,i,M.st()-1), *M.at(width-1,i,M.st()-1), minIdx );
			minIdx += width-2;
		}
		else
		{
			Mcur = M.at(minIdx,i,M.st()-1);
			min3idx ( *(Mcur-1), *Mcur, *(Mcur+1), idxOffset);
			minIdx += idxOffset;
		}
    
		*S.at ( i, S.sy()-1, seam ) = minIdx;
		*S.at ( i, S.sy()-2, seam) = *I.at(minIdx,i,I.st()-1);
	}
  
	// use only index to backtrack rest
  for ( int t=C.st()-2; t>=0; --t) {
    for ( int i =0; i < S.sx(); ++i) {
      *S.at(i,t,seam) = *I.at ( *S.at(i,t+1,seam), i, t+1 );
    }
  }
}

float SurfaceCarve::min3idx(float a, float b, float c, int &idx)
{
	if ( a <= b && a <= c )
	{
		idx = -1;
		return a;
	}
	else if ( b <= a && b<=c)
	{
		idx = 0;
		return b;
	}
	else if ( c <= a && c<=b )
	{
		idx =1;
		return c;
	}
	else
		throw std::runtime_error ("min3idx consistency error");

	return 0;
}

float SurfaceCarve::min2idx ( float a, float b, int& idx)
{
	if ( a <= b )
	{
		idx = 0;
		return a;
	}
	else
	{
		idx =1;
		return b;
	}
}


Matrix3D::Matrix3D(int x, int y, int t) : _sx(x), _sy(y), _st(t)
{
	_data = ippiMalloc_32f_C1 ( x, y*t, &_step);
	_ld = _step / sizeof (float);

	if ( _step % sizeof(float) )
		throw std::runtime_error ( "Matrix3D ippiMalloc32f float alignment error!"); 

}

Matrix3D::~Matrix3D()
{
	ippiFree(_data);
}