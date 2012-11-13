package faceDetector.gatech.edu;

import android.app.Activity;
import android.os.Bundle;
import android.view.Display;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.*;

public class FaceDetectApp extends Activity implements OnClickListener{
	public boolean stop = true;
	public boolean listen = false;
	public boolean send = false;
	
	EditText mFilePath;
	Button mStartButton;
	ImageView mImageResult;

	
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        mFilePath = (EditText)findViewById(R.id.editFilePath);
        mStartButton = (Button)findViewById(R.id.buttonStart);
        mImageResult = (ImageView)findViewById(R.id.imageResult);
        mStartButton.setOnClickListener(this);
    }
    @Override
    public void onStop(){
    	super.onStop();
    }
    
    public void onClick(View view) {
    	if(view.equals(mStartButton)){
    		String filePath = mFilePath.getText().toString();
    		FaceDetect fd = new FaceDetect(new ExecutionController(), filePath);
    		FaceInfo[] faces = fd.detectFaces();
    		Display display = getWindowManager().getDefaultDisplay(); 
			int width = display.getWidth();
			int height = display.getHeight();
			System.out.println(width);
			System.out.println(height);
			//System.out.println(faces.length);
    		ResultView resultImg = new ResultView(faces,filePath,width, height );
    		mImageResult.setImageDrawable(resultImg);
    	}
    	
    }
}