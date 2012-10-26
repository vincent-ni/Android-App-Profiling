package faceDetector.gatech.edu;

import java.io.*;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.drawable.BitmapDrawable;
import android.media.FaceDetector;
import android.media.FaceDetector.Face;

public class FaceDetection extends Thread {
	FaceDetectorExample myView;
	
	public static int count = 0;
	
	public static long startTime;
	
	public FaceDetection(FaceDetectorExample view){
		myView = view;
		
		BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
		BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565; 
		//myBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.face11, BitmapFactoryOptionsbfo);
		myBitmap = BitmapFactory.decodeFile("/sdcard/dcim/face13.jpg",BitmapFactoryOptionsbfo);
		imageWidth = myBitmap.getWidth();
		imageHeight = myBitmap.getHeight();
		myFaceDetect= new FaceDetector(imageWidth, imageHeight, numberOfFace);
		myFace = new FaceDetector.Face[numberOfFace];
		
	}
	int battery;
	public void run(){
		while(!myView.stop){
			try{
				long time = detectFace();
			
				int c = finishARound();
				
				if(c%10 == 1){
					BufferedReader reader = new BufferedReader( new InputStreamReader( 
							new FileInputStream( "/sys/class/power_supply/battery/capacity" ) ), 1000 );
					String line = reader.readLine();
					battery = Integer.parseInt(line);
					reader.close();
					record(time,c, battery);
					final String info = time+" "+c+" "+battery;
					myView.mInfo.post(new Runnable(){public void run(){myView.mInfo.setText(info);}});
				}
				if(c%2 == 1){
					final String info = time+" "+c+" "+battery;
					myView.mInfo.post(new Runnable(){public void run(){myView.mInfo.setText(info);}});
				}
			}
			catch(Exception e){}
		}
	}
	
	public static synchronized int finishARound(){
		count++;
		return count;
	}
	
	public static synchronized void record(long time, int count, int battery){
		File f = new File("/sdcard/dcim/record_single.txt");
		try{
			
			if(!f.exists())
				f.createNewFile();
			BufferedWriter wr = new BufferedWriter(new FileWriter(f,true));
			wr.append(time+"\t"+count+"\t"+battery+"\n");
			wr.flush();
			wr.close();
		}catch(Exception e){}
	}
	
	
	int imageWidth, imageHeight;
	int numberOfFace = 100;
	FaceDetector myFaceDetect;
	FaceDetector.Face[] myFace;
	float myEyesDistance;
	int numberOfFaceDetected;
	Bitmap myBitmap;
	
	public long detectFace(){
		numberOfFaceDetected = myFaceDetect.findFaces(myBitmap, myFace); 
		return System.currentTimeMillis()-startTime;
	}

}
