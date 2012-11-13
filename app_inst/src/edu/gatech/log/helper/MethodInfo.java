package edu.gatech.log.helper;

import java.util.List;

public class MethodInfo {
	public int seq;
	public int runTime;
	public String fileName;
	public int lineNum;
	public String methodSig;
	public String logPath;
	public List<FeatInfo> feats;
	
	/* one example:
	 * Time:  17 : faceDetector.gatech.edu.FaceDetectorExample : line73 : <android.app.Activity: void onStop()> : 66
	 */
	public MethodInfo(String[] strs){
		runTime = Integer.parseInt(strs[1].trim());
		fileName = strs[2].trim();
		lineNum = Integer.parseInt(strs[3].trim().substring(4));
		methodSig = (strs[4] + strs[5]).trim();
		seq = Integer.parseInt(strs[6].trim());
	}
	
	public void setLogPath(String logPath){
		this.logPath = logPath;
	}
	
	public void setFeatList(List<FeatInfo> feats){
		this.feats = feats;
	}	
	
	public String generateKey(){
		return fileName + " : line " + lineNum + " : " + methodSig;
	}
	
	public String generateFullInfo(){
		return fileName + " : line " + lineNum + " : " + methodSig + " : runtime "
				+ runTime + " : running seq " + seq;
	}
}
