package edu.gatech.log.helper;

public class FeatInfo {
	public String type; // "ret" or "loop"
	public String methodSig;
	public int lineNum;
	public String featName;
	public int value;
	
	/*
	 * Two examples:
	 * ret:  <android.view.Display: int getWidth()> : line227 : c16 : 480
	 * loop:  <faceDetector.gatech.edu.FaceDetectorExample$myView: void onDraw(android.graphics.Canvas)> : line266 : c14 : 4
	 */
	public FeatInfo(String str){
		String[] strs = str.split(":");
		type = strs[0].trim();
		methodSig = (strs[1] + strs[2]).trim();
		lineNum = Integer.parseInt(strs[3].trim().substring(4));
		featName = strs[4].trim();
		value = Integer.parseInt(strs[5].trim());
	}
	
	public String generateFullInfo(){
		return type + " : " + methodSig + " : " + " line " + lineNum + " : "
				+ featName + " : " + value;
	}
}
