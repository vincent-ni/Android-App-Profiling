package faceDetector.gatech.edu;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;

public class ResultView extends Drawable{
	private int imageWidth, imageHeight, width, height;
	private FaceInfo[] myFace;
	Bitmap myBitmap;
	
	public ResultView(FaceInfo[] faces, String filePath, int w, int h){
		super();
		myFace = faces;
		
		myBitmap = BitmapFactory.decodeFile(filePath);
		width = w;
		height = h;
		imageWidth = myBitmap.getWidth();
		imageHeight = myBitmap.getHeight();
	}
	
	@Override
	public int getOpacity(){
		return 0;
		
	}

	@Override
	public void draw(Canvas canvas) {

        int newWidth = 200;
        int newHeight = 200;
        
        if(width*1.0/ imageWidth > height*1.0/imageHeight){
        	newHeight = height;
        	newWidth = (int)(height*1.0/imageHeight*imageWidth);
        }
        else
        {
        	newWidth = width;
        	newHeight = (int)(width*1.0/imageWidth*imageHeight);
        }
        
        // calculate the scale - in this case = 0.4f
        float scaleWidth = ((float) newWidth) / imageWidth;
        float scaleHeight = ((float) newHeight) / imageHeight;
        
        // createa matrix for the manipulation
        Matrix matrix = new Matrix();
        // resize the bit map
        matrix.postScale(scaleWidth, scaleHeight);
        // rotate the Bitmap
        //matrix.postRotate(45);

        // recreate the new Bitmap
        Bitmap resizedBitmap = Bitmap.createBitmap(myBitmap, 0, 0, 
        		imageWidth, imageHeight, matrix, true); 
    
     
        canvas.drawBitmap(resizedBitmap, 0, 0, null);
        
        Paint myPaint = new Paint();
        myPaint.setColor(Color.RED);
        myPaint.setStyle(Paint.Style.STROKE); 
        myPaint.setStrokeWidth(3);
        
        for(int i=0; i < myFace.length; i++)
        {
        	FaceInfo myMidPoint = myFace[i];
        	float myEyesDistance = myMidPoint.eyedistance;
        	canvas.drawRect(
        			(int)((myMidPoint.x - myEyesDistance)*scaleWidth),
        			(int)((myMidPoint.y - myEyesDistance)*scaleHeight),
        			(int)((myMidPoint.x + myEyesDistance)*scaleWidth),
        			(int)((myMidPoint.y + myEyesDistance)*scaleHeight),
        			myPaint);
        }

	}

	@Override
	public void setAlpha(int alpha) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setColorFilter(ColorFilter cf) {
		// TODO Auto-generated method stub
		
	}

}
