#ifndef __AVI_READER_H__
#define __AVI_READER_H__
#include <string>
#include "cv.h"
#include "highgui.h"

class AviReader
{
  
  protected:
    
    CvCapture* m_capture;     // OpenCV capture device
    IplImage* m_image;        // actual image
    int m_width;              // video width
    int m_height;             // video height
    
    int m_frame;              //!< actual frame
    int m_start_frame;        //!< synch frame for use with other cameras
    int m_end_frame;          //!< last frame
    std::string m_images_prefix;   //!< prefix of image filename (including path)
    std::string m_images_suffix;   //!< suffix of image filename (e.g. .jpg)
    bool m_video_mode;        //!< True if video mode, false if image mode
    
  public:
    
    // constructors
    AviReader(std::string video_path);
    AviReader(std::string prefix, std::string suffix, int start_frame, int end_frame);
    
    // destructor
    ~AviReader();
    
    // video related methods (return image pointers, should be copied, do not delete)
    bool setImage(int frame);
    bool setNextImage();
    bool setPreviousImage();
    const IplImage* getActualImage(int& frame) const;
    void getImageSize(int& width, int& height) const;
    int getFrameCount() const;
	void convert3Cto1C(const IplImage* img3c, IplImage* img1c);
	IplImage* findContours(IplImage* binaryImage, bool noOutput = true);

	float	contourDist ( CvSeq* first, CvSeq* second) const;
    
};

#endif
