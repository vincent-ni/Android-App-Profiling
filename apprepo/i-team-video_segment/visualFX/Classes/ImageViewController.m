//
//  ImageViewController.m
//  visualFX
//
//  Created by Matthias Grundmann on 8/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "ImageViewController.h"
#import "ImageView.h"
#import "visualFXViewController.h"
#import "FXImage.h"
#import "FXImageInfo.h"
#import "FXMatrix.h"
#import "Contour.h"
#import "HarrisCorners.h"
#import "Inpaint.h"

#include <vector>



@implementation ImageViewController


// Override initWithNibName:bundle: to load the view using a nib file then perform additional customization that is not appropriate for viewDidLoad.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]) {
        // Custom initialization
    }
    return self;
}



// Implement loadView to create a view hierarchy programmatically.
- (void)loadView {
  CGRect frame = [[UIScreen mainScreen]  applicationFrame];
  ImageView* iv = [[[ImageView alloc] initWithFrame:frame] autorelease];
  self.view = iv;
  
  CGRect tool_frame = CGRectMake(0,frame.size.height-kToolbarHeight, frame.size.width,
                                 kToolbarHeight);
  UIToolbar* toolbar = [[[UIToolbar alloc] initWithFrame:tool_frame] autorelease];
  toolbar.barStyle = UIBarStyleBlackTranslucent;
  
  UIBarButtonItem* back_button = [[[UIBarButtonItem alloc] 
                                 initWithImage:[UIImage imageNamed:@"backButton.png"]
                                         style:UIBarButtonItemStylePlain 
                                        target:self
                                        action:@selector(backButtonPressed)] autorelease];
  
  UIBarButtonItem* add_button = [[[UIBarButtonItem alloc] 
                                initWithImage:[UIImage imageNamed:@"plusButton.png"]
                                style:UIBarButtonItemStylePlain 
                                target:self
                                action:@selector(addButtonPressed)] autorelease];  
  
  UIBarButtonItem* select_button = [[[UIBarButtonItem alloc] 
                                   initWithImage:[UIImage imageNamed:@"selectButton.png"]
                                   style:UIBarButtonItemStylePlain 
                                   target:self
                                   action:@selector(selectButtonPressed)] autorelease];
  
  UIBarButtonItem* contour_button = [[[UIBarButtonItem alloc] 
                                    initWithImage:[UIImage imageNamed:@"contourButton.png"]
                                    style:UIBarButtonItemStylePlain 
                                    target:self
                                    action:@selector(contourButtonPressed)] autorelease];
  
  UIBarButtonItem* test1_button = [[[UIBarButtonItem alloc] 
                                    initWithImage:[UIImage imageNamed:@"t1.png"]
                                    style:UIBarButtonItemStylePlain 
                                    target:self
                                    action:@selector(testButton1Pressed)] autorelease];
  
  UIBarButtonItem* test2_button = [[[UIBarButtonItem alloc] 
                                   initWithImage:[UIImage imageNamed:@"t2.png"]
                                   style:UIBarButtonItemStylePlain 
                                   target:self
                                   action:@selector(testButton2Pressed)] autorelease];
  
  [toolbar setItems: [NSArray arrayWithObjects:
                              back_button,
                              add_button,
                              select_button,
                              contour_button,
                              test1_button,
                              test2_button,
                              nil]];
  [iv addSubview:toolbar];
}

- (void) displayImageWithInfo:(FXImageInfo*) img_info {
  [(ImageView*)self.view addImageWithInfo:img_info];	
}


/*
// Implement viewDidLoad to do additional setup after loading the view.
- (void)viewDidLoad {
    [super viewDidLoad];
}
*/

-(void)backButtonPressed {
  [self.navigationController popViewControllerAnimated:YES];
}

-(void)addButtonPressed {
  UIActionSheet* question = [[UIActionSheet alloc] initWithTitle:@"Get image from?"
                                                        delegate:self 
                                               cancelButtonTitle:@"Photo Library"
                                          destructiveButtonTitle:@"Camera"
                                               otherButtonTitles:nil];
  [question showInView:self.view];
  [question release];  
}

- (void) actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
  UIImagePickerControllerSourceType img_src_type;
  if (buttonIndex == 0) 
    img_src_type = UIImagePickerControllerSourceTypeCamera;
  else
    img_src_type =  UIImagePickerControllerSourceTypePhotoLibrary;
  
  if ([UIImagePickerController 
       isSourceTypeAvailable:img_src_type]) {
    UIImagePickerController* img_picker = [[UIImagePickerController alloc] init];
    img_picker.sourceType = img_src_type;
    img_picker.delegate = self;
    img_picker.allowsImageEditing = NO;
    
    [self presentModalViewController:img_picker animated:YES];
    [img_picker release];
  } else {
    UIAlertView* message = 
    [[UIAlertView alloc] initWithTitle:@"Error picking image" 
                               message:@"This image source is not supported by your device." 
                              delegate:self 
                     cancelButtonTitle:@"Dismiss" 
                     otherButtonTitles:nil]; 
    [message show];
    [message release];
  }
}

-(void)imagePickerController:(UIImagePickerController*)img_picker
       didFinishPickingImage:(UIImage*)img
                 editingInfo:(NSDictionary*)editingInfo {
  
  FXImage* fx_img = [[FXImage alloc] initWithCGImage:img.CGImage 
                                         borderWidth:0
                                             maxSize:1024
                                         orientation:img.imageOrientation];
  
  FXImageInfo* img_info = [[FXImageInfo alloc] initWithFXImage:fx_img];
  
  [self displayImageWithInfo:img_info];
  
  [[img_picker parentViewController] dismissModalViewControllerAnimated: YES];
}

-(void) selectButtonPressed {
  ImageView* iv = (ImageView*)self.view;
  iv.is_select_mode = !iv.is_select_mode;
  [iv updateView];
}

-(void) contourButtonPressed {
  /*
  // Get selection and close it
  ImageView* view = (ImageView*)self.view;
  FXImage* selection_img = view.selection;
  FXImage* orig_image = view.selected_img_.image;
  FXRect selection_frame = view.selection_frame;
  selection_frame = FXRect(0, 0, 225, 276);
  
  if (selection_frame.width > 0 && selection_frame.height > 0) {
    [selection_img setBorder:2*kLayerClosureRadius toConstant:0 mode:BORDER_HORIZ];
    FXRect old_selection_roi = selection_img.roi;
    FXRect bounding_rect = selection_frame;
    
    // Make additional border for closing the contour.
    bounding_rect.x -= kLayerClosureRadius;
    bounding_rect.y -= kLayerClosureRadius;
    bounding_rect.width += 2*kLayerClosureRadius;
    bounding_rect.height += 2*kLayerClosureRadius;
    
    selection_img.roi = bounding_rect.shiftOffset(old_selection_roi.offset());
    
    FXImage* mask_img = [[FXImage alloc] initWithImgWidth:bounding_rect.width
                                                imgHeight:bounding_rect.height
                                              imgChannels:1
                                              borderWidth:kLayerClosureRadius];
    
  //  [Contour dilateImage:selection_img tmpImage:mask_img radius:kLayerClosureRadius];
   // [Contour erodeImage:selection_img tmpImage:mask_img radius:kLayerClosureRadius];
    
    // Save selection for inpainting.
    selection_img.roi = selection_frame.shiftOffset(old_selection_roi.offset());
    mask_img.roi = FXRect(2*kLayerClosureRadius,
                          2*kLayerClosureRadius,
                          selection_frame.width,
                          selection_frame.height);
    
    [selection_img copyToImage:mask_img];
    selection_img.roi = old_selection_roi;
    
    Inpaint* inpaint = [[Inpaint alloc] initWithPatchRadius:4];
    [inpaint inpaintImage:orig_image maskImage:mask_img selectionFrame:selection_frame];
    [inpaint release];
    [mask_img release];
  
    [orig_image loadTo16bitTexture];
  }
  
  ((ImageView*)self.view).is_select_mode = false;
  
//  [self selectButtonPressed];
  */
}

-(void) testButton1Pressed {
  // Test sobel filter code
  FXImage* color_img = [[[FXImage alloc] initWithUIImage: 
                        [UIImage imageNamed:@"tangent_test.png"] borderWidth:0] autorelease];
  
  uint img_width = color_img.roi.width;
  uint img_height = color_img.roi.height;
  
  FXImage* gray_img = [[[FXImage alloc] initWithImgWidth:img_width
                                              imgHeight:img_height
                                            imgChannels:1
                                            borderWidth:1] autorelease];
//  FXImage* smoothed_img = [[FXImage alloc] initWithImgWidth:img_width
//                                                  imgHeight:img_height
//                                                imgChannels:4
 //                                               borderWidth:20];
//  FXMatrix* tan_x = [[[FXMatrix alloc] initWithMatrixWidth:img_width
//                                            matrixHeight:img_height
//                                               zeroMatrix:NO] autorelease];
//  FXMatrix* tan_y = [[[FXMatrix alloc] initWithMatrixWidth:img_width
 //                                           matrixHeight:img_height
  //                                             zeroMatrix:NO] autorelease];
 
  [color_img rgbaToGrayImage:gray_img];
  std::vector<CGPoint> p(4);

  p[0] = CGPointMake(floor(0.05*img_width+0.5)-1, floor(0.05*img_height+0.5)-1);
  p[1] =  CGPointMake(floor(0.05*img_width+0.5)-1, floor(0.95*img_height+0.5)-1);
  p[2] =  CGPointMake(floor(0.95*img_width+0.5)-1, floor(0.95*img_height+0.5)-1);
  p[3] =  CGPointMake(floor(0.95*img_width+0.5)-1, floor(0.05*img_height+0.5)-1);
  
  [Contour contourFromImage:gray_img initialContour:&p];
  for (std::vector<CGPoint>::iterator i = p.begin(); i != p.end(); ++i) {
    i->y = img_height - i->y;
  }
  
  std::vector<CGPoint>* p_new = new std::vector<CGPoint>(p);
  [(ImageView*)self.view setOutline:p_new];
   
  //NSLog(@"Elems: %d", p.size() );
  //for (int i = 0; i < p.size(); ++i) {
  //  NSLog(@"x: %f y:%f", p[i].x, p[i].y);
  //}
  
  //[con tangentsFromImage:gray_img filterRadius:1 iterations:20 fillAll:false 
   //             tangentX:tan_x tangentY:tan_y];
  
//  [tan_y convertToGrayImage:gray_img];

  
 // [color_img gaussianFilterWithSigma:1.5 radius:2 outputImage:smoothed_img];
 // [smoothed_img copyReplicateBorderPixels:20];
 // smoothed_img.roi = FXRect(0, 0, smoothed_img.width, smoothed_img.height);
//  [smoothed_img grayToRgbaImage:color_img];
  
  //[color_img rgbaToGrayImage:gray_img];
	//[gray_img computeGradientX:graX gradientY:graY];
  //[graX convertToGrayImage:gray_img];
  //[graY convertToGrayImage:gray_img];

  
//  FXMatrix* gray_mat = [[FXMatrix alloc] initWithFXImage:gray_img];
 // FXMatrix* smoothed = [[FXMatrix alloc] initWithMatrixWidth:img_width 
  //                                              matrixHeight:img_height
   //                                                zeroImage:NO];
  
 // [gray_mat gaussianFilterSeparableWithSigma:1.5 radius:2 tmpMatrix:0 outputMatrix:smoothed];
 // [gray_mat gaussianFilterWithSigma:1.5 radius:2 outputMatrix:smoothed];
 // [gray_img gaussianFilterWithSigma:1.5 radius:2 outputMatrix:smoothed];
  //[smoothed convertToGrayImage:gray_img];
  
  // [gray_img computeGradientX:graX gradientY:graY];
  // [graX convertToGrayImage:gray_img];
  // [graY convertToGrayImage:gray_img];
  
  [gray_img grayToRgbaImage:color_img];
  
  // Display image
  FXImageInfo* img_info = [[FXImageInfo alloc] initWithFXImage:gray_img];
  [self displayImageWithInfo:img_info];
  
  [img_info release];
}

- (void) testButton2Pressed {
	// Test the Gaussian kernel generation of a given odd size and 
	// a given sigma.
	// Testing Done:
	// Note: The results are consistent with the matlab results.
	FXImage* color_img = [[FXImage alloc] initWithUIImage: 
                        [UIImage imageNamed:@"eagle.jpg"] borderWidth:0];
	uint img_width = color_img.roi.width;
  uint img_height = color_img.roi.height;
	FXImage* gray_img = [[FXImage alloc] initWithImgWidth:img_width
                                              imgHeight:img_height
                                            imgChannels:1
                                            borderWidth:1];
	FXImage* matrix_img = [[FXImage alloc] initWithImgWidth:img_width
                                              imgHeight:img_height
                                            imgChannels:1
                                            borderWidth:2];
	
	[color_img rgbaToGrayImage:gray_img];
	
	float gaussian_kernel[5*5];
	[color_img computeGaussianKernel:gaussian_kernel
											  kernelSize: 5
											kernelSigma: 1];
	
	//Test if the convolution is correct
	FXMatrix* test_mat = [[FXMatrix alloc] initWithMatrixWidth:img_width
																								matrixHeight:img_height
                                                  zeroMatrix:false];
	
	// Test the elementwise binary operators +, - , /, *
	// All of them have been tested to be correct
	
	float val1[9] = { 100, 1, 2, 4, 5, 6, 7, 10, 2000 };
	//float val2[9];
	//for (int i = 0; i < 25; i++) {
	//	val1[i] = i;
	//	val2[i] = 20;
	//}
	/*
	std::vector<int> rows(9);
	std::vector<int> cols(9);
	
	HarrisCorners* harris = [[[HarrisCorners alloc] init] autorelease];
	FXMatrix* src1 = [[FXMatrix alloc] initWithMatrixWidth:3
																								matrixHeight:3
																							matrixData:val1];
	FXMatrix* src2 = [[FXMatrix alloc] initWithMatrixWidth:3+2
																					matrixHeight:3+2
																							zeroMatrix:false];
	src2.roi = FXRect(1, 1, 3, 3 );
	[src1 copyToMatrix:src2 replicateBorder:1];
	//NSLog(@"x=%d y= %d , width =%d ht = %d\n", src1.roi.x, src1.roi.y ,src1.roi.width ,src1.roi.height);
	[harris nonmaximalSupression:src2
							supressionRadius:1
					 supressionThreshold:10
										rowsVector:&rows
										colsVector:&cols];
	for (int i = 0; i < rows.size(); i++)
		NSLog(@"Non maximal suppresion %d", rows.at(i));
	 */
	/*
	[src1 copyReplicateBorderPixels:2];
	float* src1_data = src1.roi_data;
	uint width = src1.width;
	uint height = src1.height;
	NSLog(@"Height and WIdth %d %d", height, width);
	for ( int i = 0; i < height*width; i++)
		NSLog(@"replicated value %f\n", src1_data[i]);
	
	[harris elementwiseAddition:src1
										secondMat:src2
												width:5
											 height:5
										resultMat:src2];
	[harris elementwiseSubtraction:src1
										secondMat:src2
												width:5
											 height:5
										resultMat:src2];
	[harris elementwiseDivision:src1
										secondMat:src2
												width:5
											 height:5
										resultMat:src2];
	[harris elementwiseMultiplication:src1
										secondMat:src2
												width:5
											 height:5
										resultMat:src2];
	*/

	
	[gray_img computeGaussianSmoothing:test_mat kernelSize:5 kernelSigma:10.0];
	[test_mat convertToGrayImage:matrix_img];
	[matrix_img grayToRgbaImage:gray_img];
	FXImageInfo* img_info = [[FXImageInfo alloc] initWithFXImage:gray_img];
  [self displayImageWithInfo:img_info];

	[img_info release];
	[matrix_img release];
	[test_mat release];
	[gray_img release];
	[color_img release];
	
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning]; // Releases the view if it doesn't have a superview
    // Release anything that's not essential, such as cached data
}


- (void)dealloc {
    [super dealloc];
}


@end
