
/********************************************************************************************
* Implementing Graph Cuts on CUDA using algorithm given in CVGPU '08                       ** 
* paper "CUDA Cuts: Fast Graph Cuts on GPUs"                                               **  
*                                                                                          **   
* Copyright (c) 2008 International Institute of Information Technology.                    **  
* All rights reserved.                                                                     **  
*                                                                                          ** 
* Permission to use, copy, modify and distribute this software and its documentation for   ** 
* educational purpose is hereby granted without fee, provided that the above copyright     ** 
* notice and this permission notice appear in all copies of this software and that you do  **
* not sell the software.                                                                   **  
*                                                                                          **
* THE SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,EXPRESS, IMPLIED OR    **
* OTHERWISE.                                                                               **  
*                                                                                          **
* Created By Vibhav Vineet.                                                                ** 
********************************************************************************************/

#ifndef _CUDACUTS_H_
#define _CUDACUTS_H_

/*Header files included*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


//#include "cudaCopy.cu"
//#include "cudaStochastic.cu"

/*****************************************************************
 * CONTROL_M -- this decides after how many iterations          **
 * m should be changed from 1 to 2. Here m equal to 1           **
 * means one push-pull operation followed by one relabel        **
 * operation and m equal to 2 means two push-pull operations    ** 
 * followed by one relabel operation.                           **
 * **************************************************************/

#define CONTROL_M 40

//////////////////////////////////////////////////////////////
//Functions prototypes                                      //
//////////////////////////////////////////////////////////////

/********************************************************************    
 * cudaCutsInit(width, height, numOfLabels) function sets the      **
 * width, height and numOfLabels of grid. It also initializes the  **
 * block size  on the device and finds the total number of blocks  **
 * running in parallel on the device. It calls checkDevice         **
 * function which checks whether CUDA compatible device is present **
 * on the system or not. It allocates the memory on the host and   **
 * the device for the arrays which are required through the        **
 * function call h_mem_init and segment_init respectively. This    **
 * function returns 0 on success or -1 on failure if there is no   **
 * * * CUDA compatible device is present on the system             **
 * *****************************************************************/

int cudaCutsInit( int, int, int );

/**************************************************
 * function checks whether any CUDA compatible   **
 * device is present on the system or not. It    **
 * returns the number of devices present on the  **
 * system.                                       **
 * ***********************************************/

int checkDevice();

/**************************************************
 * h_mem_init returns allocates and intializes   **
 * memory on the host                            **
 * ***********************************************/

void h_mem_init();

/***************************************************************
 * This function allocates memory for n-edges, t-edges,       **
 * height and mask function, pixelLabels and intializes them  **
 * on the device.                                             **
 * ************************************************************/

void d_mem_init();

/********************************************************
 * This function copies the dataTerm from the host to  **
 * device and also copies the data into datacost array **
 * of size width * height * numOfLabels                **  
 * *****************************************************/

int cudaCutsSetupDataTerm(int *);

/*************************************************************
 * This function copies the smoothnessTerm from the host to  **
 * device and also copies the data into smoothnesscost array **
 * of size numOfLabels * numOfLabels                         **  
 * ***********************************************************/

int cudaCutsSetupSmoothTerm(int *);

/*************************************************************
 * As in our case, when the graph is grid, horizotal and    **
 * vertical cues can be specified. The hcue and vcue array  **
 * of size width * height stores these respectively.        ** 
 * ***********************************************************/

int cudaCutsSetupHCue(int *);
int cudaCutsSetupVCue(int *);

/*********************************************************
 * This function constructs the graph on the device.    **
 * ******************************************************/

int cudaCutsSetupGraph();

/************************************************************
 * The function calls the Cuda Cuts optimization algorithm **
 * and the bfs algorithm so as to assign a label to each   **
 * pixel                                                   ** 
 * *********************************************************/

int cudaCutsNonAtomicOptimize();

/***********************************************************
 * This function calls three kernels which performs the   **
 * push, pull and relabel operation                       ** 
 * ********************************************************/

void cudaCutsNonAtomic( );

/**********************************************************
 * This finds which of the nodes are in source set and   **
 * sink set                                              ** 
 * *******************************************************/

void bfsLabeling();

/****************************************************************
 * This function assigns a label to each pixel and stores them ** 
 * in pixelLabel array of size width * height                  ** 
 * *************************************************************/

int cudaCutsGetResult();

/************************************************************
 * De-allocates all the memory allocated on the host and   **
 * the device.                                             ** 
 * *********************************************************/

void cudaCutsFreeMem();

/***********************************************************
 * These functions calculates the total energy of the     **
 * configuration                                          **
 * ********************************************************/

int cudaCutsGetEnergy( );
int data_energy();
int smooth_energy();

int GraphSize();
int GridWidth();
int GridHeight();
int* PixelLabel();

#endif
