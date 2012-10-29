#include <stdio.h>
#include <iostream>
#include "AviReader.h"
#include <cmath>

using namespace std;

// ************************************************************************** //

AviReader::AviReader(string video_path) : m_capture(NULL), m_image(0),
m_width(0), m_height(0), m_frame(0), m_start_frame(0), m_end_frame(0), m_video_mode(true)
{
  // open video
  m_capture = cvCaptureFromAVI(video_path.c_str());
  if (m_capture == NULL)
  {
    cout << "WARNING: Could not open video file " << video_path << endl;
  }
  else
  {
    m_image = cvQueryFrame(m_capture);
    m_width = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_FRAME_WIDTH);
    m_height = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_FRAME_HEIGHT);
    int frames = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_FRAME_COUNT);
    m_start_frame = 0;
    m_end_frame = frames - 1;
    cout << "Opened video file " << video_path << ":" << endl
         << "  frames: " << m_end_frame+1 << "  width: " << m_width << "  height: "
         << m_height << endl;
  }
}

// ************************************************************************** //

AviReader::AviReader(string prefix, string suffix, int start_frame, int end_frame) 
: m_capture(NULL), m_image(0), m_width(0), m_height(0), m_frame(0), m_start_frame(start_frame), m_end_frame(end_frame), m_images_prefix(prefix), m_images_suffix(suffix), m_video_mode(false)
{
  // open first video image
  char str[10];
  sprintf(str, "%05d", m_start_frame);
  string image_filename = m_images_prefix + str + m_images_suffix;
  m_image = cvLoadImage(image_filename.c_str());
  if (!m_image)
  {
    cerr << "ERROR: Couldn't open image " << image_filename << endl;
    exit(1);
  }
  m_width = m_image->width;
  m_height = m_image->height;
}

// ************************************************************************** //

AviReader::~AviReader()
{
  if (m_capture != NULL)
    cvReleaseCapture(&m_capture);
  if (m_image != NULL && !m_video_mode)
    cvReleaseImage(&m_image);
}

// ************************************************************************** //

bool AviReader::setImage(int frame)
{
  frame += m_start_frame;
  if (frame < m_start_frame || frame > m_end_frame)
    return false;
  if (m_video_mode)
  {
    if (m_capture == NULL)
      return false;
    cvSetCaptureProperty(m_capture, CV_CAP_PROP_POS_FRAMES, (double)frame);
    m_image = cvQueryFrame(m_capture);
  }
  else
  {
    char str[10];
    sprintf(str, "%05d", frame);
    string image_filename = m_images_prefix + str + m_images_suffix;
    if (m_image)
      cvReleaseImage(&m_image);
    m_image = cvLoadImage(image_filename.c_str());
  }
  m_frame = frame - m_start_frame;
  return true;
}

// ************************************************************************** //

bool AviReader::setNextImage()
{
  if (m_video_mode)
  {
    if (m_capture == NULL)
      return false;
    // extra check actual frame as sometimes cvCapture is not frame accurate!
    int act_frame = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_POS_FRAMES);
    if (act_frame >= m_end_frame)
      return false;
    m_image = cvQueryFrame(m_capture);
    m_frame = (act_frame - m_start_frame) + 1;
  }
  else
  {
    if ((m_frame + m_start_frame) >= m_end_frame)
      return false;
    m_frame++;
    char str[10];
    sprintf(str, "%05d", m_start_frame + m_frame);
    string image_filename = m_images_prefix + str + m_images_suffix;
    if (m_image)
      cvReleaseImage(&m_image);
    m_image = cvLoadImage(image_filename.c_str());
  }
  return true;
}

// ************************************************************************** //

bool AviReader::setPreviousImage()
{
  if (m_video_mode)
  {
    if (m_capture == NULL)
      return false;
    // extra check actual frame as sometimes cvCapture is not frame accurate!
    int act_frame = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_POS_FRAMES);
    if (act_frame <= m_start_frame)
      return false;
    cvSetCaptureProperty(m_capture, CV_CAP_PROP_POS_FRAMES, (double)(act_frame - 1));
    m_image = cvQueryFrame(m_capture);
    m_frame = (act_frame - m_start_frame) - 1;
  }
  else
  {
    if (m_frame <= 0)
      return false;
    m_frame--;
    char str[10];
    sprintf(str, "%05d", m_start_frame + m_frame);
    string image_filename = m_images_prefix + str + m_images_suffix;
    if (m_image)
      cvReleaseImage(&m_image);
    m_image = cvLoadImage(image_filename.c_str());
  }
  return true;
}

// ************************************************************************** //

const IplImage* AviReader::getActualImage(int& frame) const
{
  // frame = (int)cvGetCaptureProperty(m_capture, CV_CAP_PROP_POS_FRAMES);
  frame = m_frame;
  return m_image;
}

// ************************************************************************** //

void AviReader::getImageSize(int& width, int& height) const
{
  width = (int)m_width;
  height = (int)m_height;
}

// ************************************************************************** //

int AviReader::getFrameCount() const
{
  return (m_end_frame - m_start_frame) + 1;
}

void AviReader::convert3Cto1C(const IplImage* img3c, IplImage* img1c)
{

  unsigned char* data3c = (unsigned char*)img3c->imageData;
  unsigned char* data1c = (unsigned char*)img1c->imageData;
  int padding = img3c->widthStep - (img3c->width * img3c->nChannels);
  int padding2 = img1c->widthStep - (img1c->width * img1c->nChannels);
  int pos=0;
  int pos2=0;
  unsigned char value=0;
  for (int y=0; y<img3c->height; y++)
  {
    for (int x=0; x<img3c->width; x++)
    {
      value = data3c[pos];
      data1c[pos2] = value;

      pos += img3c->nChannels;
      pos2+= img1c->nChannels;
    }
    pos += padding;
    pos2 += padding2;
  }

}

float AviReader::contourDist ( CvSeq* first, CvSeq* second) const
{
	// find closest points
	int dist = 1 << 30;
	CvPoint* p1, *p2;
	int diffx;
	int diffy;

	for ( int i=0; i < first->total; ++i )
	{
		p1 = CV_GET_SEQ_ELEM ( CvPoint, first, i );
		for ( int j=i; j < second->total; ++j)
		{
			p2 = CV_GET_SEQ_ELEM ( CvPoint, second, j);
			diffx = p1->x - p2->x;
			diffy = p1->y - p2->y;

			dist = min ( dist, diffx*diffx+diffy*diffy);
		}
	}

	return std::sqrt ( (float) dist );

}

IplImage* AviReader::findContours(IplImage* binaryImage, bool noOutput)
{
  IplImage* dst = cvCreateImage( cvGetSize(binaryImage), 8, 1 );
  cvThreshold( binaryImage, binaryImage, 1, 255, CV_THRESH_BINARY );
  CvMemStorage* storage = cvCreateMemStorage(0);
  CvSeq* contour = 0;
  CvSeq* maxContour = NULL;
  // cvFindContours( binaryImage, storage, &contour, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE );
  cvFindContours( binaryImage, storage, &contour, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE );
  cvZero( dst );
  double area = 0, maxArea = 0;

  CvSeq* traverse = contour;

  while ( traverse )
  {
	  area = fabs(cvContourArea(traverse));
	
	  if (area > maxArea)
	  {
		maxArea = area;
		maxContour = traverse;
	  }

	  traverse = traverse->h_next;
  }

  // draw maxContour
  CvScalar color = CV_RGB( 255, 255, 255 );
  CvScalar color2 = CV_RGB( 0, 0, 0 );
  //cvDrawContours( dst, maxContour, color, color2, 0, 1, 8 );
  
  /// magic ... (7^2*pi)
 // if ( maxArea > 154) {
      cvDrawContours( dst, maxContour, color, color2, 0, CV_FILLED, 8); // draw filled
  
	  // draw all large neighbouring contours that are close to maxContour
	  traverse = contour;
	  while ( traverse )
	  {
		if (traverse == maxContour) 
		{
			traverse = traverse->h_next;
			continue;
		}

		if ( fabs(cvContourArea(traverse)) > 60 )
		{
		//draw contour
			cvDrawContours( dst, traverse, color, color2, 0, CV_FILLED, 8);
		//	std::cout << " fragment size " << fabs(cvContourArea(traverse)) << std::endl;
		}

		traverse = traverse->h_next;
	//  }
  }

  cvReleaseImage(&binaryImage);
  return dst;
}
// ***************************** END OF FILE ******************************** //
