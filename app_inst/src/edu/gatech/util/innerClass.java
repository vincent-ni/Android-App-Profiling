package edu.gatech.util;

import java.io.ByteArrayOutputStream;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Stack;

//import android.util.Log;

public class innerClass {
	
	public static List<MethodInfo> allMethods = new ArrayList<MethodInfo>();
	public static Stack<MethodInfo> methodStack = new Stack<MethodInfo>();
	public static int runSeq = 0;
	
	public static void test(){
		System.out.println("testing");
	}
	
	public static void callMethod(String method){
		System.out.println("calling: " + method);
		//Log.e("Profile", "calling: " + method);
		runSeq++;
		Date currentDate = new Date();
		MethodInfo info = new MethodInfo(method, runSeq, currentDate.getTime());
		allMethods.add(info);
		methodStack.push(info);
	}
	
	public static void endMethod(String method){
		System.out.println("end calling: " + method);
		Date currentDate = new Date();
		MethodInfo info = methodStack.pop();
		info.setRunTime(currentDate.getTime());
		System.out.println("  Seq: " + info.runSeq);
		System.out.println("  Runtime: " + info.runTime);
	}
	
	public static void runningMethod(String method){
		System.out.println("running: " + method);
	}
	
	
	public static void setNonPrimBase(Object obj, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setBase(type, obj.toString(), false, size);
		System.out.println("    Base: " + obj.toString() + ", and the size is: " + size);
	}
	
	public static void setPrimBase(String type){
		MethodInfo info = methodStack.peek();
		info.setBase(type, null, true, -1);
	}
	
	public static void setNonPrimReturnVal(Object obj, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setReturnVal(type, obj.toString(), false, size);
		System.out.println("    ReturnVal: " + obj.toString() + ", and the size is: " + size);
	}
	
	public static void setPrimReturnVal(String type){
		MethodInfo info = methodStack.peek();
		info.setReturnVal(type, null, true, -1);
	}
	
	public static void setNonPrimParam(Object obj, int index, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setParam(index, type, obj.toString(), false, size);
		System.out.println("    Param: " + obj.toString() + ", and the size is: " + size);
	}
	
	public static void setPrimParam(int index, String type){
		MethodInfo info = methodStack.peek();
		info.setParam(index, type, null, true, -1);
	}
	
	public static void updateArrayForParam(Object obj, int index, String elementType, int dimen){
		MethodInfo info = methodStack.peek();
		info.paramList[index].updateArrayInfo(obj, elementType, dimen);
	}
	
	public static void updateArrayForBase(Object obj, String elementType, int dimen){
		MethodInfo info = methodStack.peek();
		info.base.updateArrayInfo(obj, elementType, dimen);
	}
	
	public static void updateArrayForReturn(Object obj, String elementType, int dimen){
		MethodInfo info = methodStack.peek();
		info.returnVal.updateArrayInfo(obj, elementType, dimen);
	}
	
	public static void setParamCount(int count){
		MethodInfo info = methodStack.peek();
		info.initParamList(count);
	}
	
	/*
	 * If the obj is serializable, return the size after serialization
	 * Otherwise, return -1
	 */
	private static int getSerializedSize(Object obj){
		int size = -1;
		
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		ObjectOutput out = null;
		try {
			out = new ObjectOutputStream(bos);   
			out.writeObject(obj);
			byte[] bytes = bos.toByteArray();
			size = bytes.length;
			out.close();
			bos.close();
		} catch(Exception e){
//			xe.printStackTrace();
		}
		return size;
	}
}
