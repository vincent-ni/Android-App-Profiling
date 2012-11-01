package faceDetector.gatech.edu;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.Serializable;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PointF;
import android.media.FaceDetector;
import android.media.FaceDetector.Face;

public class FaceDetect implements Serializable{
	
	private static final long serialVersionUID = 1L;
	byte[] mfigure;
	
	transient ExecutionController exectrl;
	
	public FaceDetect(ExecutionController ec, String path){
		exectrl = ec;
		
		File f = new File(path);
        int size = (int) f.length();
        mfigure = new byte[size];
        InputStream ins = null;
        try {
        	ins = new FileInputStream(f);
        	ins.read(mfigure);
        	ins.close();
            }
        catch (Exception e) {
            e.printStackTrace();
        }
	}
	
	//@Offload point
	public FaceInfo[] detectFaces(){
		Class<?>[] paramTypes = {int.class};
		Object[] paramValues = {10};
		FaceInfo[] results = null;
		try{
			results = (FaceInfo[])exectrl.execute("localDetectFaces", paramTypes, paramValues, this);
		}
		catch(Exception e){}
		return results;
	}
	
	//@Real function
	public FaceInfo[] localDetectFaces(int num){
		//decode file
		ByteArrayInputStream in = new ByteArrayInputStream(mfigure);
        Bitmap myBitmap = BitmapFactory.decodeStream(in);
        //detect faces
        int imageWidth = myBitmap.getWidth();
		int imageHeight = myBitmap.getHeight();
		int numberOfFace = 100;
		Face[] myFace = new FaceDetector.Face[numberOfFace];
		FaceDetector myFaceDetect = new FaceDetector(imageWidth, imageHeight, numberOfFace);
		int numberOfFaceDetected = myFaceDetect.findFaces(myBitmap, myFace); 
		//construct results
		FaceInfo[] results = new FaceInfo[numberOfFaceDetected];
		PointF myMidPoint = new PointF();
		
		for(int i = 0; i < numberOfFaceDetected; i++){
        	myFace[i].getMidPoint(myMidPoint);
			results[i] = new FaceInfo(myMidPoint.x, myMidPoint.y, myFace[i].eyesDistance());
		}
		
		return results;
	}

}
