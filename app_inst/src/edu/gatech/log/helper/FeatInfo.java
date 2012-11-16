package edu.gatech.log.helper;

public class FeatInfo {
	public String type; // "ret" or "loop" or "para"
	public String methodSig;
	public int lineNum;
	public String featName;
	public int value;
	
	/*
	 * Three examples:
	 * ret:  <faceDetector.gatech.edu.FaceDetect: faceDetector.gatech.edu.FaceInfo[] localDetectFaces(int)> : line72 : <android.graphics.Bitmap: int getHeight()> : c3 : 2048
	 * loop:  <faceDetector.gatech.edu.FaceDetect: faceDetector.gatech.edu.FaceInfo[] localDetectFaces(int)> : line84 : c1 : 1
	 * para:  para:  <faceDetector.gatech.edu.FaceDetectApp: void onCreate(android.os.Bundle)> : line28 : <android.app.Activity: android.view.View findViewById(int)> : IntPara1 : c10 : 2131034114
	 */
	public FeatInfo(String str){
		String[] strs = str.split(":");
		type = strs[0].trim();
		if (type.equals("loop")) {
			methodSig = (strs[1] + strs[2]).trim();
			lineNum = Integer.parseInt(strs[3].trim().substring(4));
			featName = strs[4].trim();
			value = Integer.parseInt(strs[5].trim());
		}
		else if (type.equals("ret")){
			methodSig = (strs[1] + strs[2] + strs[4] + strs[5]).trim();
			lineNum = Integer.parseInt(strs[3].trim().substring(4));
			featName = strs[6].trim();
			value = Integer.parseInt(strs[7].trim());
		}
		else {
			methodSig = (strs[1] + strs[2] + strs[4] + strs[5] + strs[6]).trim();
			lineNum = Integer.parseInt(strs[3].trim().substring(4));
			featName = strs[7].trim();
			value = Integer.parseInt(strs[8].trim());
		}
	}
	
	public String generateFullInfo(){
		return type + " : " + methodSig + " : " + " line " + lineNum + " : "
				+ featName + " : " + value;
	}
}
