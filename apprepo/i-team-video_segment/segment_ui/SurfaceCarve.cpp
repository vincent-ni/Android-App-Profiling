#include "SurfaceCarve.h"

#include <ippi.h>
#include <ippcv.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <QApplication>
#include <QMessageBox>

#include <stdexcept>
#include <fstream> 
#include "movieWidget.h"

#include <functional>
#include <boost/lexical_cast.hpp>

#include "warping.h"
#include "highgui.h"
#include "image_util.h"
using namespace ImageFilter;

SurfaceCarve::SurfaceCarve(const VideoData *data, int percentage, float temporal_weight,
                           float forward_weight, float saliency_weight, bool blend,
                           int n_window, const QString& flow_file_) :
    _seams(percentage * data->getWidth () / 100), _video(data), _saliency_map(0), _user_input(0), 
    blend_mode_(blend), temporal_weight_(temporal_weight), forward_weight_(forward_weight),
    saliency_weight_(saliency_weight), neighbor_window_(n_window),
    flow_file_(flow_file_),
    _thresholdMap(data->getWidth(), data->getHeight(), data->getFrames()),
    _blendImg(data->getWidth(), data->getHeight(), 1)  { 
	_tmpImg = ippiMalloc_8u_C4 ( data->getWidth() + _seams, data->getHeight(), &_steps);
	_mask = ippiMalloc_8u_C1 ( data->getWidth(), data->getHeight(), &_maskStep);
  
  temporal_threshold_ = 200;
  min_temporal_weight_ = 0.2;
  max_temporal_weight_ = 5;
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

	// Allocate 3D Matrix.
	const int sx = _video->getWidth();
	const int sy = _video->getHeight();
	const int st = _video->getFrames ();

  // Used for conversion to grayscale.
	Matrix3D rgb (sx, sy, 3);
  Matrix3D rgb_prev (sx, sy, 3);
  
  // Plane indices 0123 correspond to BGRA in video.
	Ipp8u* plane[4];
	int	   planeStep[4];
  
  for ( int i=0; i < 4; ++i ) {
		plane[i] = ippiMalloc_8u_C1 ( sx, sy, &planeStep[i] );
	}
  
	IppiSize imgSz = { sx, sy };
  // For RGB in this order.
	float  colCoeffs [] = { 0.2989, 0.5870, 0.1140 };

  const int multi_min_sz = 5000;
  const float multi_scale = 0.5;
  
  int multi_cur_width = sx;
  int multi_cur_height = sy;
  
  // Gradient information.
  vector<Matrix3DPtr> graX;
  vector<Matrix3DPtr> graY;
  
   // Current frame in gray-scale.
  vector<Matrix3DPtr> gray_scale;
  
  // Current frame as 4 layer uchar.
  // Used for spatial and temporal coherence cost.
  vector<Matrix3DPtr> color_frame;
  
  // Saliency and external saliency map.
  vector<Matrix3DPtr> saliency;
  
  vector<Matrix3DPtr> color_frame_pyr;
  vector<Matrix3DPtr> saliency_pyr;
  
  // Temporary matrices.
  vector<Matrix3DPtr> tmp_mat;
  vector<Matrix3DPtr> seam_offset;  
  
  vector<Matrix3DPtr> temporal_constraint;
  
  // Multi-Level flow is not supported of now.
  Matrix3D flow(sx, sy, 2);
  
  // Holds current, previous frame and carved current frame as 32bit floats.
  Matrix3D color_frames_32f(sx * 3, sy, 3);
  
  // Warped image and warping error.
  Matrix3D warped_frame_32f(sx * 3, sy, 1);
  Matrix3D warping_error(sx * 3, sy, 2);
  Matrix3D frame_plus_error(sx * 3, sy, 1);
  
  // Flow file.
  std::ifstream ifs_flow;
  if (flow_file_.size()) {
    ifs_flow.open(flow_file_.toAscii(), std::ios_base::in | std::ios_base::binary);
    
    int width, height, flow_type;
    ifs_flow.read((char*)&width, sizeof(width));
    ifs_flow.read((char*)&height, sizeof(height));
    ifs_flow.read((char*)&flow_type, sizeof(flow_type));
    
    // forward flow required.
    if (width != sx || height != sy || flow_type != 0) {
      QMessageBox::critical (0, QString(), QString("Flow file ") + flow_file_ +
                             " is not compatible with video file.");
      return;
    }
    
    std::cout << "Using dense optical flow for seam tracking.";
  }
  
  while (multi_cur_width > multi_min_sz ||
         graX.size() == 0) {                  // guarantee at least one entry.
    graX.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));
    graY.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));
    gray_scale.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));
    color_frame.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 2)));
    saliency.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 2)));

    color_frame_pyr.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));
    saliency_pyr.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));

    temporal_constraint.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 1)));
    
    tmp_mat.push_back(Matrix3DPtr(new Matrix3D(multi_cur_width, multi_cur_height, 10)));
    seam_offset.push_back(Matrix3DPtr(new Matrix3D(multi_cur_height, st, _seams)));
        
    multi_cur_width *= multi_scale;
    multi_cur_height *= multi_scale;
  }
  
  const int multi_levels = saliency.size();
  
  // Filter buffer.
  int bufferSzPre[2];  
  ippiFilterSobelVertGetBufferSize_32f_C1R (imgSz, ippMskSize3x3, &bufferSzPre[0]);
	ippiFilterSobelHorizGetBufferSize_32f_C1R (imgSz, ippMskSize3x3, &bufferSzPre[1]);
	const int bufferSz = std::max(bufferSzPre[0], bufferSzPre[1]);
  int dummy;
  
  // Buffer used for gradient computation.
  Ipp8u* graBuffer = ippiMalloc_8u_C1 (bufferSz, 1, &dummy );
  
  if(_saliency_map)
    std::cout << "SeamCarving: uses external saliency!\n";
  else if (_user_input)
    std::cout << "SeamCarving: uses user input!\n";
  
  cvNamedWindow("carving_window");
  cvWaitKey(1000);
  
  // Process all frames.
  for (int t = 0; t < st; ++t) {
    emit done(t);
    if (_isCanceled ) {
      emit done(st);
      for (int i=0; i < 4; ++i) ippiFree (plane[i]);
      ippiFree (graBuffer);
      return;
    }
    
    // Keep previous image.
    if (t > 0) {
      ippiCopy_32f_C3R(color_frames_32f.img(0), color_frames_32f.step(),
                       color_frames_32f.img(1), color_frames_32f.step(),
                       imgSz);
    }
    
    // Convert current frame to gray-scale float and 32bit RGB float.
    ippiCopy_8u_C4P4R((Ipp8u*)_video->getData()[t]->getData(), sx*sizeof(int), plane,
                      planeStep[0], imgSz);

		for (int col=0; col < 3; ++col) {
			ippiConvert_8u32f_C1R (plane[2-col], planeStep[0], rgb.img(col), rgb.step(), imgSz ); 
		}
    
    float* rgb_planes[] = {rgb.img(2), rgb.img(1), rgb.img(0)};
    
    ippiCopy_32f_P3C3R(rgb_planes, rgb.step(), color_frames_32f.img(0), color_frames_32f.step(),
                       imgSz);
    ippiCopy_32f_C3R(color_frames_32f.img(0), color_frames_32f.step(),
                     color_frames_32f.img(2), color_frames_32f.step(), imgSz);
    
		for (int col=0; col < 3; ++col)
      ippiMulC_32f_C1IR ( colCoeffs[col], rgb.img(col), rgb.step(), imgSz);
    
    // Add for grayscale.
		ippiAdd_32f_C1IR(rgb.img(1), rgb.step(), rgb.img(0), rgb.step(), imgSz);
		ippiAdd_32f_C1R(rgb.img(0), rgb.step(), rgb.img(2), rgb.step(), gray_scale[0]->d(),
                    gray_scale[0]->step(), imgSz);
    
    // Load current frame.
    ippiCopy_32s_C1R (_video->getData()[t]->getData(),
                      sx*sizeof(int), 
                      (int*)color_frame[0]->img(0),
                      color_frame[0]->step(),
                      imgSz);
    
    // Load dense optical flow if possible.
    if (t > 0 && flow_file_.size()) {
      for (int i = 0; i < 2 * sy; ++i) {
        ifs_flow.read((char*)flow.at(0, i, 0), sizeof(float) * sx);
      }
    }

    if(_saliency_map) {
      // Load into graX.
      if (_saliency_map->getFrames() != _video->getFrames())
        throw std::runtime_error(
           "SurfaceCarve: Saliency and VideoData differ in number of frames\n");
      
      // Average over the next frames.
      float weight = 1.0; 
      ippiSet_32f_C1R(0.0, saliency[0]->img(1), saliency[0]->step(), imgSz);
      
      for (int k = t; k < std::min(t + 8, st); ++k) {
        weight *= 0.7;

        // Only use one channel, its an intensity video anyways.
        ippiCopy_8u_C4P4R((Ipp8u*)_saliency_map->getData()[t]->getData(), sx*sizeof(int), plane,
                          planeStep[0], imgSz);
        
        ippiConvert_8u32f_C1R (plane[0], planeStep[0], tmp_mat[0]->d(), tmp_mat[0]->step(), imgSz); 
        ippiMulC_32f_C1IR(weight, tmp_mat[0]->d(), tmp_mat[0]->step(), imgSz);
        ippiAdd_32f_C1IR(tmp_mat[0]->d(), tmp_mat[0]->step(),
                         saliency[0]->img(1), saliency[0]->step(), imgSz);
      }
      
      ippiMulC_32f_C1IR(saliency_weight_, saliency[0]->img(1), saliency[0]->step(), imgSz); 
    } else if (_user_input) {      
      float erase = -255.0 * 2;
      float maintain = 255.0 * 2;
  
      ippiSet_32f_C1R(0.0, saliency[0]->img(1), saliency[0]->step(), imgSz);
      
      for (int k = t; k < std::min(t + 8, st); ++k) {
        erase *= 0.7;
        maintain *= 0.7;
        
        int erase_int = *reinterpret_cast<int*>(&erase);
        int maintain_int = *reinterpret_cast<int*>(&maintain);
        
        _user_input->RenderRegionFrame(k, erase_int, maintain_int);        
        
        ippiAdd_32f_C1IR ((float*) _user_input->GetRegionFrame(), sx*sizeof(float),
                          saliency[0]->img(1), saliency[0]->step(), imgSz);
      }
      
      ippiMulC_32f_C1IR(saliency_weight_, saliency[0]->img(1), saliency[0]->step(), imgSz); 
    }
        
    // Process all seams.
    for (int seam_num = 0; seam_num < _seams; ++seam_num) {

      // Resize grayscale, color frame and external saliency after each seam carving operation.        
      IppiSize src_roi = { sx - seam_num, sy };
      vector<int> level_width(multi_levels);
      vector<int> level_height(multi_levels);
      level_width[0] = src_roi.width;
      level_height[0] = src_roi.height;
      
      for (int l = 1; l < multi_levels; ++l) {
        IppiSize dst_roi = {src_roi.width * multi_scale, src_roi.height * multi_scale};
        IppiRect src_rect = {0, 0, src_roi.width, src_roi.height};
        level_width[l] = dst_roi.width;
        level_height[l] = dst_roi.height;
        
        ippiResize_32f_C1R(gray_scale[l-1]->d(), src_roi, gray_scale[l-1]->step(), 
                           src_rect, gray_scale[l]->d(), gray_scale[l]->step(),
                           dst_roi, multi_scale, multi_scale, IPPI_INTER_LINEAR);

        ippiResize_8u_C4R((Ipp8u*)color_frame[l-1]->d(), src_roi, color_frame[l-1]->step(), 
                           src_rect, (Ipp8u*)color_frame[l]->d(), color_frame[l]->step(),
                           dst_roi, multi_scale, multi_scale, IPPI_INTER_LINEAR);
        
        if(_saliency_map || _user_input)
        {
          ippiResize_32f_C1R(saliency[l-1]->img(1), src_roi, saliency[l-1]->step(), 
                             src_rect, saliency[l]->img(1), saliency[l]->step(),
                             dst_roi, multi_scale, multi_scale, IPPI_INTER_LINEAR);          
        }
        
        src_roi = dst_roi;
      }
      
      // Prepare all levels first
      for (int level = 0; level < multi_levels; ++level) {
        IppiSize roi = {level_width[level], level_height[level]};
        
        ippiFilterSobelVertBorder_32f_C1R(gray_scale[level]->d(), gray_scale[level]->step(),
                                          graX[level]->d(), graX[level]->step(), roi, ippMskSize3x3,
                                          ippBorderRepl, 0, graBuffer);
          
        ippiFilterSobelHorizBorder_32f_C1R(gray_scale[level]->d(), gray_scale[level]->step(),
                                         graY[level]->d(), graY[level]->step (), roi, ippMskSize3x3,
                                         ippBorderRepl, 0, graBuffer);
      
        ippiAbs_32f_C1IR (graX[level]->d(), graX[level]->step(), roi );
        ippiAbs_32f_C1IR (graY[level]->d(), graY[level]->step(), roi );
        ippiAdd_32f_C1IR (graY[level]->d(), graY[level]->step(), 
                          graX[level]->d(), graX[level]->step(), roi);
        ippiMulC_32f_C1IR(1.0 / 8.0 * saliency_weight_, graX[level]->d(), graX[level]->step(), roi);
        
        // Use simple gradient saliency.
        if (!_saliency_map && !_user_input)
          ippiCopy_32f_C1R(graX[level]->d(), graX[level]->step(),
                           saliency[level]->d(), saliency[level]->step(), roi);
        else {
          ippiMulC_32f_C1IR(0.3, graX[level]->d(), graX[level]->step(), roi);
          ippiAdd_32f_C1R(graX[level]->d(), graX[level]->step(), saliency[level]->img(1),
                          saliency[level]->step(), saliency[level]->img(0), saliency[level]->step(),
                          roi);
        }
      }
      
      // Compute seam on base-level.
      const int last_level = multi_levels - 1;
      IppiSize last_level_roi = { level_width[last_level],
                                  level_height[last_level] };
      
      shared_ptr<IplImage> forward_warping_mask;
      
      vector<int> last_seam;
      FlowSeam flow_seam;
      
      if (t > 0) {
        if (flow_file_.size()) {
          // Compute warping error.
          IplImage flow_x, flow_y;
          // Comment
          IplImage input_image, reference_image, warped_image, error, warped_error;
          IplImage error_frame;
          
          flow.ImageView(&flow_x, 0, 1, level_width[last_level]);
          flow.ImageView(&flow_y, 1, 1, level_width[last_level]);
          // Comment
          color_frames_32f.ImageView(&input_image, 1, 3, level_width[last_level] * 3);
          color_frames_32f.ImageView(&reference_image, 2, 3, level_width[last_level] * 3);
          warped_frame_32f.ImageView(&warped_image, 0, 3, level_width[last_level] * 3);
          
          ImageFilter::ForwardWarp<float>(&input_image, &flow_x, &flow_y, &warped_image);
          
          warping_error.ImageView(&error, 0, 3, level_width[last_level] * 3);
          warping_error.ImageView(&warped_error, 1, 3, level_width[last_level] * 3);
          
          cvSub(&reference_image, &warped_image, &error);
          ImageFilter::BackwardWarp<float>(&error, &flow_x, &flow_y, &warped_error);
          
          frame_plus_error.ImageView(&error_frame, 0, 3, level_width[last_level] * 3);          
          cvAdd(&input_image, &warped_error, &error_frame);
          //cvCopy(&input_image, &error_frame);
          
          // Remove previous seam from input and flow.
          last_seam = vector<int>(level_height[last_level]);
          std::copy(seam_offset[last_level]->at(0, t-1, seam_num),
                    seam_offset[last_level]->at(level_height[last_level], t-1, seam_num),
                    last_seam.begin());
          
          DisplaceSeamWithFlow(&flow_x, &flow_y, level_width[last_level],
                               level_height[last_level], last_seam, &flow_seam);

          // Remove from previous frame (input image).
          RemoveSeam(color_frames_32f, 1, level_width[last_level], 3,
                     last_seam, color_frames_32f, 1);
          
          // Remove from error_frame.
          RemoveSeam(frame_plus_error, 0, level_width[last_level], 3,
                     last_seam, frame_plus_error, 0);
          
          // Remove from flow.
          RemoveSeam(flow, 0, level_width[last_level], 1,
                     last_seam, flow, 0);
          
          RemoveSeam(flow, 1, level_width[last_level], 1,
                     last_seam, flow, 1);
          
          // Warp.
          flow.ImageView(&flow_x, 0, 1, level_width[last_level] - 1);
          flow.ImageView(&flow_y, 1, 1, level_width[last_level] - 1);
          frame_plus_error.ImageView(&error_frame, 0, 3, (level_width[last_level]-1) * 3);        
          warped_frame_32f.ImageView(&warped_image, 0, 3, (level_width[last_level]-1) * 3);    
          
          forward_warping_mask = cvCreateImageShared(level_width[last_level] - 1,
                                                     level_height[last_level],
                                                     IPL_DEPTH_8U, 1);          
          
          ImageFilter::ForwardWarp<float>(&error_frame, &flow_x, &flow_y, &warped_image,
                                          forward_warping_mask.get());
          
          IppiSize thresh_roi = { level_width[last_level] - 1 , level_height[last_level] };
          float upper_thresh_rgb[] = { 255, 255, 255 };                           
          float lower_thresh_rgb[] = { 0, 0, 0 };                           
          ippiThreshold_LTValGTVal_32f_C3IR(warped_frame_32f.d(), warped_frame_32f.step(),
                                            thresh_roi, lower_thresh_rgb, lower_thresh_rgb,
                                            upper_thresh_rgb, upper_thresh_rgb);
          
          // Convert back to 8 bit RGBA.
          for (int i = 0; i < level_height[last_level]; ++i) {
            const float* src_ptr = warped_frame_32f.at(0, i, 0); 
            uchar* dst_ptr = (uchar*) temporal_constraint[last_level]->at(0, i, 0);

            for (int j = 0; j < level_width[last_level] - 1; ++j, src_ptr += 3, dst_ptr += 4)  {
              dst_ptr[0] = (uchar) src_ptr[0];
              dst_ptr[1] = (uchar) src_ptr[1];
              dst_ptr[2] = (uchar) src_ptr[2];
              dst_ptr[3] = 0;
            }
          }
        
       /*   IplImage warped_result;
          cvInitImageHeader(&warped_result, 
                            cvSize(level_width[last_level] - 1, level_height[last_level]),
                            IPL_DEPTH_8U, 4);
          cvSetData(&warped_result, (void*)temporal_constraint[last_level]->d(),
                    temporal_constraint[last_level]->step());
          
          cvShowImage("carving_window", &warped_result);
        
       */   
          /*
          if (seam_num + 1 == _seams) {
            QImage img ((uchar*)temporal_constraint[last_level]->d(),
                        level_width[last_level] - 1,
                        level_height[last_level], 
                        temporal_constraint[last_level]->step(),
                        QImage::Format_RGB32);

            std::string file = "warped_frame_" + boost::lexical_cast<std::string>(t) + ".png";
            img.save(file.c_str());
          }
           */
          /*
          last_seam = vector<int>(level_height[last_level]);
          std::copy(seam_offset[last_level]->at(0, t-1, seam_num),
                    seam_offset[last_level]->at(level_height[last_level], t-1, seam_num),
                    last_seam.begin());
          
          // Remove last_seam from current image.
          RemoveSeam(*color_frame[last_level], 0, level_width[last_level], 1, 
                     last_seam, *temporal_constraint[last_level]);          
          */
          
        } else {
          last_seam = vector<int>(level_height[last_level]);
          std::copy(seam_offset[last_level]->at(0, t-1, seam_num),
                    seam_offset[last_level]->at(level_height[last_level], t-1, seam_num),
                    last_seam.begin());
          
          // Remove last_seam from current image.
          RemoveSeam(*color_frame[last_level], 0, level_width[last_level], 1, 
                     last_seam, *temporal_constraint[last_level]);
        }
      }
      
      bool enforce_temporal_coherence = last_seam.size() > 0;
      
      vector<int> cur_seam(level_height[last_level]);
      if (neighbor_window_ == 3) {
        ComputeNewSeams(*color_frame[last_level],
                        *saliency[last_level], 
                        last_level_roi.width,
                        *temporal_constraint[last_level],
                        enforce_temporal_coherence,
                        *tmp_mat[last_level],
                        cur_seam,
                        flow_seam,
                        forward_warping_mask.get());
      } else {
        ComputeDiscontSeams(*color_frame[last_level],
                            *saliency[last_level],
                            last_level_roi.width,
                            *temporal_constraint[last_level],
                            enforce_temporal_coherence,
                            *tmp_mat[last_level],
                            cur_seam,
                            forward_warping_mask.get());
      }
      
      std::copy(cur_seam.begin(), cur_seam.end(),
              seam_offset[last_level]->at(0, t, seam_num));

      const int seam_diam = 10;
      
      // Propagate through other levels.
      for (int level = last_level-1; level >= 0; --level) {
        const int prev_seam_length = level_height[level+1];
        vector<int> prev_level_seam(prev_seam_length);
        std::copy(seam_offset[level+1]->at(0, t, seam_num),
                  seam_offset[level+1]->at(prev_seam_length, t, seam_num),
                  prev_level_seam.begin());
        
        vector<int> last_seam;
        if (t > 0) {
          last_seam = vector<int>(level_height[level]);
          for (int i = 0; i < level_height[level]; ++i) {
            last_seam[i] = *seam_offset[level]->at(i, t-1, seam_num);
          }
        }
        
        const vector<int>* last_seam_ptr = last_seam.size() > 0 ? &last_seam : 0;
        
        pair<int,int> shift = UpscaleSeam(*color_frame[level], *saliency[level],
                                          prev_level_seam, last_seam_ptr, seam_diam, 
                                          level_width[level],
                                          multi_scale, color_frame_pyr[level].get(),
                                          saliency_pyr[level].get());
        
        if (t > 0) {
          for (int i = 0; i < level_height[level]; ++i) {
            last_seam[i] -= shift.first;
          }
          
          RemoveSeam(*color_frame_pyr[level], 0, level_width[level], 1,
                     last_seam, *temporal_constraint[level]);
        }
        
        enforce_temporal_coherence = last_seam.size() > 0;
        
        vector<int> cur_seam(level_height[level]);
        if (neighbor_window_ == 3) {
          ComputeNewSeams(*color_frame_pyr[level],
                          *saliency_pyr[level], 
                          shift.second - shift.first + 1,
                          *temporal_constraint[level],
                          enforce_temporal_coherence,
                          *tmp_mat[level],        
                          cur_seam,
                          flow_seam);     // TODO:incorrect!
        }
        else {
          ComputeDiscontSeams(*color_frame_pyr[level],
                              *saliency_pyr[level], 
                              shift.second - shift.first + 1,
                              *temporal_constraint[level],
                              enforce_temporal_coherence,
                              *tmp_mat[level],
                              cur_seam);
        }
        
        for (int i = 0; i < level_height[level]; ++i) {
          *seam_offset[level]->at(i, t, seam_num) = cur_seam[i] + shift.first;
        }
      }
      
      // Downsample seams.
      for (int l = 1; l < multi_levels; ++l) {
         DownSampleSeam(seam_offset[l-1]->at(0, t, seam_num), level_height[l-1], 
                        seam_offset[l]->at(0, t, seam_num), level_height[l],
                        multi_scale);
      }
        
      // Remove Seams from color_frames and gray_scale.
      // Only from base level!
      removeSeam(*color_frame[0], 
                 *seam_offset[0],
                 sx - seam_num,
                 seam_num,
                 t,
                 0);
      
      // Remove from gray-scale.
      removeSeam(*gray_scale[0],
                 *seam_offset[0],
                 sx - seam_num,
                 seam_num,
                 t,
                 0);
      
      // Remove from 32f color frame.
      if (t > 0) {
        vector<int> current_seam(level_height[0]);
        std::copy(seam_offset[0]->at(0, t, seam_num),
                  seam_offset[0]->at(level_height[0], t, seam_num),
                  current_seam.begin());
        
        RemoveSeam(color_frames_32f, 2, level_width[0], 3, current_seam, color_frames_32f, 2);
      }
            
      if (_saliency_map || _user_input)
        removeSeam(*saliency[0],
                   *seam_offset[0],
                   sx - seam_num,
                   seam_num,
                   t,
                   1);
    }
  }
  
  for (int i=0; i < 4; ++i) ippiFree (plane[i]);
  ippiFree (graBuffer);  
    
  // Output first results.
  std::ofstream ofs("carve_first_seam.txt");
  for (int t = 0; t < st; ++t) {
    for (int y = 0; y < sy; ++y) {
      ofs << *seam_offset[0]->at(y, t, 0) << "  ";
    }
    if(t < st-1) {
      ofs << "\n";
    }
  }

	// convert Seams to video indexing 
  if(!blend_mode_) {
    
    Matrix3D seam_offset_local(seam_offset[0]->sx(), 
                               seam_offset[0]->sy(),
                               seam_offset[0]->st());  

    //memcpy(seam_offset_local.d(), seam_offset[0]->d(), 
    //       seam_offset[0]->step() * seam_offset[0]->sy() * seam_offset[0]->st());
    
    
    for ( int s = _seams-2; s >=0; --s ) 
      for ( int r = s+1; r < _seams; ++r) 
        for ( int t = 0; t < st; ++t)
          for (int j=0; j < sy; ++j) 
            if ( *seam_offset[0]->at(j,t,s) <= *seam_offset[0]->at(j,t,r) )
              ++(*seam_offset[0]->at(j,t,r));
     
    /*
    for ( int s = 0; s < _seams-1; ++s ) 
      for ( int r = s + 1; r < _seams; ++r) 
        for (int j=0; j < sy; ++j) 
          for ( int t = 0; t < st; ++t)
            if ( seam_offset_local.at(j,t,s) <= seam_offset_local.at(j,t,r) )
              ++(*seam_offset[0]->at(j,t,r));
     */
  } else {
    // S holds the seams.

    _computed_seams.reset(new Matrix3D(sy, st, _seams));
    IppiSize seam_roi = {sy, st * _seams};
    ippiCopy_32f_C1R(seam_offset[0]->d(), seam_offset[0]->step(), _computed_seams->d(),
                     _computed_seams->step(), seam_roi);
  }
  
  
	// create thresholdMap
	IppiSize vidSz = { sx, sy*st };
	ippiSet_32f_C1R (-1, _thresholdMap.d(), _thresholdMap.ld(), vidSz );
	for ( int s = 0; s < _seams; ++s )
		for ( int t=0; t<st; ++t) 
		{
			float* cur_seam = seam_offset[0]->at(0,t,s);

			for ( int i = 0; i < sy; ++i )
				*_thresholdMap.at(cur_seam[i], i, t) = sx-s;
		}
  
	emit done(st);
}

pair<int,int> SurfaceCarve::UpscaleSeam(const Matrix3D& color_frame, const Matrix3D& saliency,
                                        const vector<int>& seam,  const vector<int>* prev_seam,
                                        int diam, int cur_width,
                                        float scale, Matrix3D* new_color_frame,
                                        Matrix3D* new_saliency) const {
  const int sy = color_frame.sy();
  const int sx = cur_width;
  
  int min_x = sx;
  int max_x = 0;
  
  float scale_inv = 1.0 / scale;
  
  vector<pair<int, int> > upscale_seam(sy);
  
  // Upscale by interpolation
  for (int y = 0; y < sy; ++y) {
    // Limit to maximum possible value.
    float y_scale = std::min((float)y * scale, (float) seam.size() -1);
    int top = y_scale;
    float dy = y_scale - top;
    int bottom = top + (dy == 0 ? 0 : 1);
    
    float x = ((1-dy) * seam[top] + dy * seam[bottom]) * scale_inv;
    upscale_seam[y] = pair<int, int>(x, x);
  }
  
  // Make diam border around seam and include prev. seam if specified.
  if (prev_seam) {
    for (int y = 0; y < sy; ++y) {
      int x = upscale_seam[y].first;
      
      // Distance to previous seam.
      int left = std::min((int)(x - diam), (*prev_seam)[y] - 1);
      int right = std::max((int)(x + diam + 1), (*prev_seam)[y] + 1);
      
      upscale_seam[y] = pair<int, int>(std::max(0, left), 
                                       std::min(sx - 1, right));
      
      min_x = std::min(min_x, upscale_seam[y].first);
      max_x = std::max(max_x, upscale_seam[y].second);    
    }
    
  } else {
    for (int y = 0; y < sy; ++y) {
      float x = upscale_seam[y].first;
      upscale_seam[y] = pair<int, int>(std::max(0, (int)(x - diam)), 
                                       std::min(sx - 1, (int)(x + diam + 1)));

      min_x = std::min(min_x, upscale_seam[y].first);
      max_x = std::max(max_x, upscale_seam[y].second);    
    }

  }
    
  // Account for different treatment of border case.
  min_x = std::max(0, min_x - 1);
  max_x = std::min(sx-1, max_x + 1);
  
  // Copy important part to new.
  IppiSize roi = {max_x - min_x + 1, sy};
  ippiCopy_32f_C1R(color_frame.d() + min_x, color_frame.step(),
                   new_color_frame->d(), new_color_frame->step(), roi);
  ippiCopy_32f_C1R(saliency.d() + min_x, saliency.step(),
                   new_saliency->d(), new_saliency->step(), roi);
  
  // Assign saliency outside of seam diam high weight.
  // --> masking those values out.
  float* sal_ptr = new_saliency->d();
  for(int y = 0; y < sy; ++y, sal_ptr += new_saliency->ld()) {
    for (int x = min_x; x < upscale_seam[y].first; ++x)
      sal_ptr[x-min_x] = 1e10;
    for (int x = upscale_seam[y].second+1; x <= max_x; ++x)
      sal_ptr[x-min_x] = 1e10;
  }
  
  return std::make_pair(min_x, max_x);
}

void SurfaceCarve::DownSampleSeam(const float* src_seam, int src_length,
                                  float* dst_seam, int dst_length, float scale) {
  for (int y = 0; y < dst_length; ++y) {
    float y_scale = std::min((float)y / scale, (float) src_length - 1);
    
    int top = y_scale;
    float dy = y_scale - top;
    int bottom = top + (dy == 0 ? 0 : 1);
    
    float x = ((1-dy) * src_seam[top] + dy * src_seam[bottom]) * scale;
    dst_seam[y] = floor(x);
  }
}

inline float ColorDiff(const uchar* src_1, const uchar* src_2) {
  int d_r = (int)src_1[0] - (int)src_2[0];
  int d_g = (int)src_1[1] - (int)src_2[1];
  int d_b = (int)src_1[2] - (int)src_2[2];
  return sqrt((float)(d_r*d_r + d_g*d_g + d_b*d_b));
}

void SurfaceCarve::ComputeNewSeams(const Matrix3D& frame_data,
                                   Matrix3D& saliency,
                                   int cur_width, 
                                   const Matrix3D& constrain_frame,
                                   bool enforce_temporal_coherence,
                                   Matrix3D& tmp,
                                   vector<int>& cur_seam,
                                  const FlowSeam& flow_seam,
                                   IplImage* temporal_coherence_mask) {
  float* one_shift = tmp.img(0);
  float* zero_shift = tmp.img(1);
  const int row_ld = tmp.ld();
  
  int frame_height = frame_data.sy();
  int frame_width = cur_width;
  IppiSize imgRoi = {frame_width, frame_height};
  // Note frame_data holds actually RGBA as uchar values = 32 bit = sizeof(float)
  
  if (enforce_temporal_coherence) {
    // Compute temporal coherence cost.
    IppiSize tmpRoi = {frame_width-1, frame_height};
    if (!temporal_coherence_mask) {
      for (int i = 0; i < frame_height; ++i) {
        const uchar* src_old = (uchar*) constrain_frame.at(0, i, 0);
        const uchar* src = (uchar*) frame_data.at(0,i,0);
        float* zero_dst = zero_shift + i*row_ld;
        
        *zero_dst++ = ColorDiff(src_old, src);
        src_old += 4;
        src += 4;
        
        for (int j = 1; j < frame_width - 1; ++j, src_old+=4, src+=4, ++zero_dst) {
          *zero_dst = *(zero_dst-1) + ColorDiff(src_old, src);
        }
        
        float* one_dst = one_shift + i*row_ld + frame_width-1;
        src_old = (uchar*)constrain_frame.at(frame_width-2, i, 0);// carved_frame + i*row_ld + frame_width-2);
        src = (uchar*)frame_data.at(frame_width-1, i, 0);
        
        *one_dst-- = 0; 
        
        *one_dst-- = ColorDiff(src_old, src);
        src -= 4;
        src_old -= 4;
        
        for (int j = frame_width-3; j >=0; --j, src -= 4, src_old -=4, --one_dst) {
          *one_dst = *(one_dst+1) + ColorDiff(src_old, src);
        }
      }
    } else {
      for (int i = 0; i < frame_height; ++i) {
        const uchar* src_old = (uchar*) constrain_frame.at(0, i, 0);
        const uchar* src = (uchar*) frame_data.at(0,i,0);
        const uchar* mask_ptr = RowPtr<const uchar>(temporal_coherence_mask, i);
        float* zero_dst = zero_shift + i*row_ld;
        
        *zero_dst++ = ColorDiff(src_old, src) * (float)*mask_ptr++;
        src_old += 4;
        src += 4;
        
        for (int j = 1; j < frame_width - 1; ++j, src_old+=4, src+=4, ++zero_dst, ++mask_ptr) {
          *zero_dst = *(zero_dst-1) + ColorDiff(src_old, src) * (float)*mask_ptr;
        }
        
        float* one_dst = one_shift + i*row_ld + frame_width-1;
        src_old = (uchar*)constrain_frame.at(frame_width-2, i, 0);
        // carved_frame + i*row_ld + frame_width-2);
        src = (uchar*)frame_data.at(frame_width-1, i, 0);
        mask_ptr = RowPtr<const uchar>(temporal_coherence_mask, i) + frame_width - 2;
        
        *one_dst-- = 0; 
        
        *one_dst-- = ColorDiff(src_old, src) * (float)*mask_ptr--;
        src -= 4;
        src_old -= 4;
        
        for (int j = frame_width-3; j >=0; --j, src -= 4, src_old -=4, --one_dst, --mask_ptr) {
          *one_dst = *(one_dst+1) + ColorDiff(src_old, src) * (float)*mask_ptr;
        }
      }      
    }
    
    // TODO: zero_shift + 1 was old one - and I guess wrong.
    ippiAdd_32f_C1IR(zero_shift, tmp.step(), one_shift + 1, tmp.step(), tmpRoi);
    
    /*
    // Determine min in each row.
     for (int i = 0; i < frame_height; ++i) {
       float* one_dst = one_shift + i*row_ld;
       
       // Get minimum in each row and substract.
       float min_val = 1e10;
       float max_val = -1e10;
       for (int j = 0; j < frame_width; ++j) {
         if (one_dst[j] < min_val)
           min_val = one_dst[j];
         if (one_dst[j] > max_val)
           max_val = one_dst[j];
       }
       
       float n = max_val - min_val;
       if (n > 0)
         n = 1.0f / n;
       
       for (int j = 0; j < frame_width; ++j)
         one_dst[j] = (one_dst[j] - min_val) * n * 255.0f;
     }
    
    // Normalize to be in range [0,1].
    float min_val, max_val;
    ippiMinMax_32f_C1R(one_shift, tmp.step(), imgRoi, &min_val, &max_val);
    ASSURE_LOG(min_val >= 0 && max_val >= 0);
    
    ippiSubC_32f_C1IR(min_val, one_shift, tmp.step(), imgRoi);
    ippiDivC_32f_C1IR(max_val - min_val, one_shift, tmp.step(), imgRoi);
    */
    
   // if (!flow_seam.empty()) {
   //   MarkFlowSeam(one_shift, frame_width, tmp.step(),
   //                frame_height, 1, flow_seam);
   // }
    
   // ippiMulC_32f_C1IR(1.0/(float)frame_width, one_shift, tmp.step(), imgRoi);
  } else {
   // ippiSet_32f_C1R(temporal_weight_ / 2.0, one_shift, tmp.step(), imgRoi);
    ippiSet_32f_C1R(0, one_shift, tmp.step(), imgRoi);
  }
  
  float* temporal_costs = one_shift;
  
  // Visualize temporal coherence cost.
  //IplImage tmp_cost_image;
  //tmp.ImageView(&tmp_cost_image, 0, 1, frame_width);
  //double min_val, max_val;
  //cvMinMaxLoc(&tmp_cost_image, &min_val, &max_val);
  //std::cout << "Min: " << min_val << " max: " << max_val << "\n";

//  if (max_val > 0) {
//    cvScale(&tmp_cost_image,  &tmp_cost_image, 1.0 / max_val);
 //   IplImage tmp_cost_image;
 //   tmp.ImageView(&tmp_cost_image, 0, 1, frame_width);
 //   cvShowImage("carving_window", &tmp_cost_image);
 //   cvWaitKey(20);
 //   cvScale(&tmp_cost_image,  &tmp_cost_image, max_val);
//  } else {
//   cvShowImage("carving_window", &tmp_cost_image);
//   cvWaitKey(20);
//  }
  
  float* summed_costs = tmp.img(3);
/*
  if (frame_number == _video->getFrames() / 2 && seam_number == _seams / 2) {
    IppiSize roi = { frame_width, frame_height };
    std::ofstream ofs ("carve_temporal_costs.txt", std::ios_base::trunc | std::ios_base::out);
    for (int i = 0; i < frame_height; ++i) {
      float* d = temporal_costs + row_ld * i;
      
      std::copy(d, d+roi.width, std::ostream_iterator<float>(ofs, "  "));
      if (i < frame_height-1)
        ofs << "\n";
    }    
  }
  */
  
  /*
  // Normalize saliency.
  float min_val, max_val;
  ippiMinMax_32f_C1R(saliency.at(0, 0, 0), saliency.step(), imgRoi, &min_val, &max_val);
  ASSURE_LOG(min_val >= 0 && max_val >= 0);
  
  ippiSubC_32f_C1IR(min_val, saliency.at(0, 0, 0), saliency.step(), imgRoi);
  ippiDivC_32f_C1IR(max_val - min_val, saliency.at(0, 0, 0), saliency.step(), imgRoi);
  ippiMulC_32f_C1IR(saliency_weight_,  saliency.at(0, 0, 0), saliency.step(),
                    imgRoi);
  */
  
  /*
  // Combine with temporal cost.
  if (enforce_temporal_coherence) {
  for (int i = 0; i < frame_height; ++i) {
    float* sal_ptr = saliency.at(0, i, 0);
    float* tmp_ptr = tmp.at(0, i, 0);
    for (int j = 0; j < frame_width; ++j, ++sal_ptr, ++tmp_ptr) {
      float w = 1.f - (1.f - sal_ptr[0]) * (1.f - tmp_ptr[0]);
      sal_ptr[0] =  w * w;
    }
  }
  }
   */
  
 // IplImage tmp_cost_image;
 // saliency.ImageView(&tmp_cost_image, 0, 1, frame_width);
 // cvShowImage("carving_window", &tmp_cost_image);
 // cvWaitKey(20);

  ippiMulC_32f_C1IR(temporal_weight_, temporal_costs, tmp.step(),
                    imgRoi);

//  ippiMul_32f_C1IR(temporal_weights.d(), temporal_weights.step(), temporal_costs, tmp.step(),
  //                 imgRoi);
  
  // ippiMulC_32f_C1IR(0.05, temporal_gradients.img(t), temporal_gradients.step(), imgRoi);
  // ippiThreshold_LT_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), imgRoi, 1);
  // ippiThreshold_GT_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), imgRoi, 5);
  
  //    ippiDiv_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), temporal_costs,
  //                   tmp.step(), imgRoi);
  //  }
  // ippiMul_32f_C1IR(saliency.img(t), saliency.step(), temporal_costs, tmp.step(),
  //                   imgRoi);
  //    ippiThreshold_
  
  // Compute forward energy
  float* diff_45 = tmp.img(5);
  float* diff_135 = tmp.img(6);
  float* diff_col = tmp.img(7);
  float* diff_row = tmp.img(8);
  float* diff_row2 = tmp.img(9);
  
  for (int i = 0; i < frame_height; ++i) {
    const uchar* cur_ptr = (uchar*)frame_data.at(0,i,0);
    const uchar* next_ptr = (uchar*)frame_data.at(0,i+1,0); 
    
    float* diff_col_ptr = diff_col + row_ld*i;
    float* diff_row_ptr = diff_row + row_ld*i;
    float* diff_row2_ptr = diff_row2 + row_ld*i;
    
    float* diff_45_ptr = diff_45 + row_ld*i;
    float* diff_135_ptr = diff_135 + row_ld*i;
    
    for (int j = 0; 
         j < frame_width;
         ++j, cur_ptr+=4, next_ptr+=4, ++diff_col_ptr, ++diff_row_ptr, ++diff_row2_ptr,
         ++diff_45_ptr, ++diff_135_ptr) {
      
      if (i < frame_height-1)
        *diff_col_ptr = ColorDiff(cur_ptr, next_ptr);
      
      if (j < frame_width-1) {
        *diff_row_ptr = ColorDiff(cur_ptr, cur_ptr + 4);
        if (i < frame_height - 1) {
          *diff_45_ptr = ColorDiff(cur_ptr + 4, next_ptr);
          *diff_135_ptr = ColorDiff(cur_ptr, next_ptr + 4);
        }
      }
      
      if (j < frame_width-2)
        *diff_row2_ptr = ColorDiff(cur_ptr, cur_ptr+8);
    }
  }
  
  // Apply seam carving operation.
  const float forward_weight = forward_weight_ * 5;// * 255;
  
  const float* sal_ptr = saliency.at(0, 0, 0);
  const float* tmp_ptr = temporal_costs;
  
  float* sum_ptr = summed_costs;
  float* idx_mat = tmp.img(4);
  
  float* diff_row_ptr = diff_row;
  float* diff_row2_ptr = diff_row2;
  
  // First row.
  // First elem.
  sum_ptr[0] = sal_ptr[0] +
  tmp_ptr[0]  +
  fabs(diff_row_ptr[0] - diff_row_ptr[1]) * forward_weight;
  
  for (int j = 1; j < frame_width-1; ++j) {
    sum_ptr[j] = sal_ptr[j] + 
    tmp_ptr[j]  +
    (diff_row_ptr[j-1] + diff_row_ptr[j] - diff_row2_ptr[j-1]) 
    * forward_weight;
  }
  
  // Last elem.
  sum_ptr[frame_width-1] = sal_ptr[frame_width-1] +
  tmp_ptr[frame_width-1]  +
  fabs(diff_row_ptr[frame_width-2] - diff_row_ptr[frame_width-1])
  * forward_weight;
  
  
  // Remaining rows.
  int min_idx;
  for (int i = 1; i < frame_height; ++i) {
    const float* sum_prev_ptr = summed_costs + (i-1)*row_ld;
    sum_ptr = summed_costs + i*row_ld;
    sal_ptr = saliency.at(0, i, 0);
    tmp_ptr = temporal_costs + i*row_ld;
    float* idx_ptr = idx_mat + i*row_ld;
    
    float* diff_col_ptr = diff_col + row_ld*(i-1);
    float* diff_row_ptr = diff_row + row_ld*i;
    float* diff_row2_ptr = diff_row2 + row_ld*i;
    
    float* diff_45_ptr = diff_45 + row_ld*(i-1);
    float* diff_135_ptr = diff_135 + row_ld*(i-1);
    
    // First Elem.
    float forward_right = fabs(diff_col_ptr[0] - diff_135_ptr[0]) +
    fabs(diff_col_ptr[1] - diff_135_ptr[0]);
    
    float forward_middle = fabs(diff_col_ptr[0] - diff_col_ptr[1]); 
   
    *sum_ptr = min2idx(sum_prev_ptr[0] + forward_middle * forward_weight, 
                       sum_prev_ptr[1] + forward_right * forward_weight,
                       min_idx) +
    *sal_ptr +
    *tmp_ptr +
    fabs(diff_row_ptr[0] - diff_row_ptr[1]) * forward_weight;
    
    *idx_ptr = min_idx;
    
    ++diff_col_ptr;
    
    ++sum_ptr;
    ++sum_prev_ptr;
    ++sal_ptr;
    ++tmp_ptr;
    ++idx_ptr;
    
    ++diff_135_ptr;
    
    // Remaining Elems.
    for (int j = 1;
         j < frame_width-1;
         ++sum_ptr, ++sum_prev_ptr, ++sal_ptr, ++tmp_ptr, ++idx_ptr, ++j,
         ++diff_col_ptr, ++diff_row_ptr, ++diff_row2_ptr, ++diff_45_ptr, ++diff_135_ptr) {
      float forward_left = fabs(diff_col_ptr[-1] - diff_45_ptr[0]) +
      fabs(diff_col_ptr[0] - diff_45_ptr[0]);
      float forward_right = fabs(diff_col_ptr[0] - diff_135_ptr[0]) +
      fabs(diff_col_ptr[1] - diff_135_ptr[0]);
      float forward_middle = fabs(diff_col_ptr[0] - diff_col_ptr[1]) +
      fabs(diff_col_ptr[0] - diff_col_ptr[-1]);

      *sum_ptr = min3idx(sum_prev_ptr[-1] + forward_left * forward_weight,
                         sum_prev_ptr[0] + forward_middle * forward_weight,
                         sum_prev_ptr[1] + forward_right * forward_weight,
                         min_idx) +
      *sal_ptr +
      *tmp_ptr +
      (diff_row_ptr[0] + diff_row_ptr[1] - diff_row2_ptr[0]) * forward_weight;
      
      *idx_ptr = j + min_idx;
    }
    
    // Last Elem.
    float forward_left = fabs(diff_col_ptr[-1] - diff_45_ptr[0]) +
    fabs(diff_col_ptr[0] - diff_45_ptr[0]);
    forward_middle = fabs(diff_col_ptr[0] - diff_col_ptr[-1]);


    *sum_ptr = min2idx(sum_prev_ptr[0] + forward_middle * forward_weight,
                       sum_prev_ptr[-1] + forward_left * forward_weight,
                       min_idx) + 
    *sal_ptr +
    *tmp_ptr +
    fabs(diff_row_ptr[0] - diff_row_ptr[-1]) * forward_weight;
    *idx_ptr = frame_width-1 - min_idx;
  }
  /*
  if (frame_number == _video->getFrames() / 2 && seam_number == _seams / 2) {
    IppiSize roi = { frame_width, frame_height };
    std::ofstream ofs ("carve_summed_energy.txt", std::ios_base::trunc | std::ios_base::out);
    for (int i = 0; i < frame_height; ++i) {
      float* d = summed_costs + row_ld * i;
      
      std::copy(d, d+roi.width, std::ostream_iterator<float>(ofs, "  "));
      if (i < frame_height-1)
        ofs << "\n";
    }    
  }
  */
  // Get minium in last row.
  float* first_elem = summed_costs + (frame_height-1)*row_ld;
  
  min_idx = std::min_element (first_elem, first_elem + frame_width ) - first_elem;
//  int* seam_ptr = S.at(frame_height-1, frame_number, seam_number);
  cur_seam[frame_height-1] = min_idx;
//  *seam_ptr-- = min_idx;
  
  float* idx_ptr = idx_mat + (frame_height-1)*row_ld;
  
  for (int i = frame_height-2; i >= 0; --i, idx_ptr -= row_ld) {
    cur_seam[i] = idx_ptr[cur_seam[i+1]];
    //  *seam_ptr = idx_ptr[(int) *(seam_ptr+1)];
  }
}



void SurfaceCarve::ComputeDiscontSeams(const Matrix3D& frame_data,
                                       const Matrix3D& saliency,
                                       int   cur_width, 
                                       const Matrix3D& constrain_frame,
                                       bool enforce_temporal_coherence,
                                       Matrix3D& tmp,
                                       vector<int>& cur_seam,
                                       IplImage* temporal_coherence_mask) {
  float* one_shift = tmp.img(0);
  float* zero_shift = tmp.img(1);
  float* carved_frame = tmp.img(2);
  const int row_ld = tmp.ld();
  
  int frame_height = frame_data.sy();
  int frame_width = cur_width;
  IppiSize imgRoi = {frame_width, frame_height};
  // Note frame_data holds actually RGBA as uchar values = 32 bit = sizeof(float)

  /*
  // Traverse frames.
  if (old_seam.size() > 0) {
    // Remove old seam from curent image by traversing rows.
    const float* src_ptr = frame_data.at(0, 0, 0); // Current frame.
    float* dst_ptr = carved_frame;
    
    for (int i=0; i < frame_height; ++i, dst_ptr += row_ld, src_ptr += frame_data.ld() ) {
      int seam_val = old_seam[i]; // *S.at(i, frame_number-1, seam_number);
      if (seam_val > 0) // copy left part
        memcpy(dst_ptr, src_ptr, seam_val*sizeof(float));
      
      if (seam_val < frame_width-1) // copy right part
        memcpy(dst_ptr+seam_val, src_ptr+seam_val+1, (frame_width-1 - seam_val)*sizeof(float));
    }
    
    // Compute temporal coherence cost.
    IppiSize tmpRoi = {frame_width-1, frame_height};
    
    for (int i = 0; i < frame_height; ++i) {
      const uchar* src_old = (uchar*)(carved_frame + i*row_ld);
      const uchar* src = (uchar*) frame_data.at(0,i,0);
      float* zero_dst = zero_shift + i*row_ld;
      
      *zero_dst++ = ColorDiff(src_old, src);
      src_old += 4;
      src += 4;
      
      for (int j = 1; j < frame_width - 1; ++j, src_old+=4, src+=4, ++zero_dst) {
        *zero_dst = *(zero_dst-1) + ColorDiff(src_old, src);
      }
      
      float* one_dst = one_shift + i*row_ld + frame_width-1;
      src_old = (uchar*)(carved_frame + i*row_ld + frame_width-2);
      src = (uchar*)frame_data.at(frame_width-1, i, 0);
      
      *one_dst-- = 0; 
      
      *one_dst-- = ColorDiff(src_old, src);
      src -= 4;
      src_old -= 4;
      
      for (int j = frame_width-3; j >=0; --j, src -= 4, src_old -=4, --one_dst) {
        *one_dst = *(one_dst+1) + ColorDiff(src_old, src);
      }
    }
    
    ippiAdd_32f_C1IR(zero_shift+1, tmp.step(), one_shift, tmp.step(), tmpRoi);
  //  ippiMulC_32f_C1IR(1.0/(float)frame_width, one_shift, tmp.step(), imgRoi);
  } else {
    //ippiSet_32f_C1R(temporal_weight_ / 8.0 / 2.0, one_shift, tmp.step(), imgRoi);
    ippiSet_32f_C1R(0, one_shift, tmp.step(), imgRoi);
  }
  */
  
  
  if (enforce_temporal_coherence) {
    // Compute temporal coherence cost.
    IppiSize tmpRoi = {frame_width-1, frame_height};
    if (!temporal_coherence_mask) {
      for (int i = 0; i < frame_height; ++i) {
        const uchar* src_old = (uchar*) constrain_frame.at(0, i, 0);
        const uchar* src = (uchar*) frame_data.at(0,i,0);
        float* zero_dst = zero_shift + i*row_ld;
        
        *zero_dst++ = ColorDiff(src_old, src);
        src_old += 4;
        src += 4;
        
        for (int j = 1; j < frame_width - 1; ++j, src_old+=4, src+=4, ++zero_dst) {
          *zero_dst = *(zero_dst-1) + ColorDiff(src_old, src);
        }
        
        float* one_dst = one_shift + i*row_ld + frame_width-1;
        src_old = (uchar*)constrain_frame.at(frame_width-2, i, 0);// carved_frame + i*row_ld + frame_width-2);
        src = (uchar*)frame_data.at(frame_width-1, i, 0);
        
        *one_dst-- = 0; 
        
        *one_dst-- = ColorDiff(src_old, src);
        src -= 4;
        src_old -= 4;
        
        for (int j = frame_width-3; j >=0; --j, src -= 4, src_old -=4, --one_dst) {
          *one_dst = *(one_dst+1) + ColorDiff(src_old, src);
        }
      }
    } else {
      for (int i = 0; i < frame_height; ++i) {
        const uchar* src_old = (uchar*) constrain_frame.at(0, i, 0);
        const uchar* src = (uchar*) frame_data.at(0,i,0);
        const uchar* mask_ptr = RowPtr<const uchar>(temporal_coherence_mask, i);
        float* zero_dst = zero_shift + i*row_ld;
        
        *zero_dst++ = ColorDiff(src_old, src) * (float)*mask_ptr++;
        src_old += 4;
        src += 4;
        
        for (int j = 1; j < frame_width - 1; ++j, src_old+=4, src+=4, ++zero_dst, ++mask_ptr) {
          *zero_dst = *(zero_dst-1) + ColorDiff(src_old, src) * (float)*mask_ptr;
        }
        
        float* one_dst = one_shift + i*row_ld + frame_width-1;
        src_old = (uchar*)constrain_frame.at(frame_width-2, i, 0);
        // carved_frame + i*row_ld + frame_width-2);
        src = (uchar*)frame_data.at(frame_width-1, i, 0);
        mask_ptr = RowPtr<const uchar>(temporal_coherence_mask, i) + frame_width - 2;
        
        *one_dst-- = 0; 
        
        *one_dst-- = ColorDiff(src_old, src) * (float)*mask_ptr--;
        src -= 4;
        src_old -= 4;
        
        for (int j = frame_width-3; j >=0; --j, src -= 4, src_old -=4, --one_dst, --mask_ptr) {
          *one_dst = *(one_dst+1) + ColorDiff(src_old, src) * (float)*mask_ptr;
        }
      }      
    }
    
    // TODO: zero_shift + 1 was old one - and I guess wrong.
    ippiAdd_32f_C1IR(zero_shift, tmp.step(), one_shift + 1, tmp.step(), tmpRoi);
    
    /*
    // Determine min in each row.
    for (int i = 0; i < frame_height; ++i) {
      float* one_dst = one_shift + i*row_ld;
      
      // Get minimum in each row and substract.
      float min_val = 1e10;
      float max_val = -1e10;
      for (int j = 0; j < frame_width; ++j) {
        if (one_dst[j] < min_val)
          min_val = one_dst[j];
        if (one_dst[j] > max_val)
          max_val = one_dst[j];
      }
      
      float n = max_val - min_val;
      if (n > 0)
        n = 1.0f / n;
      
      for (int j = 0; j < frame_width; ++j)
        one_dst[j] = (one_dst[j] - min_val) * n * 255.0f;
    }
     */
    /*
     // Normalize to be in range [0,1].
     float min_val, max_val;
     ippiMinMax_32f_C1R(one_shift, tmp.step(), imgRoi, &min_val, &max_val);
     ASSURE_LOG(min_val >= 0 && max_val >= 0);
     
     ippiSubC_32f_C1IR(min_val, one_shift, tmp.step(), imgRoi);
     ippiDivC_32f_C1IR(max_val - min_val, one_shift, tmp.step(), imgRoi);
     */
    // ippiMulC_32f_C1IR(1.0/(float)frame_width, one_shift, tmp.step(), imgRoi);
  } else {
    // ippiSet_32f_C1R(temporal_weight_ / 2.0, one_shift, tmp.step(), imgRoi);
    ippiSet_32f_C1R(0, one_shift, tmp.step(), imgRoi);
  }  
  
  float* temporal_costs = one_shift;
  float* summed_costs = tmp.img(3);
  
 // ippiMul_32f_C1IR(temporal_weights.d(), temporal_weights.step(), temporal_costs, tmp.step(),
   //                imgRoi);
  
  ippiMulC_32f_C1IR(temporal_weight_, temporal_costs, tmp.step(),
                    imgRoi);
  
  // ippiMulC_32f_C1IR(0.05, temporal_gradients.img(t), temporal_gradients.step(), imgRoi);
  // ippiThreshold_LT_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), imgRoi, 1);
  // ippiThreshold_GT_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), imgRoi, 5);
  
  //    ippiDiv_32f_C1IR(temporal_gradients.img(t), temporal_gradients.step(), temporal_costs,
  //                   tmp.step(), imgRoi);
  //  }
  // ippiMul_32f_C1IR(saliency.img(t), saliency.step(), temporal_costs, tmp.step(),
  //                   imgRoi);
  //    ippiThreshold_
  
  // Compute forward energy
  float* diff_45 = tmp.img(5);
  float* diff_135 = tmp.img(6);
  float* diff_col = tmp.img(7);
  float* diff_row = tmp.img(8);
  float* diff_row2 = tmp.img(9);
  
  for (int i = 0; i < frame_height; ++i) {
    const uchar* cur_ptr = (uchar*)frame_data.at(0,i,0);
    const uchar* next_ptr = (uchar*)frame_data.at(0,i+1,0); 
    
    float* diff_col_ptr = diff_col + row_ld*i;
    float* diff_row_ptr = diff_row + row_ld*i;
    float* diff_row2_ptr = diff_row2 + row_ld*i;
    
    float* diff_45_ptr = diff_45 + row_ld*i;
    float* diff_135_ptr = diff_135 + row_ld*i;
    
    for (int j = 0; 
         j < frame_width;
         ++j, cur_ptr+=4, next_ptr+=4, ++diff_col_ptr, ++diff_row_ptr, ++diff_row2_ptr,
         ++diff_45_ptr, ++diff_135_ptr) {
      
      if (i < frame_height-1)
        *diff_col_ptr = ColorDiff(cur_ptr, next_ptr);
      
      if (j < frame_width-1) {
        *diff_row_ptr = ColorDiff(cur_ptr, cur_ptr + 4);
        if (i < frame_height - 1) {
          *diff_45_ptr = ColorDiff(cur_ptr + 4, next_ptr);
          *diff_135_ptr = ColorDiff(cur_ptr, next_ptr + 4);
        }
      }
      
      if (j < frame_width-2)
        *diff_row2_ptr = ColorDiff(cur_ptr, cur_ptr+8);
    }
  }
  
  // Apply seam carving operation.
  const float forward_weight = forward_weight_ * 5;
  
  const float* sal_ptr = saliency.at(0, 0, 0);
  const float* tmp_ptr = temporal_costs;
  
  float* sum_ptr = summed_costs;
  float* idx_mat = tmp.img(4);
  
  float* diff_row_ptr = diff_row;
  float* diff_row2_ptr = diff_row2;
  
  float* forward_n = new float[neighbor_window_];
  float* sum_prev_n = new float[neighbor_window_];
  const int rad = neighbor_window_ / 2;
  
  // First row.
  // First elem.
  sum_ptr[0] = sal_ptr[0] +
               tmp_ptr[0]  +
               fabs(diff_row_ptr[0] - diff_row_ptr[1]) * forward_weight;
  
  for (int j = 1; j < frame_width-1; ++j) {
    sum_ptr[j] = sal_ptr[j] + 
                 tmp_ptr[j]  +
                 (diff_row_ptr[j-1] + diff_row_ptr[j] - diff_row2_ptr[j-1]) 
                      * forward_weight;
  }
  
  // Last elem.
  sum_ptr[frame_width-1] = sal_ptr[frame_width-1] +
                           tmp_ptr[frame_width-1]  +
                           fabs(diff_row_ptr[frame_width-2] - diff_row_ptr[frame_width-1])
                              * forward_weight;
  
  
  // Remaining rows.
  int min_idx;
  for (int i = 1; i < frame_height; ++i) {
    const float* sum_prev_ptr = summed_costs + (i-1)*row_ld;
    sum_ptr = summed_costs + i*row_ld;
    sal_ptr = saliency.at(0, i, 0);
    tmp_ptr = temporal_costs + i*row_ld;
    float* idx_ptr = idx_mat + i*row_ld;
    
    float* diff_col_ptr = diff_col + row_ld*(i-1);
    float* diff_row_ptr = diff_row + row_ld*i;
    float* diff_row2_ptr = diff_row2 + row_ld*i;
    
    float* diff_45_ptr = diff_45 + row_ld*(i-1);
    float* diff_135_ptr = diff_135 + row_ld*(i-1);
    
    
    // First Elem.
  //  float forward_right = fabs(diff_col_ptr[0] - diff_135_ptr[0]) +
  //  fabs(diff_col_ptr[1] - diff_135_ptr[0]);
    
  //  float forward_middle =0 ;

    // Neighbor above.
    forward_n[rad] = 0 ;
    sum_prev_n[rad] = sum_prev_ptr[0];
    
    for (int k = 1; k <= rad; ++k) {
      forward_n[rad + k] = forward_n[rad + k - 1] + fabs(diff_col_ptr[k-1] - diff_135_ptr[k-1]) +
                                      fabs(diff_col_ptr[k] - diff_135_ptr[k-1]);
                                           
      sum_prev_n[rad + k] = forward_n[rad + k] * forward_weight + sum_prev_ptr[k];
    }

    // Get min.
    *sum_ptr = MinVecIdx(sum_prev_n + rad, rad+1, min_idx) + 
               *sal_ptr +
               *tmp_ptr +
               fabs(diff_row_ptr[0] - diff_row_ptr[1]) * forward_weight;
/*                                           
    *sum_ptr = min2idx(sum_prev_ptr[0] + forward_middle * forward_weight, 
                       sum_prev_ptr[1] + forward_right * forward_weight,
                       min_idx) +
    *sal_ptr +
    *tmp_ptr +
    fabs(diff_row_ptr[0] - diff_row_ptr[1]) * forward_weight;
  */
    
    *idx_ptr = min_idx;
    
    ++diff_col_ptr;
    
    ++sum_ptr;
    ++sum_prev_ptr;
    ++sal_ptr;
    ++tmp_ptr;
    ++idx_ptr;
    
    ++diff_135_ptr;
    
    // Remaining Elems.
    for (int j = 1;
         j < frame_width-1;
         ++sum_ptr, ++sum_prev_ptr, ++sal_ptr, ++tmp_ptr, ++idx_ptr, ++j,
         ++diff_col_ptr, ++diff_row_ptr, ++diff_row2_ptr, ++diff_45_ptr, ++diff_135_ptr) {
      
      int start = std::max(-j, -rad);
      int end = std::min(rad, frame_width - 1 - j);
      
      // Get above.
      forward_n[rad] = 0 ;
      sum_prev_n[rad] = sum_prev_ptr[0];
      
      for (int k = -1; k >= start; --k) {
        forward_n[rad+k] = forward_n[rad + k+1] + fabs(diff_col_ptr[k] - diff_45_ptr[k+1]) +
                                              fabs(diff_col_ptr[k+1] - diff_45_ptr[k+1]);
        
        sum_prev_n[rad+k] = forward_n[rad+k] * forward_weight + sum_prev_ptr[k];
      }
      
      for (int k = 1; k <= end; ++k) {
        forward_n[rad+k] = forward_n[rad + k-1] + fabs(diff_col_ptr[k-1] - diff_135_ptr[k-1]) +
                                        fabs(diff_col_ptr[k] - diff_135_ptr[k-1]);
        
        sum_prev_n[rad+k] = forward_n[rad+k] * forward_weight + sum_prev_ptr[k];
      }
      
      *sum_ptr = MinVecIdx(sum_prev_n + rad + start, end - start + 1, min_idx) + 
      
      
     /* float forward_left = fabs(diff_col_ptr[-1] - diff_45_ptr[0]) +
      fabs(diff_col_ptr[0] - diff_45_ptr[0]);
      float forward_right = fabs(diff_col_ptr[0] - diff_135_ptr[0]) +
      fabs(diff_col_ptr[1] - diff_135_ptr[0]);
      float forward_middle = fabs(diff_col_ptr[0] - diff_col_ptr[1]) +
      fabs(diff_col_ptr[0] - diff_col_ptr[-1]);
      forward_middle = 0;
      */
      
      
      
    //  *sum_ptr = min3idx(sum_prev_ptr[-1] + forward_left * forward_weight,
      //                   sum_prev_ptr[0] + forward_middle * forward_weight,
        //                 sum_prev_ptr[1] + forward_right * forward_weight,
          //               min_idx) +
      *sal_ptr +
      *tmp_ptr +
      (diff_row_ptr[0] + diff_row_ptr[1] - diff_row2_ptr[0]) * forward_weight;
      
      min_idx = min_idx + start + j;
      
      
      *idx_ptr = min_idx;
    }
    
    // Last Elem.
    //float forward_left = fabs(diff_col_ptr[-1] - diff_45_ptr[0]) +
    //fabs(diff_col_ptr[0] - diff_45_ptr[0]);

    forward_n[rad] = 0 ;
    sum_prev_n[rad] = sum_prev_ptr[0];
    
    for (int k = -1; k >= -rad; --k) {
      forward_n[rad + k] = forward_n[rad + k+1] + fabs(diff_col_ptr[k] - diff_45_ptr[k+1]) +
                                      fabs(diff_col_ptr[k+1] - diff_45_ptr[k+1]);
      
      sum_prev_n[rad + k] = forward_n[rad + k] * forward_weight + sum_prev_ptr[k];
    }
    
    *sum_ptr = MinVecIdx(sum_prev_n, rad+1, min_idx) + 
    
//    *sum_ptr = min2idx(sum_prev_ptr[0] + forward_middle * forward_weight, 
  //                     sum_prev_ptr[-1] + forward_left * forward_weight,
    //                   min_idx) + 
    *sal_ptr +
    *tmp_ptr +
    fabs(diff_row_ptr[0] - diff_row_ptr[-1]) * forward_weight;
    
    min_idx = (frame_width - 1) + (min_idx - rad);
    
    *idx_ptr = min_idx;
  }
  
  // Get minium in last row.
  float* first_elem = summed_costs + (frame_height-1)*row_ld;
  
  min_idx = std::min_element (first_elem, first_elem + frame_width ) - first_elem;
//  float* seam_ptr = S.at(frame_height-1, frame_number, seam_number);
  
  cur_seam[frame_height-1] = min_idx;
  //*seam_ptr-- = min_idx;
  
  float* idx_ptr = idx_mat + (frame_height-1)*row_ld;
  
  for (int i = frame_height-2; i >= 0; --i, idx_ptr -= row_ld) {
    cur_seam[i] = idx_ptr[cur_seam[i+1]];
    //*seam_ptr = idx_ptr[(int) *(seam_ptr+1)];
  }
  
  delete [] forward_n;
  delete [] sum_prev_n;
}

// Awesome color average hacking!
#define AVERAGE(a, b) (((((a) ^ (b)) & 0xfefefefeL) >> 1) + ((a) & (b)) )

unsigned char* SurfaceCarve::getCarvedImg (int which, int seam)
{
	IppiSize imgRoi = { _thresholdMap.sx(), _thresholdMap.sy() };
	bool enlarge = false;
	if ( seam > 0 ) {
		seam *= -1;
		enlarge = true;
	}
  
  if(!blend_mode_)
    ippiCompareC_32f_C1R (_thresholdMap.img(which), _thresholdMap.step(), _thresholdMap.sx()+seam, 
                          _mask, _maskStep, imgRoi, ippCmpLessEq);
  
	if ( !enlarge ) {
    if(blend_mode_) {
      seam *= -1;
      // Copy.
      IppiSize roi = {_video->getWidth(), _video->getHeight()};
      
      ippiCopy_32s_C1R(_video->getData()[which]->getData(), sizeof(int) * _video->getWidth(),
                       (int*) _blendImg.d(), _blendImg.step(), roi);
      
      for (int s = 0; s < seam; ++s) {
        removeSeamBlend(_blendImg, *_computed_seams, _video->getWidth() - s, s, which, 0);
      }
      
      for ( int j=0; j < _video->getHeight(); ++j) {
        int* curSrcRow = (int*) _blendImg.at(0,j,0);
        int* curDstRow = (int*) (_tmpImg + _steps * j);
        memcpy(curDstRow, curSrcRow,  (_video->getWidth() - seam) * sizeof(int));
      } 
    } else {
        /*
         for ( int i =0; i < _video->getHeight(); ++i) {
         Ipp8u* curMaskRow = _mask + _maskStep * i;
         int* curSrcRow = _video->getData()[which]->getData() + _video->getWidth()*i;
         int* curDstRow = (int*) (_tmpImg + _steps * i);
         
         for ( int j=0; j < _video->getWidth(); ++j) {
         if ( *(curMaskRow+j) )
         *(curDstRow++) = *(curSrcRow+j);
         }
         }
         */
        
        int* blend_vec = new int[_video->getWidth()/2];
        int blend_sz = 0;
        int r,g,b,a,color;
        
        // traverse mask and copy everything that is 1
        for ( int i =0; i < _video->getHeight(); ++i)
        {
          Ipp8u* curMaskRow = _mask + _maskStep * i;
          int* curSrcRow = _video->getData()[which]->getData() + _video->getWidth()*i;
          int* curDstRow = (int*) (_tmpImg + _steps * i);
          int* start_elem = curDstRow;
          
          for ( int j=0; j < _video->getWidth(); ++j)
          {
            if (curMaskRow[j]) {
              if (blend_sz == 0) {
                *(curDstRow++) = curSrcRow[j];
              } else {
                // Blend all pixels together.
                r = 0, g=0, b=0, a=0;
                for (int i = 0; i < blend_sz; ++i) {
                  r += (blend_vec[0] & 0xff000000) >> 24;
                  g += (blend_vec[0] & 0x00ff0000) >> 16;
                  b += (blend_vec[0] & 0x0000ff00) >> 8;
                  a += (blend_vec[0] & 0x000000ff) >> 0;
                }
                
                r /= blend_sz;
                g /= blend_sz;
                b /= blend_sz;
                a /= blend_sz;
                blend_sz = 0;
                color = r << 24 | g << 16 | b << 8 | a;
                
                // Put on previous one as well.
                if (curDstRow > start_elem)
                  curDstRow[-1] = AVERAGE(curDstRow[-1], color);
                
                *(curDstRow++) = AVERAGE(curSrcRow[j], color);
              }
            } else {
              blend_vec[blend_sz++] = curSrcRow[j];
            }
          }
        }
        delete [] blend_vec;
      }
    } else {
		
      for ( int i =0; i < _video->getHeight(); ++i)
		{
			Ipp8u* curMaskRow = _mask + _maskStep * i;
			int* curSrcRow = _video->getData()[which]->getData() + _video->getWidth()*i;
			int* curDstRow = (int*) (_tmpImg + _steps * i);

			for (int j=0; j < _video->getWidth(); ++j)
			{
				*(curDstRow++) = *(curSrcRow+j);
				if ( ! *(curMaskRow+j) )
					*(curDstRow++) = *(curSrcRow+j);
			}
		}
	}

	return _tmpImg;
}

void SurfaceCarve::RemoveSeam(const Matrix3D& input, int frame, int cur_width, int colors,
                              const vector<int>& old_seam,
                              Matrix3D& output, int outframe) {
  // Remove old seam from current image by traversing rows.
  for (int i=0; i < input.sy(); ++i) {
    const float* src_ptr = input.at(0, i, frame);
    float* dst_ptr = output.at(0, i, outframe);
    int seam_val = old_seam[i];
    if (seam_val > 0 && src_ptr != dst_ptr) // copy left part
      memmove(dst_ptr, src_ptr, seam_val * sizeof(float) * colors);
    
    if (seam_val < cur_width-1) // copy right part
      memmove(dst_ptr + seam_val * colors, src_ptr + (seam_val + 1) * colors,
             (cur_width - 1 - seam_val) * sizeof(float) * colors);
  }
}


void SurfaceCarve::removeSeam (Matrix3D& mat, const Matrix3D& S, int width, int seam, 
                               int frame_number, int idx) {
  for ( int i=0; i < mat.sy(); ++ i) {
    int val = *S.at(i, frame_number, seam);
    if ( val >= 0 && val < width-1 )
      memmove (mat.at(val, i, idx), mat.at(val+1, i, idx), (width - 1 - val)*sizeof(float));
  }
}

void SurfaceCarve::removeSeamBlend (Matrix3D& mat, const Matrix3D& S, int width, int seam, 
                                    int frame_number, int idx) {
  for ( int i=0; i < mat.sy(); ++ i) {
    int val = *S.at(i, frame_number, seam);
    if (val > 0 && val < width-2) {
      int col_0 =  *reinterpret_cast<int*>(mat.at(val-1, i, idx));
      int col_1 =  *reinterpret_cast<int*>(mat.at(val, i, idx));
      int col_2 =  *reinterpret_cast<int*>(mat.at(val+1, i, idx));
      
      *reinterpret_cast<int*>(mat.at(val-1, i, idx)) = AVERAGE(col_0, col_1);
      *reinterpret_cast<int*>(mat.at(val, i, idx)) = AVERAGE(col_1, col_2);
      memmove (mat.at(val+1, i, idx), mat.at(val+2, i, idx), (width - 1 - (val+1))*sizeof(float));
      } else if ( val == 0 && val < width-1 ) {
      memmove (mat.at(val, i, idx), mat.at(val+1, i, idx), (width - 1 - val)*sizeof(float));
     } 
	}
}

void SurfaceCarve::DisplaceSeamWithFlow(const IplImage* flow_x, 
                                        const IplImage* flow_y,
                                        int width,
                                        int height,
                                        const vector<int>& seam,
                                        FlowSeam* flow_seam) const {
  flow_seam->resize(seam.size());
  
  for (int i = 0; i < height; ++i) {
    const float* flow_x_ptr = RowPtr<float>(flow_x, i);
    const float* flow_y_ptr = RowPtr<float>(flow_y, i);
    
    int x_loc = seam[i];
    FlowSeamElem fse;
    
    fse.x = std::max(0.f, std::min((float) x_loc + flow_x_ptr[x_loc], width - 1.f));
    fse.y = std::max(0.f, std::min((float) i + flow_y_ptr[x_loc], height - 1.f));
    fse.magn = std::sqrt(flow_x_ptr[x_loc] * flow_x_ptr[x_loc] +
                         flow_y_ptr[x_loc] * flow_y_ptr[x_loc]);

    (*flow_seam)[i] = fse;
  }
}



void SurfaceCarve::MarkFlowSeam(float* coherence_map,
                                int width,
                                int width_step,
                                int height,
                                int rad,
                                const FlowSeam& flow_seam) {
  
  vector<float> weights (2*rad+1);
  
  for (int i = 0; i < 2 * rad + 1; ++i) {
    weights[i] = sqrt(fabs(rad - i) / (float) rad * 0.5);
  }
  
  for (int i = 0; i < height; ++i) {
    const float x = flow_seam[i].x;
    const float y = flow_seam[i].y;
    const float magn = flow_seam[i].magn;
    
    // Round to nearest location. 
    // Multiply patch of cost zero around it.
    // Todo take magnitude of flow into account.
    const int x_loc = x + 0.5;
    const int y_loc = y + 0.5;
    
    for (int k = y_loc - rad, r_k = 0; k <= y_loc + rad; ++k, ++r_k) {
      if (k >= 0 && k < height) {
        float* coherence_ptr = PtrOffset(coherence_map, k * width_step);
        for (int l = x_loc - rad, r_l = 0; l <= x_loc + rad; ++l, ++r_l) {      
          if (l >= 0 && l < width) {
            coherence_ptr[l] *= weights[r_l] * weights[r_k];
          }
        }
      }
    }
  }
}


void SurfaceCarve::resolveDisconts (Matrix3D& S, Matrix3D& R, const Matrix3D& C, int seam, 
                                    int first, int last) {

	const int st = last - first;

	IppiSize seamH = { S.sx()-1, st };
	IppiSize seamV = { S.sx(), st-1 };
	int runs =0;
	float maxH, maxV;
	const int max_runs = 10000;

	for ( ;runs < max_runs; ++runs )
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
		
		for ( int i=0; i < st ; ++i) {
			for (int j =0; j < S.sx(); ++j)
			{
				float sum =0;
				int num =0;

				float val = *S.at(j,i,seam);
				float t;

				float weights[5];
				float vals[5];
				float min_val = 1e30;;
				float max_val = -1e30;
				float summed_weight =0;

				if ( i > 0 )
					if ( std::abs ((t = *S.at(j,i-1,seam)) - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j, first+i-1);
						vals[num] = t;

						sum += t;
						++num;
					}

				if ( i < st-1 )
					if ( std::abs ((t= *S.at(j,i+1, seam)) - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j, first+i+1);
						vals[num] = t;
						
						sum +=t; 
						++num;
					}

				if ( j > 0 )
					if ( std::abs ((t = *S.at(j-1,i,seam)) - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j-1, first+i);
						vals[num] = t;
						
						sum += t; 
						++num;
					}

				if ( j < S.sx()-1 )
					if ( std::abs ((t= *S.at(j+1,i, seam)) - val) > 1 )
					{
						weights[num] = 1.0 / *C.at ( t, j+1, first+i);
						vals[num] = t;

						sum +=t; 
						++num;
						
					}

				if ( num > 0 ) {
					weights[num] = 1.0 / *C.at ( val, j, first+i);
					vals[num] = val;
					++num;

					// normalize weights to be in range 1 .. 5;
					for ( int k=0; k < num; ++k )
					{
						// cap values
						if ( weights[k] < 1e-10 )
							weights[k] = 1e-10;
						if ( weights[k] > 1e10 )
							weights[k] = 1e10;

						if ( weights[k] < min_val )
							min_val = weights[k];
						if ( weights[k] > max_val )
							max_val = weights[k];
					}

					sum =0;
					
					for ( int k=0; k < num; ++k )
					{
						if ( std::fabs(max_val - min_val) > 1e-2 )
							weights[k] = ((weights[k] - min_val) / (max_val - min_val) + 1.0) * 50.0;
						else
							weights[k] = 1.0;

						summed_weight += weights[k];
						sum += vals[k] * weights[k];
					}

					*R.at(j,i,0) = std::floor ( sum / summed_weight + 0.5 );
					
					//*R.at(j,i,0) = std::floor ( (val + sum) / num + 0.5);
				}
				else {
					*R.at(j,i,0) = *S.at(j,i,seam);
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

void SurfaceCarve::getSeam (const Matrix3D& C, Matrix3D& S, Matrix3D& M, Matrix3D& I, 
                            Matrix3D& T, int width, int seam, 
                            int first, int last) {
	// test for simple parallelization
	const int st = last-first;
  
	// set M and I zero
	IppiSize roi3D = { width, C.sy() * st };
	IppiSize roiImg = { width, C.sy () };

	ippiSet_32f_C1R (0, M.d(), M.step(), roi3D );
	ippiSet_32f_C1R (0, I.d(), I.step(), roi3D );

	// initialize M and I
	ippiSet_32f_C1R (-1, I.img(0), I.step(), roiImg);

	memcpy ( M.d(), C.img(first), width*sizeof(float) );

	// compute forward seams in first image
  for ( int i =1; i < C.sy(); ++i )
  {
    float* McurRow = M.img(0)+M.ld()*i;
    float* MprevRow = McurRow - M.ld();
    const float* CcurRow = C.img(first)+C.ld()*i;
    
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
	memcpy ( T.d() + T.ld() * (T.sy()-1), C.img(first) + C.ld() * ( C.sy()-1 ), width*sizeof(float));
  
  for ( int i =C.sy()-2; i >= 0; --i )
  {
    float* TcurRow = T.d()+T.ld()*i;
    float* TprevRow = TcurRow + T.ld();
    const float* CcurRow = C.img(first)+C.ld()*i;
    
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
	ippiSub_32f_C1IR ( C.img(first), C.step(), M.d(), M.step(), roiImg );
	//new
	ippiDivC_32f_C1IR ( C.sy(), M.d(), M.step(), roiImg ); 
  
	// traverse images
  int idx;
  float elem;
  float minValAbove;
  int	  minElemAbove;
  std::vector<int> tmpMask;
  tmpMask.reserve(3);
  
  for ( int t=1; t < st; ++t ) {
    // first row
    elem = min2idx ( *M.img(t-1), *(M.img(t-1)+1), idx );
    *I.img(t) = idx;
    *M.img(t) = elem + *C.img(first+t);
    
    for ( int j =1; j < width-1; ++j)
    {
        elem = min3idx ( *(M.img(t-1)+j-1), *(M.img(t-1)+j), *(M.img(t-1)+j+1), idx );
      *(I.img(t)+j) = j+idx;
      *(M.img(t)+j) = elem +*(C.img(first+t)+j);
    }
    
    elem = min2idx ( *(M.img(t-1)+width-2), *(M.img(t-1)+width-1), idx ); 
    *(I.img(t)+width-1) = width-2+idx;
    *(M.img(t)+width-1) = elem + *(C.img(first+t)+width-1);
    
    for ( int i =1; i < C.sy(); ++i )
    {
      float* McurRow = M.img(t)+M.ld()*i;
      float* MprevRow = McurRow - M.ld();
    //  float* CcurRow = C.img(first+t)+C.ld()*i;
      
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
        std::vector<int>::iterator end = std::remove_if(tmpMask.begin(), tmpMask.end(), 
                                                        NotNeighborTo(j));
        
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
        *M.at(j,i,t) = minValAbove + *C.at(j,i,first+t) + elem;
      }
    }
    
    // last row
    float* Mlastrow = M.at(0,M.sy()-1,t-1);
    const float* Clastrow = C.at(0,C.sy()-1,first+t);
    float* Tlastrow = T.at(0,T.sy()-1,0);
    float* IVallastrow = T.at (0, T.sy()-1, 1);
    float* IlastRow = I.at(0,I.sy()-1, t);
    
    *IVallastrow = *(Mlastrow+(int)*IlastRow);
    *Tlastrow    = *IVallastrow + *Clastrow;
    
    for ( int j =1; j < width-1; ++j)
    {
      *(IVallastrow+j) = *(Mlastrow+(int)(*(IlastRow+j)));
      *(Tlastrow+j)    = *(IVallastrow+j) +*(Clastrow+j);
    }
    
    *(IVallastrow+width-1) = *(Mlastrow+(int)(*(IlastRow+width-1)));
    *(Tlastrow+width-1)    = *(IVallastrow+width-1) +*(Clastrow+width-1);
    
    for (int i = C.sy()-2; i >= 0; --i )
    {
      float* TcurRow = T.d()+T.ld()*i;
      float* TprevRow = TcurRow + T.ld();
      const float* CcurRow = C.img(first+t)+C.ld()*i;
      float* IcurRow = I.img(t) + I.ld()*i;
      float* MprevRow = M.img(t-1) + M.ld()*i;
      float* IValcurRow = T.img(1) + T.ld() * i;
      
      // first elem
      *IValcurRow = *(MprevRow+(int)(*IcurRow));
      *TcurRow = std::min ( *TprevRow, *(TprevRow+1)) + *CcurRow + *IValcurRow; 

      for ( int j =1; j < width-1; ++j )
      {
        *(IValcurRow+j) = *(MprevRow+(int)(*(IcurRow+j)));
        *(TcurRow+j) = min3 ( *(TprevRow+j-1), *(TprevRow+j), *(TprevRow+j+1))
        + *(CcurRow+j) + *(IValcurRow+j);
      }
      
      //last elem
        *(IValcurRow+width-1) = *(MprevRow+(int)(*(IcurRow+width-1)));
        *(TcurRow+width-1) = std::min ( *(TprevRow+width-2), *(TprevRow+width-1) ) + 
        + *(CcurRow+width-1) + *(IValcurRow+width-1);
    }
    
    // complete seam costs
    ippiAdd_32f_C1IR ( T.d(), T.step(), M.img(t), M.step(), roiImg );
    ippiSub_32f_C1IR ( C.img(first+t), C.step(), M.img(t), M.step(), roiImg );
    ippiSub_32f_C1IR ( T.img(1), T.step(), M.img(t), M.step(), roiImg );
    
    //new
    ippiDivC_32f_C1IR ( C.sy()*2.0, M.img(t), M.step(), roiImg);
  }

	// compose Seam
	// get minimum
	float* start = M.at(0, M.sy()-1, st-1);
	float* end = M.at(width-1, M.sy()-1, st-1);

	int minIdx = std::min_element ( start, end ) - start;
	
	*S.at ( S.sx()-1, st-1,seam ) = minIdx;
	
	if ( st > 1 )
		*S.at ( S.sx()-1, st-2,seam ) = *I.at(minIdx, I.sy()-1, st-1);

	int idxOffset;
	float* Mcur;

	for ( int i =C.sy()-2; i >=0; --i )
	{
		if ( minIdx == 0 )
		{
			min2idx ( *M.at(0,i,st-1), *M.at(1,i,st-1), minIdx );
		}
		else if ( minIdx == width-1)
		{
			min2idx ( *M.at(width-2,i,st-1), *M.at(width-1,i,st-1), minIdx );
			minIdx += width-2;
		}
		else
		{
			Mcur = M.at(minIdx,i,st-1);
			min3idx ( *(Mcur-1), *Mcur, *(Mcur+1), idxOffset);
			minIdx += idxOffset;
		}

		*S.at ( i, st-1, seam ) = minIdx;
		
		if ( st > 1)
			*S.at ( i, st-2, seam) = *I.at(minIdx,i,st-1);
	}

  for ( int t=st-2; t>=0; --t) {
    for ( int i =0; i < S.sx(); ++i) {
				*S.at(i,t,seam) = *I.at ( *S.at(i,t+1,seam), i, t+1 );
    }
	}
}

float SurfaceCarve::MinVecIdx(float* vec, int sz, int& idx) {
  idx = 0;
  float min_val = vec[0];
  for (int k = 1; k < sz; ++k)
    if (vec[k] < min_val) {
      min_val = vec[k];
      idx = k;
    }
  return min_val;
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
