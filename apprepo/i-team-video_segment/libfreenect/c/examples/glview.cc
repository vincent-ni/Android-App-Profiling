/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>
#include "libfreenect.h"

#include <pthread.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <math.h>
#include <iostream>
#include <vector>
#include <utility>
#include <cv.h>

pthread_t gl_thread;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t gl_depth_front[640*480*4];
uint8_t gl_depth_back[640*480*4];

float depth_values_back[640 * 480];
float depth_values_front[640 * 480];
float depth_values_warped[640 * 480];

uint8_t gl_rgb_front[640*480*4];
uint8_t gl_rgb_back[640*480*4];

float mesh_vertices[640 * 480 * 3];
float mesh_vertices_filtered[640 * 480 * 3];
float mesh_normals[640 * 480 * 3];
bool mesh_valid[640 * 480];

GLuint gl_depth_tex;
GLuint gl_rgb_tex;

freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;

pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_frames = 0;

// UI functionality.
float global_x = -0.5;
float global_z = -0.5;

float center_x = 0;
float center_y = 0;

int prev_mouse_x; 
int prev_mouse_y;

CvMat* map_x;
CvMat* map_y;

float DepthTransform(float depth) {
  return pow(depth/2048, 4) * 30;
}

// Does not use or filter border. Just proof of concept implementation.
void FilterMeshBilateral(const float* input, int lda, int width, int height,
                         float sigma_space, float sigma_value, 
                         float* output) {
  const int radius = sigma_space * 0.75;
  const int diam = 2*radius + 1;
  const int cn = 3;
  
  // Calculate space offsets and weights.
  std::vector<int> space_ofs(diam*diam);
  std::vector<float> space_weights(diam*diam);
  int space_sz = 0;
  const float space_coeff = -0.5 / (sigma_space * sigma_space);
  
  for (int i = -radius; i <= radius; ++i) {
    for (int j = -radius; j <= radius; ++j) {
      int r2 = i*i + j*j;
      if (r2 > radius*radius)
        continue;
      
      space_ofs[space_sz] = i * lda + j * cn;
      space_weights[space_sz++] = exp(space_coeff * (float)r2);
    }
  }
  
  // Compute color exp-weight lookup table.
  const float min_val = 0;
  const float max_val = 0.2;
  const float diff_range = std::max(1e-3, (max_val - min_val)*(max_val - min_val) * 1.02);
  
  const int num_bins = (1 << 12);
  const float scale = (float) num_bins / diff_range;
  
  std::vector<float> expLUT(num_bins);
  const float color_coeff = -0.5 / (sigma_value * sigma_value);
  bool zero_reached = false;
  for (int i = 0; i < num_bins; ++i) {
    if (!zero_reached) {
      expLUT[i] = exp( (float)i / scale * color_coeff);
      zero_reached = (expLUT[i] < 1e-10);
    } else {
      expLUT[i] = 0;
    }
  }
  
  for (int i = radius; i < height - radius; ++i) {
    const float* src_ptr = input + i * lda + 3 * radius;
    float* dst_ptr = output + i * lda + 3 * radius;
    const float* local_ptr;
    
    for (int j = radius; j < width - radius; ++j, src_ptr += 3, dst_ptr += 3) {
      float my_val = src_ptr[2];
      float weight, weight_sum = 0, val_sum = 0, val_diff;
      
      for (int k = 0; k < space_sz; ++k) {
        local_ptr = src_ptr + space_ofs[k];
        val_diff = my_val - local_ptr[2];
        val_diff *= val_diff;
        
        if (val_diff > 0.04) {
          weight = 0;
        } else {
          weight = space_weights[k] * expLUT[int(val_diff * scale)];
        }
         
        weight_sum += weight;
        
        val_sum += local_ptr[2] * weight;
      }
      
      dst_ptr[2] = val_sum / weight_sum;
    }
  }
}

void DrawGLScene() {
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_VERTEX_ARRAY);
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(0, 0, 1.0, center_x, center_y, 0.0, 0, 1, 0);
  
	pthread_mutex_lock(&gl_backbuf_mutex);
  
	while (got_frames < 2) {
		pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
	}
  
	memcpy(depth_values_front, depth_values_back, sizeof(depth_values_back));
	memcpy(gl_rgb_front, gl_rgb_back, sizeof(gl_rgb_back));
	got_frames = 0;
	pthread_mutex_unlock(&gl_backbuf_mutex);
  
  // Warp depth to CS of rgb camera.
  CvMat depth;
  CvMat depth_warped;
  cvInitMatHeader(&depth, 480, 640, CV_32F, depth_values_front, 
                  640 * sizeof(float));
  cvInitMatHeader(&depth_warped, 480, 640, CV_32F, depth_values_warped, 
                  640 * sizeof(float));
  cvRemap(&depth, &depth_warped, map_x, map_y, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS,
          cvScalarAll(-2048.0));
  
  // Setup mesh.
  for (int i = 0; i < 480; ++i) {
    float* mesh_ptr = mesh_vertices + i * 640 * 3;
    bool* valid_ptr = mesh_valid + i * 640;
    for (int j = 0; j < 640; ++j, mesh_ptr += 3, ++valid_ptr) {
      const float depth = depth_values_warped[i * 640 + j];
      const float x = (float) j / 640.f;
      const float y = 1.f - (float) i / 480.f;
      
      if (depth < 2000) {
        mesh_ptr[0] = x;
        mesh_ptr[1] = y;
        mesh_ptr[2] = DepthTransform(depth);
        *valid_ptr = true;
      } else {
        mesh_ptr[0] = x;
        mesh_ptr[1] = y;
        mesh_ptr[2] = 60;
        *valid_ptr = false;
      }
    }
  }

  memcpy(mesh_vertices_filtered, mesh_vertices, 640 * 480 * 3 * sizeof(float));
  FilterMeshBilateral(mesh_vertices, 640 * 3, 640, 480, 5, 0.05, mesh_vertices_filtered);
  for (int i = 0; i < 480; ++i) {
    float* row_ptr = mesh_vertices_filtered + i * 640 * 3;
    for (int j = 0; j < 640; ++j, row_ptr += 3) {
      row_ptr[2] *= -1.0;
    }
  }
  
  // Compute normals.
  for (int i = 0; i < 480; ++i) {
    float* row_ptr = mesh_normals + i * 640 * 3;
    
    if (i == 0 || i == 479) {
      for (int j = 0; j < 640; ++j, row_ptr += 3) {
        row_ptr[0] = row_ptr[1] = 0;
        row_ptr[2] = 1.0f;
      }
      
      continue;
    }
    
    row_ptr[0] = row_ptr[1] = 0;
    row_ptr[2] = 1;
    row_ptr += 3;
    
    float* mesh_ptr = mesh_vertices_filtered + i * 640 * 3 + 3;
    bool* valid_ptr = mesh_valid + i * 640 + 1;
    for (int j = 1; j < 639; ++j, row_ptr += 3, mesh_ptr += 3, ++valid_ptr) {      
      const float dx = (mesh_ptr[5] - mesh_ptr[-1]) * 150;
      const float dy = (mesh_ptr[640 * 3 + 2] - mesh_ptr[-640 * 3 + 2]) * 150;
      float inv_norm = 1.0 / sqrt(1 + dx * dx + dy * dy);
      
      // Reject if depth discontinuity.
      const float cut_off = 5;
      if (fabs(dx) > cut_off || fabs(dy) > cut_off) {
        *valid_ptr = false;
      }
      
      row_ptr[0] = dx * inv_norm;
      row_ptr[1] = dy * inv_norm;
      row_ptr[2] = inv_norm;
    }

    row_ptr[0] = row_ptr[1] = 0;
    row_ptr[2] = 1;
  }
  
  
  glTranslatef(global_x, -0.5, global_z);
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT, GL_AMBIENT);
  
	glBegin(GL_TRIANGLES);
  const int width_step = 640 * 3;
  
  for (int i = 0; i < 479; ++i) {
    const float* mesh_ptr = mesh_vertices_filtered + i * width_step;
    const float* norm_ptr = mesh_normals + i * width_step;
    GLubyte* color_ptr = (GLubyte*)gl_rgb_front + i * width_step;
    
    const bool* valid_ptr = mesh_valid + i * 640;
    
    for (int j = 0;
         j < 639;
         ++j, mesh_ptr += 3, norm_ptr += 3, color_ptr += 3, ++valid_ptr) {
      if (!valid_ptr[0] || !valid_ptr[1] || !valid_ptr[640] || !valid_ptr[641]) {
        continue;
      }
      
      glColor3ubv(color_ptr);
      glNormal3fv(norm_ptr);
      glVertex3fv(mesh_ptr);

      glColor3ubv(color_ptr + width_step);
      glNormal3fv(norm_ptr + width_step);
      glVertex3fv(mesh_ptr + width_step);
      
      glColor3ubv(color_ptr + width_step + 3);
      glNormal3fv(norm_ptr + width_step + 3);     
      glVertex3fv(mesh_ptr + width_step + 3);

      glColor3ubv(color_ptr + width_step + 3);
      glNormal3fv(norm_ptr + width_step + 3);     
      glVertex3fv(mesh_ptr + width_step + 3);

      glColor3ubv(color_ptr + 3);
      glNormal3fv(norm_ptr + 3);
      glVertex3fv(mesh_ptr + 3);
      
      glColor3ubv(color_ptr);
      glNormal3fv(norm_ptr);
      glVertex3fv(mesh_ptr);
    }
  }
  glEnd();
  
	glutSwapBuffers();
}

void mousePressed(int button, int state, int x, int y) {
  if (state == GLUT_DOWN) {
    prev_mouse_x = x;
    prev_mouse_y = y;
  }
}

void mouseMoved(int x, int y) {
  float dx = x - prev_mouse_x;
  float dy = y - prev_mouse_y;
  
  center_x += dx / 300;
  center_y -= dy / 300;
  
  prev_mouse_x = x;
  prev_mouse_y = y;
}

void keyPressed(unsigned char key, int x, int y)
{
	if (key == 27) {
		glutDestroyWindow(window);
    die = 1;
    exit(0);
	}
  
	if (key == 'i') {
		freenect_angle++;
		if (freenect_angle > 30) {
			freenect_angle = 30;
		}
	}
  
	if (key == 'p') {
		freenect_angle = 0;
	}
	if (key == 'o') {
		freenect_angle--;
		if (freenect_angle < -30) {
			freenect_angle = -30;
		}
	}
  
  if (key == 'w') {
    global_z += 0.1;
  }
  
  if (key == 's') {
    global_z -= 0.1;
  }

  if (key == 'a') {
    global_x += 0.1;
  }
  
  if (key == 'd') {
    global_x -= 0.1;
  }
  
	if (key == '1') {
		freenect_set_led(f_dev,LED_GREEN);
	}
	if (key == '2') {
		freenect_set_led(f_dev,LED_RED);
	}
	if (key == '3') {
		freenect_set_led(f_dev,LED_YELLOW);
	}
	if (key == '4') {
		freenect_set_led(f_dev,LED_BLINK_YELLOW);
	}
	if (key == '5') {
		freenect_set_led(f_dev,LED_BLINK_GREEN);
	}
	if (key == '6') {
		freenect_set_led(f_dev,LED_BLINK_RED_YELLOW);
	}
	if (key == '0') {
		freenect_set_led(f_dev,LED_OFF);
	}
	freenect_set_tilt_in_degrees(f_dev,freenect_angle);
}

void ReSizeGLScene(int Width, int Height)
{
	glViewport(0,0,Width,Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(40.0, (float)Width / (float)Height,
                 0.01, 10.0);
  
	glMatrixMode(GL_MODELVIEW);
}

void InitGL(int Width, int Height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0);
  glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);
	glGenTextures(1, &gl_depth_tex);
	glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenTextures(1, &gl_rgb_tex);
	glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  GLfloat light_diffuse[] = {.65, .65, .65, 1.0};  
  GLfloat light_position[] = {1.0, 1.0, 1.0, 0.0};  

  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  GLfloat light_ambient[] = { .2, .2, .2, 1};
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
  
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHTING);
  
	ReSizeGLScene(Width, Height);
}

void *gl_threadfunc(void *arg)
{
	printf("GL thread\n");
  
	glutInit(&g_argc, g_argv);
  
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(1280, 960);
	glutInitWindowPosition(0, 0);
  
	window = glutCreateWindow("LibFreenect");
  
	glutDisplayFunc(&DrawGLScene);
	glutIdleFunc(&DrawGLScene);
	glutReshapeFunc(&ReSizeGLScene);
	glutKeyboardFunc(&keyPressed);
  glutMouseFunc(&mousePressed);
  glutMotionFunc(&mouseMoved);
  
	InitGL(1280, 960);
  
	glutMainLoop();
  
	pthread_exit(NULL);
	return NULL;
}

uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, freenect_depth *depth, uint32_t timestamp)
{
	int i;
  
	pthread_mutex_lock(&gl_backbuf_mutex);
	for (i=0; i<FREENECT_FRAME_PIX; i++) {
    depth_values_back[i] = (float) depth[i];
  }
  
	got_frames++;
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp) {
  
	pthread_mutex_lock(&gl_backbuf_mutex);
	got_frames++;
	memcpy(gl_rgb_back, rgb, FREENECT_RGB_SIZE);
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void* kinect_threadfunc(void *arg) {
	freenect_context *f_ctx;
  
	printf("Kinect camera test\n");
  
	int i;
	for (i=0; i<2048; i++) {
		float v = i/2048.0;
		v = powf(v, 3)* 6;
		t_gamma[i] = v*6*256;
	}
  
	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
    pthread_exit(NULL);
    return NULL;
	}
  
	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);
	
	int user_device_number = 0;
	if (g_argc > 1)
		user_device_number = atoi(g_argv[1]);
	
  
	if (nr_devices < 1) {
    pthread_exit(NULL);
	  return NULL;
  }
  
	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
		printf("Could not open device\n");
    pthread_exit(NULL);
    return NULL;
	}
  
  freenect_set_tilt_in_degrees(f_dev,freenect_angle);
  freenect_set_led(f_dev,LED_RED);
	freenect_set_depth_callback(f_dev, depth_cb);
	freenect_set_rgb_callback(f_dev, rgb_cb);
	freenect_set_rgb_format(f_dev, FREENECT_FORMAT_RGB);
	freenect_set_depth_format(f_dev, FREENECT_FORMAT_11_BIT);  
  
  freenect_start_depth(f_dev);
	freenect_start_rgb(f_dev);
  
	printf("'w'-tilt up, 's'-level, 'x'-tilt down, '0'-'6'-select LED mode\n");
  
	while(!die && freenect_process_events(f_ctx) >= 0 )
	{
		int16_t ax,ay,az;
		freenect_get_raw_accelerometers(f_dev, &ax, &ay, &az);
		double dx,dy,dz;
		freenect_get_mks_accelerometers(f_dev, &dx, &dy, &dz);
		printf("\r raw acceleration: %4d %4d %4d  mks acceleration: %4f %4f %4f", ax, ay, az, dx, dy, dz);
		fflush(stdout);
	}
  
	printf("-- done!\n");  
}

int main(int argc, char **argv) {
  int res;
  
  g_argc = argc;
	g_argv = argv;
  
  map_x = cvCreateMat(480, 640, CV_32F);
  map_y = cvCreateMat(480, 640, CV_32F);
  
  for (int i = 0; i < 480; ++i) {
    float* x_ptr = (float*)(map_x->data.ptr + map_x->step * i);
    float* y_ptr = (float*)(map_y->data.ptr + map_y->step * i);
    
    for (int j = 0; j < 640; ++j, ++x_ptr, ++y_ptr) {
      *x_ptr = 1.079 * (float)j - 0.0001 * (float)i - 12.53;
      *y_ptr = 0.0055 * (float)j + 1.0913 * (float)i - 44.16;
    }
  }
  
	res = pthread_create(&gl_thread, NULL, kinect_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		return 1;
	}
	
  gl_threadfunc(NULL);
	  
  cvReleaseMat(&map_x);
  cvReleaseMat(&map_y);
	pthread_exit(NULL);
}