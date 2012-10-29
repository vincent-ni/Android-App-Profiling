#include <stdio.h>
#include "cvCodeBook.h"
#include <vector>
#include <algorithm>

// *****************************************************************************

CV_IMPL CodeBookModel::CodeBookModel( const CvSize& imageSize, double colorThreshold, double alpha, double beta )
{
	this->colorThreshold = colorThreshold;
	this->alpha = alpha;
	this->beta = beta;

	this->imageSize = imageSize;

	uint codeBookCount = imageSize.width * imageSize.height;
	codeBooks = new CodeWord*[ codeBookCount ];
	memset( codeBooks, 0, sizeof( CodeWord* ) * codeBookCount );

	frameCount = 0;
}

// *****************************************************************************

CodeBookModel::CodeBookModel( const IplImage** images, uint imageCount, double colorThreshold, uint maxNegativeRunLengthThreshold, double alpha, double beta )
{
	this->colorThreshold = colorThreshold;
	this->alpha = alpha;
	this->beta = beta;

	for( uint i = 0; i < imageCount; ++i )
	{
		const IplImage* image = images[ i ];
		CvSize newImageSize( cvGetSize( image ) );
		if( i == 0 )
		{
			imageSize = newImageSize;
			uint codeBookCount = imageSize.width * imageSize.height;
			codeBooks = new CodeWord*[ codeBookCount ];
			memset( codeBooks, 0, sizeof( CodeWord* ) * codeBookCount );
		}
	}

	finishTraining( maxNegativeRunLengthThreshold );

	frameCount = 0;
}

// *****************************************************************************

CodeBookModel::~CodeBookModel( )
{
	uint codeBookCount = imageSize.width * imageSize.height;
	for( uint i = 0; i < codeBookCount; ++i )
	{
		CodeWord* codeWord = codeBooks[ i ];
		while( codeWord != 0 )
		{
			CodeWord* nextCodeWord = codeWord->next;
			delete codeWord;
			codeWord = nextCodeWord;
		}
	}

	delete[] codeBooks;
}

// *****************************************************************************

void CodeBookModel::detect( const IplImage* image, const IplImage* maskOutput )
{
	++frameCount;
  
	uint imageWidth = image->width;
	uint imageHeight = image->height;
	uchar* imageData = ( uchar* ) image->imageData;
	uchar* maskImageData = ( uchar* ) maskOutput->imageData;
	uint imageDataDisplacement = image->widthStep - imageWidth - imageWidth;
	uint maskImageDataDisplacement = maskOutput->widthStep - imageWidth - imageWidth;

	uint i = 0;
	for( uint y = 0; y < imageHeight; ++y )
	{
		for( uint x = 0; x < imageWidth; ++x )
		{
			double inputBlue = imageData[ x ];
			++imageData;
			double inputGreen = imageData[ x ];
			++imageData;
			double inputRed = imageData[ x ];

			double inputIlluminationSquare = inputRed * inputRed + inputBlue * inputBlue + inputGreen * inputGreen;
			double inputIllumination = std::sqrt( inputIlluminationSquare );
			double illuminationSquare, colorDotProductSquare, red, green, blue;

			
			CodeWord** lastNextPointer = &( codeBooks[ i ] );
			CodeWord* codeBook = *lastNextPointer;

			while( codeBook != 0 )
			{
				red = codeBook->red;
				green = codeBook->green;
				blue = codeBook->blue;

				illuminationSquare = red * red + green * green + blue * blue;
				colorDotProductSquare = inputRed * red + inputGreen * green + inputBlue * blue;
				colorDotProductSquare *= colorDotProductSquare;

				if( std::sqrt( inputIlluminationSquare - ( colorDotProductSquare / illuminationSquare ) ) <= colorThreshold &&
					inputIllumination >= alpha * codeBook->maxIllumination &&
					inputIllumination <= std::min( beta * codeBook->maxIllumination, codeBook->minIllumination / alpha ) )
				{
					*lastNextPointer = codeBook->next;

					break;
				}

				lastNextPointer = &( codeBook->next );
				codeBook = codeBook->next;
			}

			if( codeBook == 0 )
			{
				maskImageData[ x ] = 255;
				++maskImageData;
				maskImageData[ x ] = 255;
				++maskImageData;
				maskImageData[ x ] = 255;
			}
			else
			{
				codeBook->maxNegativeRunLength = std::max( codeBook->maxNegativeRunLength, frameCount - codeBook->lastAccessTime - 1 );
				codeBook->lastAccessTime = frameCount;

				if( inputIllumination < codeBook->minIllumination  )
				{
					codeBook->minIllumination = inputIllumination;
				}
				else if( inputIllumination > codeBook->maxIllumination )
				{
					codeBook->maxIllumination = inputIllumination;
				}

				double oldFrequency = codeBook->frequency;
				double frequency = ++( codeBook->frequency );

				codeBook->red = ( oldFrequency * codeBook->red + inputRed ) / frequency;
				codeBook->green = ( oldFrequency * codeBook->green + inputGreen ) / frequency;
				codeBook->blue = ( oldFrequency * codeBook->blue + inputBlue ) / frequency;

				codeBook->next = codeBooks[ i ];
				codeBooks[ i ] = codeBook;
				if( codeBook == codeBook->next )
				{
					codeBook->next = 0;
				}

				maskImageData[ x ] = 0;
				++maskImageData;
				maskImageData[ x ] = 0;
				++maskImageData;
				maskImageData[ x ] = 0;
			}

			++i;
		}

		imageData += imageDataDisplacement;
		maskImageData += maskImageDataDisplacement;
	}
}

// *****************************************************************************

void CodeBookModel::detectNonAdapting( const IplImage* image, const IplImage* maskOutput )
{
	uint imageWidth = image->width;
	uint imageHeight = image->height;
	uchar* imageData = ( uchar* ) image->imageData;
	uchar* maskImageData = ( uchar* ) maskOutput->imageData;
	uint imageDataDisplacement = image->widthStep;// - imageWidth - imageWidth;
	uint maskImageDataDisplacement = maskOutput->widthStep;// - imageWidth - imageWidth;

	uint i = 0;
	for( uint y = 0; y < imageHeight; ++y )
	{
		for( uint x = 0; x < imageWidth; ++x )
		{
			double inputBlue = imageData[ 3*x ];
			//++imageData;
			double inputGreen = imageData[ 3*x+1 ];
			//++imageData;
			double inputRed = imageData[ 3*x+2 ];

			double inputIlluminationSquare = inputRed * inputRed + inputBlue * inputBlue + inputGreen * inputGreen;
			double inputIllumination = std::sqrt( inputIlluminationSquare );
			double illuminationSquare, colorDotProductSquare, red, green, blue;

			
			CodeWord** lastNextPointer = &( codeBooks[ i ] );
			CodeWord* codeBook =  codeBooks[ i ];
			while( codeBook != 0 )
			{
				red = codeBook->red;
				green = codeBook->green;
				blue = codeBook->blue;

				illuminationSquare = red * red + green * green + blue * blue;
				colorDotProductSquare = inputRed * red + inputGreen * green + inputBlue * blue;
				colorDotProductSquare *= colorDotProductSquare;

				if( std::sqrt( inputIlluminationSquare - ( colorDotProductSquare / illuminationSquare ) ) <= colorThreshold &&
					inputIllumination >= alpha * codeBook->maxIllumination &&
					inputIllumination <= std::min( beta * codeBook->maxIllumination, codeBook->minIllumination / alpha ) )
				{
					// remove myself
					*lastNextPointer = codeBook->next;

					break;
				}

				lastNextPointer = &( codeBook->next );
				codeBook = codeBook->next;
			}

			if( codeBook == 0 )
			{
				maskImageData[ 3*x ] = 255;
				//++maskImageData;
				maskImageData[ 3*x+1 ] = 255;
				//++maskImageData;
				maskImageData[ 3*x+2 ] = 255;
			}
			else
			{
			
				codeBook->next = codeBooks[ i ];
				codeBooks[ i ] = codeBook;
				if( codeBook == codeBook->next )
				{
					codeBook->next = 0;
				}

				maskImageData[ 3*x ] = 0;
				//++maskImageData;
				maskImageData[ 3*x+1 ] = 0;
				//++maskImageData;
				maskImageData[ 3*x+2 ] = 0 ;
			}

			++i;
		}

		imageData += imageDataDisplacement;
		maskImageData += maskImageDataDisplacement;
	}
}

// *****************************************************************************

void CodeBookModel::train( const IplImage* image, const IplImage* maskOutput )
{
	++frameCount;

	uint imageWidth = image->width;
	uint imageHeight = image->height;
	uchar* imageData = ( uchar* ) image->imageData;
	uchar* maskImageData = ( uchar* ) maskOutput->imageData;
	uint imageDataDisplacement = image->widthStep; // - imageWidth - imageWidth;
	uint maskImageDataDisplacement = maskOutput->widthStep; // - imageWidth - imageWidth;

	uint i = 0;
	for( uint y = 0; y < imageHeight; ++y )
	{
		for( uint x = 0; x < imageWidth; ++x )
		{
			double inputBlue = imageData[ 3*x ];
			//++imageData;
			double inputGreen = imageData[ 3*x+1 ];
			//++imageData;
			double inputRed = imageData[ 3*x+2 ];

			double inputIlluminationSquare = inputRed * inputRed + inputBlue * inputBlue + inputGreen * inputGreen;
			double inputIllumination = std::sqrt( inputIlluminationSquare );
			double illuminationSquare, colorDotProductSquare, red, green, blue;

			
			CodeWord** lastNextPointer = &( codeBooks[ i ] );
			CodeWord* codeBook = *lastNextPointer;

			while( codeBook != 0 )
			{
				red = codeBook->red;
				green = codeBook->green;
				blue = codeBook->blue;

				illuminationSquare = red * red + green * green + blue * blue;
				colorDotProductSquare = inputRed * red + inputGreen * green + inputBlue * blue;
				colorDotProductSquare *= colorDotProductSquare;

				if( std::sqrt( inputIlluminationSquare - ( colorDotProductSquare / illuminationSquare ) ) <= colorThreshold &&
					inputIllumination >= alpha * codeBook->maxIllumination &&
					inputIllumination <= std::min( beta * codeBook->maxIllumination, codeBook->minIllumination / alpha ) )
				{
					// remove myself
					*lastNextPointer = codeBook->next;

					break;
				}

				lastNextPointer = &( codeBook->next );
				codeBook = codeBook->next;
			}

			if( codeBook == 0 )
			{
				codeBook = new CodeWord( );
				codeBook->firstAccessTime = frameCount;
				codeBook->lastAccessTime = frameCount;
				codeBook->frequency = 1;
				codeBook->maxNegativeRunLength = frameCount - 1;
				codeBook->minIllumination = inputIllumination;
				codeBook->maxIllumination = inputIllumination;
				codeBook->red = inputRed;
				codeBook->green = inputGreen;
				codeBook->blue = inputBlue;

				maskImageData[ 3*x ] = 255;
				//++maskImageData;
				maskImageData[ 3*x+1 ] = 255;
				//++maskImageData;
				maskImageData[ 3*x+2 ] = 255;

			}
			else
			{
				codeBook->maxNegativeRunLength = std::max( codeBook->maxNegativeRunLength, frameCount - codeBook->lastAccessTime - 1 );
				codeBook->lastAccessTime = frameCount;

				if( inputIllumination < codeBook->minIllumination  )
				{
					codeBook->minIllumination = inputIllumination;
				}
				else if( inputIllumination > codeBook->maxIllumination )
				{
					codeBook->maxIllumination = inputIllumination;
				}

				double oldFrequency = codeBook->frequency;
				double frequency = ++( codeBook->frequency );

				codeBook->red = ( oldFrequency * codeBook->red + inputRed ) / frequency;
				codeBook->green = ( oldFrequency * codeBook->green + inputGreen ) / frequency;
				codeBook->blue = ( oldFrequency * codeBook->blue + inputBlue ) / frequency;

				maskImageData[ 3*x ] = 0;
				//++maskImageData;
				maskImageData[ 3*x+1 ] = 0;
				//++maskImageData;
				maskImageData[ 3*x+2 ] = 0;
			}

			codeBook->next = codeBooks[ i ];
			codeBooks[ i ] = codeBook;
			if( codeBook == codeBook->next )
			{
				codeBook->next = 0;
			}

			++i;
		}

		imageData += imageDataDisplacement;
		maskImageData += maskImageDataDisplacement;
	}
}

// *****************************************************************************

void CodeBookModel::finishTraining( uint maxNegativeRunLengthThreshold, double RLEThreshold  )
{
	if( maxNegativeRunLengthThreshold == 0 )
	{
		maxNegativeRunLengthThreshold = frameCount * 0.8; //>> 1;
	}

	uint codeBookCount = imageSize.width * imageSize.height;
	for( uint i = 0; i < codeBookCount; ++i )
	{
		CodeWord** lastNextPointer = &( codeBooks[ i ] );
		CodeWord* codeBook = *lastNextPointer;

		std::vector<double> intensities;
		std::vector<CodeWord*> tmpCodeWords;

		while( codeBook != 0 )
		{
			codeBook->maxNegativeRunLength = std::max( codeBook->maxNegativeRunLength, frameCount - codeBook->lastAccessTime + codeBook->firstAccessTime - 1 );
			if( codeBook->maxNegativeRunLength >= maxNegativeRunLengthThreshold )
			{
				*lastNextPointer = codeBook->next;
			}
			else {
				lastNextPointer = &(codeBook->next);
				intensities.push_back ( sqrt(codeBook->red*codeBook->red + codeBook->blue*codeBook->blue +
												codeBook->green*codeBook->green ) );
				tmpCodeWords.push_back ( codeBook );
			}

			codeBook = codeBook->next;
		}

		// check variance of background --> if too big remove
		if ( intensities.size () > 1 )
		{
			std::vector<double>::const_iterator iter_max = std::max_element ( intensities.begin(), intensities.end() );
			std::vector<double>::const_iterator iter_min = std::min_element ( intensities.begin(), intensities.end() );
	
			if ( i % imageSize.width == 60 && i / imageSize.width == 64 )
					int peter = 13;

		    if ( *iter_max - *iter_min > 128 ) {	
				
				double d = (*iter_max+*iter_min) / sqrt(3.0) * 0.5;
				uint minRLE = frameCount;
				uint maxRLE = frameCount;

				for ( int k =0, sz = intensities.size(); k < sz; ++k) {
					if ( intensities[k] < d && tmpCodeWords[k]->maxNegativeRunLength < minRLE )
						minRLE = tmpCodeWords[k]->maxNegativeRunLength;
					if ( intensities[k] > d && tmpCodeWords[k]->maxNegativeRunLength < maxRLE )
						maxRLE = tmpCodeWords[k]->maxNegativeRunLength;
				}

				if ( minRLE < frameCount * RLEThreshold || maxRLE < frameCount * RLEThreshold )
					codeBooks[i] = 0;

				//   codeBooks[i] = 0;
			}
		}

	}
}