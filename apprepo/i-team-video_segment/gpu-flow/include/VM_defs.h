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
 * Project     : vmgpu
 * Module      : VM Lib
 * Class       : $RCSfile$
 * Language    : C++/CUDA
 * Description : Deffinitions for VMLib
 *
 * Author     :
 * EMail      : unger@icg.tugraz.at
 *
 */

#ifndef VM_DEFS_H
#define VM_DEFS_H

//##############################################################################
//!                                 Includes
//##############################################################################
// CUDA Templates
#include <cudatemplates/copy.hpp>
#include <cudatemplates/devicememorypitched.hpp>


//##############################################################################
//!                                  PUBLIC
//##############################################################################

////////////////////////////////////////////////////////////////////////////////
//! Parameter deffinitions
////////////////////////////////////////////////////////////////////////////////
// Models (Energy functionals)
#define TV_L2            0
#define TV_L1            1
#define TV_L1_MIN        2
#define gTV_L1           3
#define gTV_L1_MIN       4
#define gTV_L1_MAX       5
#define gTV_L1_INP       6
#define gTV_L1_MAX_INP   7
#define wTV_MS           8
#define SO_L2            9
#define SO_L2_MIN       10
#define SO_L2_MAX       11
#define SO_INP          12
#define TV_INP          13
#define gTV_INP         14
#define D_INP           15
#define wTV_L2          16
#define ADTV_L2         17
#define aniWTV_MS       18
#define NO_MOD          99

#define NUM_MODELS      19

// Algorithms (Solvers)
#define VFP      0
#define PDU      1
#define CFP      2
#define CPG      3
#define AUG      4
#define IPD      5
#define QP       6
#define NO_ALG   9

#define NUM_ALGORITHMS 7

// Image type (determined automatically)
#define GREY_2D  0
#define COLOR_2D 1
#define GREY_3D  2
#define NO_IMG   9

#define NUM_MODES      3

//##############################################################################
//!                                  INTERNAL
//##############################################################################

////////////////////////////////////////////////////////////////////////////////
//! Structs
////////////////////////////////////////////////////////////////////////////////

// Algorithm selection
struct Algorithm{
  // members
  int mode;
  int model;
  int algorithm;

  // constructors
  Algorithm()
    : mode(NO_IMG), model(NO_MOD), algorithm(NO_ALG) {}
  Algorithm(int m, int mo, int alg)
    : mode(m), model(mo), algorithm(alg) {}

  // less operator
  bool operator<(Algorithm const & other) const {
    return mode+model*100+algorithm*10000 < other.mode+other.model*100+other.algorithm*10000;
  }
  // equality operator
  bool operator==(Algorithm const & other) const {
    return ((mode == other.mode) && (model == other.model) && (algorithm == other.algorithm));
  }
};

// Data structures
#define NO_STRUCT  99

#define TV_L2_2D_PRIMAL 0
struct TV_L2_2D_Primal {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* g;
  // Parameters:
  float lambda;
};

#define TV_L2_2D_DUAL 1
struct TV_L2_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  float min_fact;
  unsigned int iterations;
};

#define TV_L2_2D_AUG  2
struct TV_L2_2D_Aug {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* rhs;
  Cuda::DeviceMemory<float, 2>* bx;
  Cuda::DeviceMemory<float, 2>* by;
  Cuda::DeviceMemory<float, 2>* dx;
  Cuda::DeviceMemory<float, 2>* dy;
  // Parameters:
  float lambda;
};

#define TV_L1_2D_PRIMAL 3
struct TV_L1_2D_Primal {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* g;
  Cuda::DeviceMemory<float, 2>* h;
  // Parameters:
  float lambda;
};

#define TV_L1_2D_DUAL 4
struct TV_L1_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* q;
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Parameters:
  float lambda;
  // Additional parameters: (not used by all algorithms)
  float alpha;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define TV_L1_2D_AUG 5
struct TV_L1_2D_Aug {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* v;
  Cuda::DeviceMemory<float, 2>* bv;
  Cuda::DeviceMemory<float, 2>* bx;
  Cuda::DeviceMemory<float, 2>* by;
  Cuda::DeviceMemory<float, 2>* dx;
  Cuda::DeviceMemory<float, 2>* dy;
  Cuda::DeviceMemory<float, 2>* rhs;
  // Parameters:
  float lambda;
};

#define TV_L2_3D_DUAL 6
struct TV_L2_3D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 3>* f;
  Cuda::DeviceMemory<float, 3>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 3>* p1;
  Cuda::DeviceMemory<float, 3>* p2;
  Cuda::DeviceMemory<float, 3>* p3;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  float min_fact;
  unsigned int iterations;
};

#define TV_L1_3D_DUAL 7
struct TV_L1_3D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 3>* f;
  Cuda::DeviceMemory<float, 3>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 3>* v;
  Cuda::DeviceMemory<float, 3>* p1;
  Cuda::DeviceMemory<float, 3>* p2;
  Cuda::DeviceMemory<float, 3>* p3;
  // Parameters:
  float lambda;
};

#define wTV_MS_2D_DUAL 8
struct wTV_MS_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define SO_L2_2D_DUAL 9
struct SO_L2_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  Cuda::DeviceMemory<float, 2>* p3;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  float min_fact;
  unsigned int iterations;
};

#define SO_INP_2D_DUAL 10
struct SO_INP_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  Cuda::DeviceMemory<float, 2>* p3;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};


#define wTV_L2_2D_DUAL 11
struct wTV_L2_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define SO_INP_2D_ALT 12
struct SO_INP_2D_Alt {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* v;
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  Cuda::DeviceMemory<float, 2>* p3;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  float theta;
  unsigned int iterations;
};

#define TV_INP_2D_DUAL 13
struct TV_INP_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Additional Parameters (not needed by all models)
  float alpha;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define INP_2D_PRIMAL 14
struct INP_2D_Primal {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal parameters:
  unsigned int iterations;
};

#define ADTV_L2_2D_QP 15
struct ADTV_L2_2D_Qp {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* xi1;
  Cuda::DeviceMemory<float, 2>* xi2;
  Cuda::DeviceMemory<float, 2>* xi3;
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  Cuda::DeviceMemory<float, 2>* p3;
  Cuda::DeviceMemory<float, 2>* q1;
  Cuda::DeviceMemory<float, 2>* q2;
  // Parameters:
  float alpha;
  float beta;
  // Internal parameters:
  float theta;
  unsigned int cg_iterations;
  unsigned int iterations;
};


#define TV_L1_INP_2D_DUAL 16
struct TV_L1_INP_2D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 2>* f;
  Cuda::DeviceMemory<float, 2>* u;
  Cuda::DeviceMemory<float, 2>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 2>* q;
  Cuda::DeviceMemory<float, 2>* p1;
  Cuda::DeviceMemory<float, 2>* p2;
  // Parameters:
  float lambda;
  // Additional parameters: (not used by all algorithms)
  float alpha;
  float delta;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define TV_L2_2DCOL_DUAL 17
struct TV_L2_2DCol_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 3>* f;
  Cuda::DeviceMemory<float, 3>* u;
  // Internal Images:
  Cuda::DeviceMemory<float, 3>* p1;
  Cuda::DeviceMemory<float, 3>* p2;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  float min_fact;
  unsigned int iterations;
};

#define wTV_MS_3D_DUAL 18
struct wTV_MS_3D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 3>* f;
  Cuda::DeviceMemory<float, 3>* u;
  Cuda::DeviceMemory<float, 3>* g;
  // Internal Images:
  Cuda::DeviceMemory<float, 3>* p1;
  Cuda::DeviceMemory<float, 3>* p2;
  Cuda::DeviceMemory<float, 3>* p3;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#define aniWTV_MS_3D_DUAL 19
struct aniWTV_MS_3D_Dual {
  // IO Images:
  Cuda::DeviceMemory<float, 3>* f;
  Cuda::DeviceMemory<float, 3>* u;
  Cuda::DeviceMemory<float, 3>* g;
  Cuda::DeviceMemory<float, 3>* gz;
  // Internal Images:
  Cuda::DeviceMemory<float, 3>* p1;
  Cuda::DeviceMemory<float, 3>* p2;
  Cuda::DeviceMemory<float, 3>* p3;
  // Parameters:
  float lambda;
  // Internal parameters:
  float tau_primal;
  float tau_dual;
  unsigned int iterations;
};

#endif //VM_DEFS_H

