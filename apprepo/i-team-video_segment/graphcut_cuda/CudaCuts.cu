


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

#include "CudaCuts.h"
#include "CudaCuts_kernel.cu"
#include <cutil.h>
#include <iostream>

////////////////////////////////////////////////////////////
//Global Variables declared                               //
////////////////////////////////////////////////////////////

/*************************************************
 * n-edges and t-edges                          **
 * **********************************************/

int *d_left_weight, *d_right_weight, *d_down_weight, *d_up_weight, *d_push_reser, *d_sink_weight;
int *s_left_weight, *s_right_weight, *s_down_weight, *s_up_weight, *s_push_reser, *s_sink_weight;
int *d_pull_left, *d_pull_right, *d_pull_down, *d_pull_up;

int *d_stochastic,  *d_stochastic_pixel ; 

/*************************************************
 * Emergu parameters stored                     **
 * **********************************************/

int *hcue, *vcue, *datacost, *smoothnesscost ;
int *dataTerm, *smoothTerm, *hCue, *vCue ;
int *dDataTerm, *dSmoothTerm, *dHcue, *dVcue, *dPixelLabel ;


/*************************************************
 * Height and mask functions are stored         **
 * **********************************************/

int  *d_relabel_mask, *d_graph_heightr, *d_graph_heightw;

/*************************************************
 * Grid and Block parameters                    **
 * **********************************************/

int graph_size, size_int, width, height, graph_size1, width1, height1, depth, num_Labels;
int blocks_x, blocks_y, threads_x, threads_y, num_of_blocks, num_of_threads_per_block ;

// Helper functions.
int GraphSize() { return graph_size1; }
int GridWidth() { return width1; }
int GridHeight() { return height1; }

/***************************************************
 * Label of each pixel is stored in this function **
 * *************************************************/

int *pixelLabel;
int* PixelLabel() { return pixelLabel; }

bool *d_pixel_mask, h_over, *d_over, *h_pixel_mask ;
int *d_counter, *h_graph_height ;
int *h_reset_mem ;
int cueValues, deviceCheck, deviceCount ;

int *h_stochastic, *h_stochastic_pixel, *h_relabel_mask ; 
int counter ;

#define datacost(pix,lab)     (datacost[(pix)*num_Labels+(lab)] )
#define smoothnesscost(lab1,lab2) (smoothnesscost[(lab1)+(lab2)*num_Labels] )

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

int cudaCutsInit(int widthGrid, int heightGrid, int labels)
{
	deviceCount = checkDevice() ; 	
	
	printf("No. of devices %d\n",deviceCount);
	if( deviceCount < 1 )
		return -1 ; 
	
	int cuda_device = 0;
	
	cudaSetDevice( cuda_device );

	cudaDeviceProp device_properties;

	CUDA_SAFE_CALL( cudaGetDeviceProperties(&device_properties, cuda_device) );
	printf("%d %d\n",device_properties.major, device_properties.minor);
	if( (1 == device_properties.major) && (device_properties.minor < 0))
		deviceCheck = 2 ; 
	else 
	if( (1 == device_properties.major) && (device_properties.minor >= 0))
		deviceCheck = 1 ; 
	else 
		deviceCheck = 0 ; 

	width =  widthGrid ; 
	height = heightGrid ; 
	num_Labels = labels ; 

	blocks_x = 1 ;
	blocks_y = 1 ;
	num_of_blocks = 1 ; 

	num_of_threads_per_block = 256 ; 
	threads_x = 32 ; 
	threads_y = 8 ;  

	width1 = threads_x * ((int)ceil((float)width/ (float)threads_x))  ;  
	height1 = threads_y * ( (int)ceil((float) height / (float) threads_y ))   ; 

	graph_size = width * height ; 
	graph_size1 = width1 * height1 ; 
	size_int = sizeof(int) * graph_size1 ; 

	blocks_x = (int)((ceil)((float)width1/(float)threads_x));
	blocks_y = (int)((ceil)((float)height1/(float)threads_y));

	num_of_blocks = (int)((ceil)((float)graph_size1/(float)num_of_threads_per_block));

	h_mem_init(); 
	d_mem_init(); 
	cueValues = 0 ;

	return deviceCheck ; 

}


int checkDevice()
{
	int deviceCount ;

	cudaGetDeviceCount( &deviceCount ) ;

	if( deviceCount == 0 )
	{
		return -1;
	}


	return deviceCount ;
}


void h_mem_init()
{
	h_reset_mem  =  ( int* )malloc(sizeof(int) * graph_size1);
	h_graph_height  =  (int* )malloc(size_int);
	pixelLabel  =  (int*)malloc( size_int);
	h_pixel_mask = (bool*)malloc( sizeof(bool) * graph_size1  ) ;
	

	for( int i = 0 ; i < graph_size1 ; i++ )
	{
		pixelLabel[i] = 0 ;
		h_graph_height[i] = 0 ;
	}

	for( int i = 0 ; i < graph_size1 ; i++ )
	{
		h_reset_mem[i] = 0 ;
	}
}


void d_mem_init()
{
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_left_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_right_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_down_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_up_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_push_reser, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_sink_weight, sizeof(int) * graph_size1 ) );

	CUDA_SAFE_CALL( cudaMalloc((void**)&s_left_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&s_right_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&s_down_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&s_up_weight, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&s_push_reser, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&s_sink_weight, sizeof(int) * graph_size1 ) );


	CUDA_SAFE_CALL( cudaMalloc((void**)&d_stochastic, sizeof(int) * num_of_blocks ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_stochastic_pixel, sizeof(int) * graph_size1 ) );


	//CUDA_SAFE_CALL( cudaMalloc((void**)&d_sink_weight, sizeof(int) * graph_size1 ) );
	//CUDA_SAFE_CALL( cudaMalloc((void**)&d_sink_weight, sizeof(int) * graph_size1 ) );
	//CUDA_SAFE_CALL( cudaMalloc((void**)&d_sink_weight, sizeof(int) * graph_size1 ) );
	//CUDA_SAFE_CALL( cudaMalloc((void**)&d_sink_weight, sizeof(int) * graph_size1 ) );
	

	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_left, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_right, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_down, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_up, sizeof(int) * graph_size1 ) );

	CUDA_SAFE_CALL( cudaMalloc((void**)&d_graph_heightr, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_graph_heightw, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_relabel_mask, sizeof(int) * graph_size1 ) );

	CUDA_SAFE_CALL( cudaMalloc( ( void**)&d_pixel_mask, sizeof(bool)*graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc( ( void**)&d_over, sizeof(bool)*1 ) );
	CUDA_SAFE_CALL(cudaMalloc((void**)&d_counter,sizeof(int)));

	CUDA_SAFE_CALL( cudaMalloc( ( void **)&dPixelLabel, sizeof(int) * width1 * height1 ));
	CUDA_SAFE_CALL( cudaMemcpy( d_left_weight, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	CUDA_SAFE_CALL( cudaMemcpy( d_right_weight, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	CUDA_SAFE_CALL( cudaMemcpy( d_down_weight, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	CUDA_SAFE_CALL( cudaMemcpy( d_up_weight, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	CUDA_SAFE_CALL( cudaMemcpy( d_push_reser, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	CUDA_SAFE_CALL( cudaMemcpy( d_sink_weight, h_reset_mem, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;
	
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_left, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_right, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_down, sizeof(int) * graph_size1 ) );
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_pull_up, sizeof(int) * graph_size1 ) );

	h_relabel_mask = (int*)malloc(sizeof(int)*width1*height1);
	
	     h_stochastic = (int *)malloc(sizeof(int) * num_of_blocks);
	     h_stochastic_pixel = (int *)malloc(sizeof(int) * graph_size1);



	for(int i = 0 ; i < graph_size1 ; i++ )
		h_relabel_mask[i] = 1 ; 

	
	CUDA_SAFE_CALL( cudaMemcpy( d_relabel_mask, h_relabel_mask, sizeof( int ) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;

	int *dpixlab = (int*)malloc(sizeof(int)*width1*height1);

	for( int i = 0 ; i < width1 * height1 ; i++ )
	{
		dpixlab[i] = 0 ;
		h_stochastic_pixel[i] = 1 ; 
	}

	for(int i = 0 ; i < num_of_blocks ; i++ )
	{
		h_stochastic[i] = 1 ; 
	}

	        CUDA_SAFE_CALL( cudaMemcpy( d_stochastic, h_stochastic, sizeof(int) * num_of_blocks , cudaMemcpyHostToDevice   ) ) ;
	        CUDA_SAFE_CALL( cudaMemcpy( d_stochastic_pixel, h_stochastic_pixel, sizeof(int) * graph_size1 , cudaMemcpyHostToDevice   ) ) ;


	CUDA_SAFE_CALL( cudaMemcpy( dPixelLabel, dpixlab, sizeof(int) * width1 * height1 , cudaMemcpyHostToDevice   ) ) ;
}


int cudaCutsSetupDataTerm( int *dataTerm )
{
	if( deviceCheck < 1 )
		return -1 ; 

	datacost  =  (int*)malloc(sizeof(int) * width *height * num_Labels );
	
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&dDataTerm, sizeof(int) * width * height * num_Labels ));
	
	CUDA_SAFE_CALL( cudaMemcpy( dDataTerm, dataTerm, sizeof(int) * width * height * num_Labels , cudaMemcpyHostToDevice   ) ) ;
	
	for( int i = 0 ; i < width * height * num_Labels ; i++)
	{
		datacost[i] = dataTerm[i] ; 
	}

	return 0 ; 
}


int cudaCutsSetupSmoothTerm( int *smoothTerm )
{
	if( deviceCheck < 1 )
		return -1 ; 


	smoothnesscost  =  (int*)malloc(sizeof(int) * num_Labels * num_Labels );

	CUDA_SAFE_CALL( cudaMalloc( ( void **)&dSmoothTerm, sizeof(int) * num_Labels * num_Labels ));

	CUDA_SAFE_CALL( cudaMemcpy( dSmoothTerm, smoothTerm, sizeof(int) * num_Labels * num_Labels , cudaMemcpyHostToDevice   ) ) ;
	
	for( int i = 0 ; i < num_Labels * num_Labels ; i++)
	{
		smoothnesscost[i] = smoothTerm[i] ; 
	}

	return 0 ; 
}

int cudaCutsSetupHCue( int *hCue )
{

	if( deviceCheck < 1 )
		return -1 ; 

	hcue  =  (int*)malloc(sizeof(int) * width * height );

	CUDA_SAFE_CALL( cudaMalloc( ( void **)&dHcue, sizeof(int) * width * height ));

	CUDA_SAFE_CALL( cudaMemcpy( dHcue, hCue, sizeof(int) * width * height , cudaMemcpyHostToDevice   ) ) ;

	for( int i = 0 ; i < width * height ; i++)
	{
		hcue[i] = hCue[i] ; 
	}

	cueValues = 1 ; 

	return 0 ; 
}

int cudaCutsSetupVCue( int *vCue )
{
	if( deviceCheck < 1 )
		return -1 ; 

	vcue  =  (int*)malloc(sizeof(int) * width * height );

	CUDA_SAFE_CALL( cudaMalloc( ( void **)&dVcue, sizeof(int) * width * height ));

	CUDA_SAFE_CALL( cudaMemcpy( dVcue, vCue, sizeof(int) * width * height , cudaMemcpyHostToDevice   ) ) ;

	for( int i = 0 ; i < width * height ; i++)
	{
		vcue[i] = vCue[i] ; 
	}

	return 0 ; 
}


int cudaCutsSetupGraph( )
{
	
	if( deviceCheck < 1 )
		return -1 ; 

	int alpha_label = 1 ;

	for( int i = 0 ; i < graph_size1 ; i++ )
	{
		h_reset_mem[i] = 0 ;
		h_graph_height[i] = 0 ;
	}

	int blockEdge = (int)((ceil)((float)( width * height )/ ( float ) 256 ));
	dim3 block_weight(256, 1, 1);
	dim3 grid_weight(blockEdge,1,1);
	
	if( cueValues == 1 )
	{
		CudaWeightCue<<< grid_weight , block_weight >>>( alpha_label, d_left_weight, d_right_weight, d_down_weight, 
								d_up_weight, d_push_reser, d_sink_weight, dPixelLabel, dDataTerm, 
								dSmoothTerm, dHcue, dVcue, width, height, 2) ;
	} else 
	{
		CudaWeight<<< grid_weight , block_weight >>>( alpha_label, d_left_weight, d_right_weight, d_down_weight, 
								d_up_weight, d_push_reser, d_sink_weight, dPixelLabel, dDataTerm, 
								dSmoothTerm, width, height, 2) ;
		
	}

	int *temp_left_weight, *temp_right_weight, *temp_down_weight, *temp_up_weight, *temp_source_weight, *temp_terminal_weight ;

	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_left_weight, sizeof( int ) * graph_size1 ) ) ;
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_right_weight, sizeof( int ) * graph_size1 ) ) ;
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_down_weight, sizeof( int ) * graph_size1 ) ) ;
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_up_weight, sizeof( int ) * graph_size1 ) ) ;
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_source_weight, sizeof( int ) * graph_size1 ) ) ;
	CUDA_SAFE_CALL( cudaMalloc( ( void **)&temp_terminal_weight, sizeof( int ) * graph_size1 ) ) ;

	int blockEdge1 = (int)((ceil)((float)( width1 * height1 )/ ( float ) 256 ));
	dim3 block_weight1(256, 1, 1);
	dim3 grid_weight1(blockEdge1,1,1);

	adjustedgeweight<<<grid_weight1, block_weight1>>>(d_left_weight,d_right_weight,d_down_weight,d_up_weight,d_push_reser,
							d_sink_weight,temp_left_weight,temp_right_weight,temp_down_weight,temp_up_weight,
							temp_source_weight, temp_terminal_weight, width,  height,  graph_size,  width1, 
							height1, graph_size1) ;
	
	copyedgeweight<<<grid_weight1, block_weight1>>>(d_left_weight,d_right_weight,d_down_weight,d_up_weight,d_push_reser,d_sink_weight,
							temp_left_weight,temp_right_weight,temp_down_weight,temp_up_weight,temp_source_weight,
							temp_terminal_weight, d_pull_left, d_pull_right, d_pull_down, d_pull_up, d_relabel_mask, 
							d_graph_heightr, d_graph_heightw, width,  height,  graph_size,  width1, height1, graph_size1) ;
	

	return 0 ; 
}


int cudaCutsNonAtomicOptimize( )
{
	if( deviceCheck < 1 )
	{
		return -1 ; 
	}

	cudaCutsNonAtomic();

	bfsLabeling( );

	return 0 ; 

}


void cudaCutsNonAtomic()
{
	dim3 block_push(threads_x, threads_y, 1);
	dim3 grid_push(blocks_x, blocks_y, 1);

	dim3 d_block(num_of_threads_per_block,1,1);
	dim3 d_grid(num_of_blocks,1,1);

	bool finish = true ;
	counter = num_of_blocks ;

	int numThreadsEnd = 256, numBlocksEnd = 1 ;
	if( numThreadsEnd > counter)
	{
		numBlocksEnd = 1 ;
		numThreadsEnd = counter ;
	}
	else
	{
		numBlocksEnd = (int)ceil(counter/(double)numThreadsEnd);
	}

	dim3 End_block(numThreadsEnd,1,1);
	dim3 End_grid(numBlocksEnd,1,1);

	int *d_counter ; 
	
	bool *d_finish ; 
	for(int i = 0 ; i < num_of_blocks ; i++ )
	{
		h_stochastic[i] = 0 ; 
	}

	CUDA_SAFE_CALL( cudaMalloc((void**)&d_counter, sizeof(int)));
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_finish, sizeof(bool)));

	CUDA_SAFE_CALL( cudaMemcpy( d_counter, &counter, sizeof(int), cudaMemcpyHostToDevice));
	
	counter = 0 ;
	
	int block_num = width1 / 32 ; 

	int *d_block_num; 
	
	CUDA_SAFE_CALL( cudaMalloc((void**)&d_block_num, sizeof(int)));
	CUDA_SAFE_CALL( cudaMemcpy( d_block_num, &block_num, sizeof(int), cudaMemcpyHostToDevice));

	int h_count_blocks = num_of_blocks ;
	int *d_count_blocks;

	CUDA_SAFE_CALL( cudaMalloc((void**)&d_count_blocks, sizeof(int)));
	CUDA_SAFE_CALL( cudaMemcpy( d_count_blocks, &h_count_blocks, sizeof(int), cudaMemcpyHostToDevice));

	h_count_blocks = 0 ;


	counter = 1 ; 
	//kernel_push1_start_nonatomic<<<grid_push,block_push>>>(d_left_weight,d_right_weight, d_down_weight, d_up_weight, 
	//						d_sink_weight, d_push_reser, d_relabel_mask,d_graph_heightr,d_graph_heightw, 
	//						graph_size,width,height, graph_size1, width1 , height1, d_stochastic );

	CUDA_SAFE_CALL(cudaThreadSynchronize());
	unsigned int timer = 0;
	CUT_SAFE_CALL(cutCreateTimer(&timer));
	CUT_SAFE_CALL(cutStartTimer(timer));
	

	
	do
	{

		if(counter % 10 == 0  )
		{
			finish = true ;
			CUDA_SAFE_CALL( cudaMemcpy( d_finish, &finish, sizeof(bool), cudaMemcpyHostToDevice));
			kernel_push_stochastic1<<<grid_push,block_push>>>(d_push_reser, s_push_reser,  d_count_blocks, d_finish, d_block_num, width1);
			CUDA_SAFE_CALL( cudaMemcpy( &finish, d_finish, sizeof(bool), cudaMemcpyDeviceToHost));
		}
		if(counter% 11 == 0 )
		{
			CUDA_SAFE_CALL(cudaMemset(d_stochastic, 0, sizeof(int)*num_of_blocks));
			h_count_blocks = 0 ;
			CUDA_SAFE_CALL( cudaMemcpy( d_count_blocks, &h_count_blocks, sizeof(int), cudaMemcpyHostToDevice));
			kernel_push_stochastic2<<<grid_push,block_push>>>(d_push_reser, s_push_reser, d_stochastic, d_block_num, width1);
			

			CUDA_SAFE_CALL( cudaMemcpy( h_stochastic, d_stochastic, sizeof(int)*num_of_blocks, cudaMemcpyDeviceToHost));
			int blocks_num = 0 ; 
			for ( int i = 0 ; i < num_of_blocks; i++ )
				{
					if( h_stochastic[i] == 1 ) blocks_num++ ;
				}
			CUDA_SAFE_CALL(cudaMemcpy(d_count_blocks, &blocks_num, sizeof(int), cudaMemcpyHostToDevice));
			
		}

		if( counter % 2 == 0 )
		{
		
				kernel_push_nonatomic<<<grid_push,block_push>>>(d_left_weight,d_right_weight, d_down_weight, d_up_weight, 
						d_sink_weight, d_push_reser,d_pull_left, d_pull_right, d_pull_down, d_pull_up,
						d_relabel_mask,d_graph_heightr, graph_size,width,height, graph_size1, width1 , height1 );
				

				krelabel_nonatomic<<<grid_push,block_push>>>(d_left_weight,d_right_weight, d_down_weight, d_up_weight, 
						d_push_reser,d_sink_weight, d_pull_left, d_pull_right, d_pull_down, d_pull_up,
						d_relabel_mask, d_graph_heightr,d_graph_heightw, graph_size,width,height, 
						graph_size1, width1 , height1 );
				
		}
		else
		{
				kernel_push_nonatomic<<<grid_push,block_push>>>(d_left_weight,d_right_weight, d_down_weight, d_up_weight, 
						d_sink_weight, d_push_reser,d_pull_left, d_pull_right, d_pull_down, d_pull_up,
						d_relabel_mask,d_graph_heightw, graph_size,width,height, graph_size1, width1 , height1 );


				krelabel_nonatomic<<<grid_push,block_push>>>(d_left_weight,d_right_weight, d_down_weight, d_up_weight, 
						d_push_reser,d_sink_weight, d_pull_left, d_pull_right, d_pull_down, d_pull_up,
						d_relabel_mask, d_graph_heightw,d_graph_heightr, graph_size,width,height, 
						graph_size1, width1 , height1 );

		}
		counter++ ;
	}
	while(finish);
	
	CUDA_SAFE_CALL(cudaThreadSynchronize());
	CUT_SAFE_CALL(cutStopTimer(timer));
	printf("TT Cuts :: %f\n",cutGetTimerValue(timer));
	CUT_SAFE_CALL(cutDeleteTimer(timer));

}


void bfsLabeling()
{
	dim3 block_push(threads_x, threads_y, 1);
	dim3 grid_push(blocks_x, blocks_y, 1);

	dim3 d_block(num_of_threads_per_block,1,1);
	dim3 d_grid(num_of_blocks,1,1);

	CUDA_SAFE_CALL( cudaMemcpy( d_graph_heightr, h_graph_height, size_int, cudaMemcpyHostToDevice));

	for(int i = 0 ; i < graph_size ; i++ )
		h_pixel_mask[i]=true;

	CUDA_SAFE_CALL( cudaMemcpy( d_pixel_mask, h_pixel_mask, sizeof(bool) * graph_size1, cudaMemcpyHostToDevice));

	kernel_bfs_t<<<d_grid,d_block,0>>>(d_push_reser,d_sink_weight,d_graph_heightr,d_pixel_mask,graph_size,width,height, graph_size1, width1, height1);
	counter=1;

	CUDA_SAFE_CALL( cudaMemcpy( d_counter, &counter, sizeof(int), cudaMemcpyHostToDevice));

	do
	{
		h_over=false;

		CUDA_SAFE_CALL( cudaMemcpy( d_over, &h_over, sizeof(bool), cudaMemcpyHostToDevice) );

		kernel_bfs<<< d_grid,d_block, 0 >>>(d_left_weight,d_right_weight,d_down_weight,d_up_weight,d_graph_heightr,d_pixel_mask,
							graph_size,width,height,graph_size1, width1, height1, d_over,d_counter);

		CUT_CHECK_ERROR("Kernel execution failed");

		CUDA_SAFE_CALL( cudaMemcpy( &h_over, d_over, sizeof(bool), cudaMemcpyDeviceToHost) );

		counter++;

		CUDA_SAFE_CALL(cudaMemcpy(d_counter,&counter,sizeof(int),cudaMemcpyHostToDevice));
	}
	while(h_over);
	
	CUDA_SAFE_CALL(cudaMemcpy(h_graph_height,d_graph_heightr,size_int,cudaMemcpyDeviceToHost));
}


int cudaCutsGetResult( )
{
	if( deviceCheck < 1 )
		return -1 ; 

	int alpha = 1 ;

	for(int i = 0 ; i < graph_size1 ; i++ ) 
	{
		int row_here = i / width1, col_here = i % width1 ;
		if(h_graph_height[i]>0 && row_here < height && row_here > 0 && col_here < width && col_here > 0 ) {
			pixelLabel[i]=alpha;
		}
	}
	
	return 0 ; 

}

int cudaCutsGetEnergy()
{
	return data_energy() + smooth_energy() ;
}

int data_energy()
{
	int eng=0;

	for(int i = 0 ; i < height ; i ++)
	{
		for(int j = 0 ; j < width ; j++)
		{

			eng += datacost(i*width+j, pixelLabel[i*width1+j]);
		}
	}

	printf("DATA ENERGY: %d\n",eng);
	return(eng);
}

int smooth_energy()
{
	int eng = 0;

	int x,y;
	for ( y = 0; y < height; y++ )
		for ( x = 1; x < width; x++ )
		{
			if( cueValues == 1 )
				eng = eng + smoothnesscost(pixelLabel[y*width1+x],pixelLabel[y*width1+x-1])*hcue[y*width+x-1];
			else
				eng = eng + smoothnesscost(pixelLabel[y*width1+x],pixelLabel[y*width1+x-1]);

		}

	for ( y = 1; y < height; y++ )
		for ( x = 0; x < width; x++ )
		{
			if(cueValues == 1)
				eng = eng + smoothnesscost(pixelLabel[y*width1+x],pixelLabel[y*width1+x-width1])*vcue[y*width+x-width];
			else
				eng = eng + smoothnesscost(pixelLabel[y*width1+x],pixelLabel[y*width1+x-width1]);

		}
	printf("SMOOTHNESS ENERGY: %d\n",eng);
	return(eng);
}


void cudaCutsFreeMem()
{
	free(h_reset_mem);
	free(h_graph_height);

	CUDA_SAFE_CALL(cudaFree(d_left_weight));
	CUDA_SAFE_CALL(cudaFree(d_right_weight));
	CUDA_SAFE_CALL(cudaFree(d_down_weight));
	CUDA_SAFE_CALL(cudaFree(d_up_weight));
	CUDA_SAFE_CALL(cudaFree(d_sink_weight));
	CUDA_SAFE_CALL(cudaFree(d_push_reser));

	CUDA_SAFE_CALL(cudaFree(d_pull_left));
	CUDA_SAFE_CALL(cudaFree(d_pull_right));
	CUDA_SAFE_CALL(cudaFree(d_pull_down));
	CUDA_SAFE_CALL(cudaFree(d_pull_up));

	CUDA_SAFE_CALL(cudaFree(d_graph_heightr));
	CUDA_SAFE_CALL(cudaFree(d_graph_heightw));
}

