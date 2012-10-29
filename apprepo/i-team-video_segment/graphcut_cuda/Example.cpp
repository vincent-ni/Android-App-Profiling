
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

#include "Example.h"
#include "CudaCuts.h"
#include <algorithm>
#include <iostream>
#include <iterator>

using namespace std; 

int main(int argc,char** argv)
{
  int width, height, num_Labels;
  int *dataTerm, *smoothTerm, *hCue, *vCue;
    
	load_files("data/flower.txt", width, height, num_Labels, dataTerm, smoothTerm, hCue, vCue);

	int initCheck = cudaCutsInit(width, height ,num_Labels) ;
	
	printf("Compute Capability %d\n",initCheck);

	if( initCheck > 0 )
	{
		printf("The grid is initialized successfully\n");
	}
	else 
		if( initCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}
	

	int dataCheck   =  cudaCutsSetupDataTerm( dataTerm );

	if( dataCheck == 0 )
	{
		printf("The dataterm is set properly\n");
		
	}
	else 
		if( dataCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}


	int smoothCheck =  cudaCutsSetupSmoothTerm( smoothTerm );


	if( smoothCheck == 0 )
	{
		printf("The smoothnessterm is set properly\n");
	}
	else
		if( smoothCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}
	

	int hcueCheck   =  cudaCutsSetupHCue( hCue );

	if( hcueCheck == 0 )
	{
		printf("The HCue is set properly\n");
	}
	else
		if( hcueCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}

	int vcueCheck   =  cudaCutsSetupVCue( vCue );


	if( vcueCheck == 0 )
	{
		printf("The VCue is set properly\n");
	}
	else 
		if( vcueCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}


	int graphCheck = cudaCutsSetupGraph();

	if( graphCheck == 0 )
	{
		printf("The graph is constructed successfully\n");
	}
	else 
		if( graphCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}

	int optimizeCheck = -1; 
	if( initCheck == 1 )
	{
		//CudaCuts involving atomic operations are called
		optimizeCheck = cudaCutsNonAtomicOptimize();
		//CudaCuts involving stochastic operations are called
		//optimizeCheck = cudaCutsStochasticOptimize();
	}


	if( optimizeCheck == 0 )
	{
		printf("The algorithm successfully converged\n");
	}
	else 
		if( optimizeCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}

	int resultCheck = cudaCutsGetResult( );

	if( resultCheck == 0 )
	{
		printf("The pixel labels are successfully stored\n");
	}
	else 
		if( resultCheck == -1 )
		{
			printf("Error: Please check the device present on the system\n");
		}
		
	int energy = cudaCutsGetEnergy(); 

  for (int i = 0; i < height; ++i) {
    
    for (int j = 0; j < width; ++j) {
      int c1 = dataTerm[2*width*i + 2*j];
      int c2 = dataTerm[2*width*i + 2*j + 1];
      PixelLabel()[i*GridWidth() + j] = c1 < c2 ? 0 : 1;
    }
  }
  

	initFinalImage(width, height);
	
	cudaCutsFreeMem();
	
	exit(1);
//	CUT_EXIT(argc,argv);
}


void load_files(char *filename, int &width, int &height, int &nLabels,
		int *&dataCostArray,
		int *&smoothCostArray,
		int *&hCue,
		int *&vCue)
{
	LoadDataFile(filename, width, height, nLabels, dataCostArray, smoothCostArray, hCue, vCue);

}

void initFinalImage(int width, int height)
{
	out_pixel_values=(int**)malloc(sizeof(int*)*height);

	for(int i = 0 ; i < height ; i++ )
	{
		out_pixel_values[i] = (int*)malloc(sizeof(int) * width ) ;
		for(int j = 0 ; j < width ; j++ ) {
			out_pixel_values[i][j]=0;
		}
	}

	writeImage(width, height) ;
}

void writeImage(int width, int height)
{
	for(int i = 0 ; i < GraphSize() ; i++)
	{
		int row = i / GridWidth(), col = i % GridWidth();

		if(row >= 0 && col >= 0 && row <= height -1 && col <= width - 1 )
			out_pixel_values[row][col]=PixelLabel()[i]*255;
	}

	write_image(width, height);
}

void write_image(int width, int height)
{

	FILE* fp=fopen("result_sponge/flower_cuda_test.pgm","w");

	fprintf(fp,"%c",'P');
	fprintf(fp,"%c",'2');
	fprintf(fp,"%c",'\n');
	fprintf(fp,"%d %c %d %c ",width,' ',height,'\n');
	fprintf(fp,"%d %c",255,'\n');

	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			fprintf(fp,"%d\n",out_pixel_values[i][j]);
		}
	}
	fclose(fp);
}



void LoadDataFile(char *filename, int &width, int &height, int &nLabels,
		int *&dataCostArray,
		int *&smoothCostArray,
		int *&hCue,
		int *&vCue)
{
	printf("enterd\n");
	
	FILE *fp = fopen(filename,"r");
	
	fscanf(fp,"%d %d %d",&width,&height,&nLabels);

	int i, n, x, y;
	int gt;
	for(i = 0; i < width * height; i++)
		fscanf(fp,"%d",&gt);

	dataCostArray = new int[width * height * nLabels];
	for(int c=0; c < nLabels; c++) {
		n = c;
		for(i = 0; i < width * height; i++) {
			fscanf(fp,"%d",&dataCostArray[n]);
			n += nLabels;
		}
	}

	hCue = new int[width * height];
	vCue = new int[width * height];

	n = 0;
	for(y = 0; y < height; y++) {
		for(x = 0; x < width-1; x++) {
			fscanf(fp,"%d",&hCue[n++]);
		}
		hCue[n++] = 0;
	}

	n = 0;
	for(y = 0; y < height-1; y++) {
		for(x = 0; x < width; x++) {
			fscanf(fp,"%d",&vCue[n++]);
		}
	}
	for(x = 0; x < width; x++) {
		vCue[n++] = 0;
	}

	fclose(fp);
	smoothCostArray = new int[nLabels * nLabels];
  
  // Print some sample values.
  cout << "\n\n\n\n>>Sample data:\n";
  copy(dataCostArray, dataCostArray + 50, std::ostream_iterator<int>(std::cout, " "));
  
  cout << "\n\n\n\n>>Sample hCue:\n";
  copy(hCue, hCue + 50, std::ostream_iterator<int>(std::cout, " "));

  cout << "\n\n\n\n>>Sample vCue:\n";
  copy(vCue, vCue + 50, std::ostream_iterator<int>(std::cout, " "));

	smoothCostArray[0] = 0 ;
	smoothCostArray[1] = 1 ;
	smoothCostArray[2] = 1 ;
	smoothCostArray[3] = 0 ;


}



