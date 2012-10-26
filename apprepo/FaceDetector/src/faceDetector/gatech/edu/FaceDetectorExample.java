package faceDetector.gatech.edu;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;


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
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.Display;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

public class FaceDetectorExample extends Activity implements OnClickListener{
	public boolean stop = true;
	public boolean listen = false;
	public boolean send = false;
	
	EditText mCPUNum;
	Button mStartButton;
	Button mListenButton;
	Button mSendButton;
	TextView mInfo;
	PowerManager pm;
	PowerManager.WakeLock wl;
	WifiManager wm;
	WifiManager.WifiLock wif;
	long total = 0;
	long count = 0;

	
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //setContentView(R.layout.main);
        setContentView(new myView(this));
        
        /*pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Run in background");
        wm = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        wif = wm.createWifiLock(3,"comm");
        
        mCPUNum = (EditText)findViewById(R.id.editCPUNum);
        mStartButton = (Button)findViewById(R.id.buttonStart);
        mListenButton = (Button)findViewById(R.id.buttonListen);
        mSendButton = (Button)findViewById(R.id.buttonSend);
        mInfo = (TextView)findViewById(R.id.textView1);
        mStartButton.setOnClickListener(this);
        mListenButton.setOnClickListener(this);
        mSendButton.setOnClickListener(this);*/
        
        
    }
    @Override
    public void onStop(){
    	super.onStop();
    	/*stop = true;
    	if(wl.isHeld())
    		wl.release();
    	if(wif.isHeld())
			wif.release();*/
    }
    
    @Override
    public void onClick(View view) {
    	if(view.equals(mStartButton)){
	    	if(mStartButton.getText().equals("Start")){
	    		wl.acquire();
	    		mStartButton.setText("Stop");
	    		stop = false;
	    		
	    		int num = Integer.parseInt(mCPUNum.getText().toString());
	    		FaceDetection.startTime = System.currentTimeMillis();
	    		FaceDetection.count = 0;
	    		for(int i = 0; i < num; i++){
	    			FaceDetection f = new FaceDetection(this);
	    			f.start();
	    		}
	    		 
	    	}
	    	else{
	    		if(wl.isHeld())
	    			wl.release();
	    		mStartButton.setText("Start");
	    		stop = true;
	    	}
    	}
    	else if(view.equals(mListenButton)){
    		if(mListenButton.getText().equals("Listen")){
    			//wl.acquire();
    			wif.acquire();
    			mListenButton.setText("Listening");
    			
    			listen = true;
    			ProbListener listener = new ProbListener(this);
    			listener.start();
    		}
    		else{
    			//if(wl.isHeld())
	    		//	wl.release();
    			if(wif.isHeld())
    				wif.release();
    			mListenButton.setText("Listen");
    			
    			listen = false;
    		}
    	}
    	else if(view.equals(mSendButton)){
    		if(mSendButton.getText().equals("Send")){
    			//wl.acquire();
    			wif.acquire();
    			mSendButton.setText("Sending");
    			
    			send = true;
    			TCPSender sender = new TCPSender(this);
    			sender.start();
    		}
    		else{
    			//if(wl.isHeld())
	    			//wl.release();
    			if(wif.isHeld())
    				wif.release();
    			mSendButton.setText("Send");
    			
    			send = false;
    			
    		}
    	}
    	
    }
    
    private class myView extends View{
    	
    	private int imageWidth, imageHeight;
    	private int numberOfFace = 100;
    	private FaceDetector myFaceDetect; 
    	private FaceDetector.Face[] myFace;
    	private FaceDetector.Face[] myFace2;
    	float myEyesDistance;
    	int numberOfFaceDetected;
    	int numberOfFaceDetected2;
    	
    	Bitmap myBitmap;
    	Bitmap myBitmap2;
    	long duration;
    	long comp;
    	long du2 = 0;

		public myView(Context context) {
			super(context);
			// TODO Auto-generated constructor stub
			
			BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
			BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565; 
			//myBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.face11, BitmapFactoryOptionsbfo);
			myBitmap = BitmapFactory.decodeFile("/sdcard/DCIM/face1.jpg",BitmapFactoryOptionsbfo);
			imageWidth = myBitmap.getWidth();
			imageHeight = myBitmap.getHeight();
			
			new Thread(){
				public void run(){
					BitmapFactory.Options BitmapFactoryOptionsbfo = new BitmapFactory.Options();
					BitmapFactoryOptionsbfo.inPreferredConfig = Bitmap.Config.RGB_565; 
					//myBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.face11, BitmapFactoryOptionsbfo);
					myBitmap2 = BitmapFactory.decodeFile("/sdcard/DCIM/face2.jpg",BitmapFactoryOptionsbfo);
					int imageWidth = myBitmap2.getWidth();
					int imageHeight = myBitmap2.getHeight();
					long s = System.currentTimeMillis();
					myFace2 = new FaceDetector.Face[numberOfFace];
					FaceDetector myFaceDetect = new FaceDetector(imageWidth, imageHeight, numberOfFace);
					numberOfFaceDetected2 = myFaceDetect.findFaces(myBitmap2, myFace2); 
					du2 = System.currentTimeMillis()-s;
				}
			}.start();
			
			 Matrix matrix = new Matrix();
		        // resize the bit map
		     matrix.postScale(1, 1);
			
			Bitmap resizedBitmap = Bitmap.createBitmap(myBitmap, imageWidth/2, 0, 
	        		imageWidth/2, imageHeight, matrix, true); 
			long start = System.currentTimeMillis();
			/*File f = new File("/sdcard/DCIM/face2.jpg");
			try{
				if(f.exists())
					f.delete();
			f.createNewFile();
			OutputStream out = new FileOutputStream(f );
			resizedBitmap.compress(Bitmap.CompressFormat.JPEG, 10, out);
			}
			catch(Exception e){}
			comp = System.currentTimeMillis()-start;*/
			
			myFace = new FaceDetector.Face[numberOfFace];
			myFaceDetect = new FaceDetector(imageWidth, imageHeight, numberOfFace);
			numberOfFaceDetected = myFaceDetect.findFaces(myBitmap, myFace); 
			duration = System.currentTimeMillis()- start;
			/*while(du2 == 0){
				try{
					Thread.sleep(1000);
				}catch(Exception e){}
			}*/
			
		}

		@Override
		protected void onDraw(Canvas canvas) {
			// TODO Auto-generated method stub
			Display display = getWindowManager().getDefaultDisplay(); 
			int width = display.getWidth();
			int height = display.getHeight();

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
            
            for(int i=0; i < numberOfFaceDetected; i++)
            {
            	Face face = myFace[i];
            	PointF myMidPoint = new PointF();
            	face.getMidPoint(myMidPoint);
				myEyesDistance = face.eyesDistance();
            	canvas.drawRect(
            			(int)((myMidPoint.x - myEyesDistance)*scaleWidth),
            			(int)((myMidPoint.y - myEyesDistance)*scaleHeight),
            			(int)((myMidPoint.x + myEyesDistance)*scaleWidth),
            			(int)((myMidPoint.y + myEyesDistance)*scaleHeight),
            			myPaint);
            }
            myPaint.setStrokeWidth(1);
            myPaint.setTextSize(50);
            myPaint.setStyle(Paint.Style.FILL); 
            myPaint.setColor(Color.RED);
            //canvas.drawText(du2+"  "+numberOfFaceDetected+"  "+duration, 10, 100, myPaint);
            canvas.drawText("Task 1", 100, 100, myPaint);
            myPaint.setStrokeWidth(5);
            canvas.drawLine(newWidth, 0, newWidth, newHeight, myPaint);
            
            //segment 2
            resizedBitmap = Bitmap.createBitmap(myBitmap2, 0, 0, 
	        		imageWidth, imageHeight, matrix, true); 
	    
	     
            canvas.drawBitmap(resizedBitmap, newWidth, 0, null);
            
            myPaint.setColor(Color.RED);
            myPaint.setStyle(Paint.Style.STROKE); 
            myPaint.setStrokeWidth(3);
            
            for(int i=0; i < numberOfFaceDetected2; i++)
            {
            	Face face = myFace2[i];
            	PointF myMidPoint = new PointF();
            	face.getMidPoint(myMidPoint);
				myEyesDistance = face.eyesDistance();
            	canvas.drawRect(
            			newWidth+(int)((myMidPoint.x - myEyesDistance)*scaleWidth),
            			(int)((myMidPoint.y - myEyesDistance)*scaleHeight),
            			newWidth+(int)((myMidPoint.x + myEyesDistance)*scaleWidth),
            			(int)((myMidPoint.y + myEyesDistance)*scaleHeight),
            			myPaint);
            }
            myPaint.setStrokeWidth(1);
            myPaint.setTextSize(50);
            myPaint.setStyle(Paint.Style.FILL); 
            myPaint.setColor(Color.RED);
            //canvas.drawText(du2+"  "+numberOfFaceDetected+"  "+duration, 10, 100, myPaint);
            canvas.drawText("Task 2", newWidth + 100, 100, myPaint);
		}
    }
}