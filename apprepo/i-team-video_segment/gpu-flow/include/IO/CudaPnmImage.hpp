/*
 * Copyright (c) ICG. All rights reserved.
 *
 * Institute for Computer Graphics and Vision
 * Graz University of Technology / Austria
 *
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the above copyright notices for more information.
 *
 *
 * Project     : Pnm image class for CUDA Templates
 * Module      :
 * Class       : $RCSfile$
 * Language    : C++/CUDA
 * Description :
 *
 * Author     :
 * EMail      :
 *
 */

#ifndef CUDAPNMIMAGE_H
#define CUDAPNMIMAGE_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

using namespace std;

namespace CommonLib{

//////////////////////////////////////////////////////////////////////////////////
//! PNM image reading class for CUDA Templates.
//!
//! Reads gray value pnm and pgm 8bit images. The template parameter specifies the
//! type of the accessible data.
//! @todo add 16bit and color support
//////////////////////////////////////////////////////////////////////////////////
template <class Type>
class CudaPnmImage
{
  public:
    ////////////////////////////////////////////////////////////////////////////////
    //! Construct an image by loading it from a file.
    //! @param[in] fname   Name of file to be read
    ////////////////////////////////////////////////////////////////////////////////
    CudaPnmImage(const char *fname):
       width(-1), height(-1), bit_depth(8),
       data_char(NULL), data_sint(NULL), data(NULL), hostRef(NULL)
    {
      ifstream src(fname, ios::binary);

      if(!src)
      {
        cerr << "CudaPnmImage::CudaPnmImage(): Couldn't open image file \"" << fname << "\"\n";
        return;
      }

      skipComment(src);
      string type;
      src >> type >> ws;

      if(type == "P5")
        planes = 1;
      else if(type == "P6")
      {
        planes = 3;
        cerr << "Currently not supported" << endl;
      }
      else
      {
        cerr << "CudaPnmImage::CudaPnmImage(): File \"" << fname
            << "\" is not a binary gray or color image (PPM type P5 or P6)\n";
        return;
      }

      skipComment(src);
      int maxval;
      src >> width >> height >> maxval;

      if(maxval == 255)
        bit_depth = 8;
      else if (maxval == 65535)
        bit_depth = 16;
      else
      {
        cerr << "CudaPnmImage::CudaPnmImage(): Only 8 or 16 bit per channel supported\n";
        return;
      }

      // Allocate memory
      data = new Type[width * height * planes];

      // Create CUDA image
      size = Cuda::Size<2>(width, height);
      hostRef = new Cuda::HostMemoryReference<Type, 2>(size, data);

      src.get();

      // allocate memory for 8 and 16bit
      data_char = new unsigned char[width * height * planes];
      if(data_char == NULL)
      {
        cerr << "CudaPnmImage::CudaPnmImage(): Couldn't allocate memory\n";
        return;
      }
      data_sint = new unsigned short int[width * height * planes];
      if(data_sint == NULL)
      {
        cerr << "CudaPnmImage::CudaPnmImage(): Couldn't allocate memory\n";
        return;
      }

      if (bit_depth == 8)
      {
        // Read image
        src.read((char *)data_char, width * height * planes);
        // Convert data to Cuda image
        convert(data, data_char);
      }

      if (bit_depth == 16)
      {
        // Read image
        src.read((char *)data_sint, width * height * planes * 2);
        // Convert data to Cuda image
        convert(data, (unsigned short int*)data_sint);
      }

    }


    ////////////////////////////////////////////////////////////////////////////////
    //! Construct an image with a specified size. The size can be given here as separate values.
    //! @param[in] w Width of image
    //! @param[in] h Height of image
    ////////////////////////////////////////////////////////////////////////////////
    CudaPnmImage(int w, int h):
        width(w), height(h), bit_depth(8),
        data_char(NULL), data_sint(NULL), data(NULL), hostRef(NULL)
    {
      data = new Type[width * height];
      size = Cuda::Size<2>(width, height);
      hostRef = new Cuda::HostMemoryReference<Type, 2>(size, data);
      data_char = new unsigned char[width * height * 3];
      data_sint = new unsigned short int[width * height * 3];
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Construct an image with a specified size.
    //! @param[in] newsize Size of image
    ////////////////////////////////////////////////////////////////////////////////
    CudaPnmImage(Cuda::Size<2> newsize):
        bit_depth(8), size(newsize),
        data_char(NULL), data_sint(NULL), data(NULL), hostRef(NULL)
    {
      width = size[0];
      height = size[1];
      planes = 1;
      data = new Type[width * height * planes];
      hostRef = new Cuda::HostMemoryReference<Type, 2>(size, data);
      data_char = new unsigned char[width * height * planes];
      data_sint = new unsigned short int[width * height * planes];
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Destructor. Cleans up everything.
    ////////////////////////////////////////////////////////////////////////////////
    ~CudaPnmImage()
    {
      if (data_char != NULL)
        delete [] data_char;
      if (data_sint != NULL)
        delete [] data_sint;
      if (data != NULL)
        delete [] data;
      delete hostRef;
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Returns a host memory reference to the internal data of the desired type.
    //! Also use this buffer to set data for writing to disc.
    //! @return Cudatemplates hostmemoryreference to image data
    ////////////////////////////////////////////////////////////////////////////////
    Cuda::HostMemoryReference<Type, 2> *CUDAhostRef() const { return hostRef; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Information on image height.
    //! @return Height of image
    ////////////////////////////////////////////////////////////////////////////////
    int getHeight() const { return height; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Information on image width.
    //! @return Width of image
    ////////////////////////////////////////////////////////////////////////////////
    int getWidth() const { return width; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Information on image planes.
    //! @return Planes of image
    ////////////////////////////////////////////////////////////////////////////////
    int getPlanes() const { return planes; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Information on image bit depth.
    //! @return Bit depth of image
    ////////////////////////////////////////////////////////////////////////////////
    int getBitDepth() const { return bit_depth; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Information on size of the image.
    //! @return Size of image
    ////////////////////////////////////////////////////////////////////////////////
    Cuda::Size<2> getSize() const { return size; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Set bit depth of the image.
    //! @param value Bit depth of image (8 or 16)
    ////////////////////////////////////////////////////////////////////////////////
    void setBitDepth(int value) { bit_depth = value; }

    ////////////////////////////////////////////////////////////////////////////////
    //! Write current image to disk.
    //! @param fname Name of file to be written to.
    ////////////////////////////////////////////////////////////////////////////////
    void write(const char *fname)
    {
      // Convert image
      if (bit_depth == 8)
        convert(data_char, data);
      if (bit_depth == 16)
        convert(data_sint, data);

      // determine image type
      int type = 0;
      switch(planes)
      {
        case 1: type = 5; break;
        case 3: type = 6; break;
        default: cerr << "CudaPnmImage::write(): Can only write images with 1 or 3 planes\n"; return;
      }

      // Create output file
      ofstream dst(fname, ios::binary);
      if(!dst)
      {
        cerr << "CudaPnmImage::write(): Couldn't open image file \"" << fname << "\"\n";
        return;
      }

      // write to disk
      if (bit_depth == 8)
      {
        dst << 'P' << type << endl << width << ' ' << height << " 255\n";
        dst.write((char *)data_char, width * height * planes);
      }
      if (bit_depth == 16)
      {
        dst << 'P' << type << endl << width << ' ' << height << " 65535\n";
        dst.write((char *)data_sint, width * height * planes * 2);
      }
    }

  private:
    ////////////////////////////////////////////////////////////////////////////////
    //! Used for parsing input image. Skips over an comment.
    //! @param src Input stream
    ////////////////////////////////////////////////////////////////////////////////
	void skipComment(istream &src)
    {
      while(src.peek() == '#')
        src.ignore(1000000, '\n');
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: unsigned char => float
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(float* data_out, unsigned char* data_in)
    {
      if (planes == 1)  // Grayscale image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
            data_out[y*width+x] = data_in[y*width+x]/255.0f;
          }
        }
      }
      else  // RGB image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
            float value = 0.0f;
            for (int p=0; p<3; p++)
            {
              value += data_in[(y*width+x)*3+p]/255.0f;
            }
            data_out[y*width+x] = value/3.0f;
          }
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: unsigned short int => float
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(float* data_out, unsigned short int* data_in)
    {
      if (planes == 1)  // Grayscale image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
			unsigned short int in_value = ((data_in[y*width+x] & 0x00ffU) << 8) | ((data_in[y*width+x] & 0xff00U) >> 8);
            data_out[y*width+x] = in_value/65535.0f;
          }
        }
      }
      else  // RGB image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
            float value = 0.0f;
            for (int p=0; p<3; p++)
            {
		      unsigned short int in_value = ((data_in[(y*width+x)*3+p] & 0x00ffU) << 8) | ((data_in[(y*width+x)*3+p] & 0xff00U) >> 8);
              value += in_value/65535.0f;
            }
            data_out[y*width+x] = value/3.0f;
          }
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: unsigned char => float3
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(float3* data_out, unsigned char* data_in)
    {
      if (planes == 1)  // Grayscale image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
            float value = data_in[y*width+x]/255.0f;
            data_out[y*width+x] = make_float3(value, value, value);
          }
        }
      }
      else  // RGB image
      {
        for (int x=0; x<width; x++)
        {
          for (int y=0; y<height; y++)
          {
            float r = data_in[(y*width+x)*3]/255.0f;
            float g = data_in[(y*width+x)*3+1]/255.0f;
            float b = data_in[(y*width+x)*3+2]/255.0f;
            data_out[y*width+x] = make_float3(r, g, b);
          }
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: float => unsigned char
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(unsigned char* data_out, float* data_in)
    {
      planes = 1;

      for (int x=0; x<width; x++)
        for (int y=0; y<height; y++)
          data_out[y*width+x] = (unsigned char)(data_in[y*width+x]*255.0f);
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: float => unsigned short int
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(unsigned short int* data_out, float* data_in)
    {
      planes = 1;

      for (int x=0; x<width; x++)
	  {
        for (int y=0; y<height; y++)
		{
		  unsigned short int in_value = (unsigned short int)(data_in[y*width+x]*65535.0f);
          data_out[y*width+x] = ((in_value & 0x00ffU) << 8) | ((in_value & 0xff00U) >> 8);
		}
	  }
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: float3 => unsigned char
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(unsigned char* data_out, float3* data_in)
    {
      planes = 3;

      for (int x=0; x<width; x++)
      {
        for (int y=0; y<height; y++)
        {
          data_out[(y*width+x)*3] = (unsigned char)(data_in[y*width+x].x*255.0f);
          data_out[(y*width+x)*3+1] = (unsigned char)(data_in[y*width+x].y*255.0f);
          data_out[(y*width+x)*3+2] = (unsigned char)(data_in[y*width+x].z*255.0f);
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    //! Convert image data: float3 => short unsigned int
    //! @param[out] data_out Output data
    //! @param[in] data_in Input data
    ////////////////////////////////////////////////////////////////////////////////
    void convert(short unsigned int* data_out, float3* data_in)
    {
      planes = 3;

      for (int x=0; x<width; x++)
      {
        for (int y=0; y<height; y++)
        {
		  unsigned short int in_value_x = (unsigned short int)(data_in[y*width+x].x*65535.0f);
		  unsigned short int in_value_y = (unsigned short int)(data_in[y*width+x].y*65535.0f);
		  unsigned short int in_value_z = (unsigned short int)(data_in[y*width+x].z*65535.0f);

          data_out[(y*width+x)*3] = ((in_value_x & 0x00ffU) << 8) | ((in_value_x & 0xff00U) >> 8);
          data_out[(y*width+x)*3+1] = ((in_value_y & 0x00ffU) << 8) | ((in_value_y & 0xff00U) >> 8);
          data_out[(y*width+x)*3+2] = ((in_value_z & 0x00ffU) << 8) | ((in_value_z & 0xff00U) >> 8);
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Variables
    ////////////////////////////////////////////////////////////////////////////////

    // internal size variables
    int width, height, planes;
    int bit_depth;
    Cuda::Size<2> size;

    // data buffers & images
    unsigned char *data_char;
    unsigned short int *data_sint;
    Type *data;
    Cuda::HostMemoryReference<Type, 2> *hostRef;
};

}

#endif //CUDAPNMIMAGE_H
