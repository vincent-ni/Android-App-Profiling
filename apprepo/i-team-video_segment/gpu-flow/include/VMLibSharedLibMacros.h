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
 * Description : shared lib macros for windows dlls
 *
 * Author     :
 * EMail      : urschler@icg.tugraz.at
 *
 */

#ifndef VMLIBSHAREDLIBMACROS_H_
#define VMLIBSHAREDLIBMACROS_H_

#ifdef WIN32
   #pragma warning( disable : 4251 ) // disable the warning about exported template code from stl

   #ifdef VMLIB_USE_STATIC
      #define VMLIB_DLLAPI
   #else
      #ifdef VMLIB_DLL_EXPORTS
         #define VMLIB_DLLAPI __declspec(dllexport)
      #else
         #define VMLIB_DLLAPI __declspec(dllimport)
      #endif
   #endif
#else
   #define VMLIB_DLLAPI
#endif

#endif // VMLIBSHAREDLIBMACROS_H_
