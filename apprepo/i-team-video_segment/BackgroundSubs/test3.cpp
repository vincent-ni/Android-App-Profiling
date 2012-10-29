// test3.cpp : Definiert den Einstiegspunkt für die Konsolenanwendung.
//

#include <stdio.h>
#include <string>
#include <iostream>
#include <cv.h>
#include <highgui.h>
#include "AviReader.h"
#include "cvCodeBook.h"

using namespace std;


int main(int argc, char** argv)
{

  string video_path = argv[1];
  string bg_train_path;

  AviReader* avi_reader = new AviReader(video_path);

  AviReader* bgavi_reader =0;

  if ( argc > 2 )
  {
	  bg_train_path = argv[2];
	  if (!bg_train_path.empty())
		bgavi_reader = new AviReader(bg_train_path);
  }
  
  //train background of video
  int act_img_dummy;
  const IplImage *image = avi_reader->getActualImage(act_img_dummy);

  // standard values ...
  double alpha = 0.5;
  double beta = 1.1;
  double colorTresh = 10;
  double RLEThreshold = 0.3;

  if( argc > 5)
  {
	  RLEThreshold = atof(argv[3]);
	  alpha = atof(argv[4]);
	  beta = atof(argv[5]);
  }

  CodeBookModel *cb = new CodeBookModel(cvSize(image->width, image->height), colorTresh, alpha, beta);
  IplImage* fg_image = cvCreateImage(cvGetSize(image), image->depth, image->nChannels);

  unsigned int frames = 10;
  int gaussSz = 3;
 
  // train without background video
  /*if ( !bgavi_reader ) 
  {
	  for (unsigned int i=0; i < frames; i++)
	  {
		avi_reader->setImage(1);
		image = avi_reader->getActualImage(act_img_dummy);
		cb->train(image, fg_image);
	  }

	  for (unsigned int i=0; i < frames; i++)
	  {
		avi_reader->setImage(avi_reader->getFrameCount()/2);
		image = avi_reader->getActualImage(act_img_dummy);
		cb->train(image, fg_image);
	  }

	  for (unsigned int i=0; i < frames; i++)
	  {
		avi_reader->setImage(avi_reader->getFrameCount()-1);
		image = avi_reader->getActualImage(act_img_dummy);
		cb->train(image, fg_image);
	  }
  }*/
  if ( !bgavi_reader ) 
  {
	  for (unsigned int i=0; i < avi_reader->getFrameCount()-1; ++i)
	  {
			avi_reader->setImage(i);
			image = avi_reader->getActualImage(act_img_dummy);
			cb->train(image, fg_image);
	  }
  }
  else
  {
		for (int i=0; i < bgavi_reader->getFrameCount(); ++i)
		{
			bgavi_reader->setImage(i);
		    image = bgavi_reader->getActualImage(act_img_dummy);

			cb->train(image, fg_image);
		}
  }
 
  cb->finishTraining(0,RLEThreshold);
  avi_reader->setImage(0);
  
  std::cout << "Trained all input images\n";

  //cvNamedWindow("CameraCapture", 1);
  
  
  double ms_acc = 0;
  for (unsigned act_frame =0; act_frame < avi_reader->getFrameCount(); ++act_frame)
  {
	char filename[128];
	sprintf(filename,"frame_%03d.png", act_frame);

    const IplImage* frame = 0;

	avi_reader->setImage(act_frame);
	frame = avi_reader->getActualImage(act_img_dummy);
	
	IplImage *test = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
	if(!frame)
      break;

	IplImage* fg_image = cvCreateImage(cvGetSize(frame), frame->depth,
                                     frame->nChannels);

	cb->detectNonAdapting(frame, fg_image);
	//cb->detect(frame, fg_image);
	IplConvKernel* kernel = cvCreateStructuringElementEx( 3, 3, 1, 1, CV_SHAPE_ELLIPSE );
	IplImage* temp_image = cvCreateImage(cvSize(fg_image->width, fg_image->height), IPL_DEPTH_8U, 1);
	
	IplImage* temp2_image = cvCreateImage(cvGetSize(frame), frame->depth,
                                     frame->nChannels);

	cvMorphologyEx(fg_image, temp2_image, temp_image, kernel, CV_MOP_OPEN, 1);

	cvDilate(temp2_image, fg_image, kernel, 1); // grow contour
	cvReleaseImage(&temp_image);
	cvReleaseImage(&temp2_image);
	cvReleaseStructuringElement(&kernel);

	avi_reader->convert3Cto1C(fg_image,test);
  IplImage* save_image = cvCreateImage(cvSize(fg_image->width, fg_image->height), IPL_DEPTH_8U, 3);
  cvCvtColor(avi_reader->findContours(test), save_image, CV_GRAY2BGR);
	cvSaveImage(filename, save_image);
  cvReleaseImage(&save_image);

    cvReleaseImage(&fg_image);
	//cvReleaseImage(&test);
	//cvReleaseImage((IplImage**)&frame);
  }

  //cvDestroyWindow("CameraCapture");
  return -1;
}

