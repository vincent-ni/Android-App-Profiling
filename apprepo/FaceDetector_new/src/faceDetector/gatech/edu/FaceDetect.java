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
	String path;
//	Bitmap myBitmap2;
	
	transient ExecutionController exectrl;
	
	public FaceDetect(ExecutionController ec, String path){
//		BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
//		BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565; 
//		myBitmap2 = BitmapFactory.decodeFile(path, BitmapFactoryOptionsbfo);
		
		this.path = path;
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
			System.out.println("results: " + results.length);
		}
		catch(Exception e){}
		return results;
	}
	
	//@Real function
	public FaceInfo[] localDetectFaces(int num){
		Bitmap myBitmap2;
		BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
		BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565; 
		myBitmap2 = BitmapFactory.decodeFile(path, BitmapFactoryOptionsbfo);
		
		System.out.println("detecting:" + mfigure.length);
//		BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
//		BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565;
//		ByteArrayInputStream in = new ByteArrayInputStream(mfigure);
//        Bitmap myBitmap = BitmapFactory.decodeStream(in);
        int imageWidth = myBitmap2.getWidth();
		int imageHeight = myBitmap2.getHeight();
		System.out.println("imageWidth: " + imageWidth);
		System.out.println("imageHeight: " + imageHeight);
		int numberOfFace = 100;
		Face[] myFace = new FaceDetector.Face[numberOfFace];
		FaceDetector myFaceDetect = new FaceDetector(imageWidth, imageHeight, numberOfFace);
		int numberOfFaceDetected = myFaceDetect.findFaces(myBitmap2, myFace); 
		System.out.println("numberOfFaceDetected: " + numberOfFaceDetected);
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
