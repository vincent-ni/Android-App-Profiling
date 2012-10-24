package edu.gatech.util;

public class MethodInfo {
	public int runSeq;
	public String methodSig;
	public ObjectInfo base;
	public ObjectInfo returnVal;
	public ObjectInfo[] paramList;
	public long startTime;
	public long runTime;
	
	public MethodInfo(String methodSig, int runSeq, long startTime){
		this.methodSig = methodSig;
		this.runSeq = runSeq;
		this.startTime = startTime;
	}
	
	public void setRunTime(long endTime){
		runTime = endTime - startTime;
	}
	
	public void initParamList(int size){
		paramList = new ObjectInfo[size];
	}
	
	public void setParam(int index, String type, String value, Boolean isPrimType, int size){
		paramList[index] = new ObjectInfo(type, value, isPrimType, size);
		if(size != -1){
			paramList[index].isSerialized = false;
		}
	}
	
	public void setBase(String type, String value, Boolean isPrimType, int size){
		base = new ObjectInfo(type, value, isPrimType, size);
	}
	
	public void setReturnVal(String type, String value, Boolean isPrimType, int size){
		returnVal = new ObjectInfo(type, value, isPrimType, size);
	}
}
