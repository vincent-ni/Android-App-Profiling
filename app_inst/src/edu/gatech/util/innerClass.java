package edu.gatech.util;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.FileWriter;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import edu.gatech.log.helper.FeatInfo;

public class innerClass {
	
	public static List<MethodInfo> allMethods = new ArrayList<MethodInfo>();
	public static Stack<MethodInfo> methodStack = new Stack<MethodInfo>();
//	public static Map<String, Integer> currentFeatKeyVals = new HashMap<String, Integer>();
	public static Map<Integer, List<String>> currentFeats = new HashMap<Integer, List<String>>();
	public static int runSeq = 0;
	
	public static void test(){
		System.out.println("testing");
	}
	
	public static void callMethod(String method, String fileName, int linenum){
//		System.out.println("calling: " + method);
//		Log.e("Profile", "calling: " + method);
		runSeq++;
		
		if (innerFeature.featureKeySet.size() > 0) {
			Iterator iter = innerFeature.featureKeySet.entrySet().iterator();
			List<String> featList = new ArrayList<String>();
			while (iter.hasNext()) {
				Map.Entry pairs = (Map.Entry)iter.next();
				featList.add(pairs.getKey() + " : " + innerFeature.featureKeySet.get(pairs.getKey()) 
							+ " : " + innerFeature.featureValSet.get(pairs.getValue()));
			}
			currentFeats.put(runSeq, featList);
		}
		
		Date currentDate = new Date();
		MethodInfo info = new MethodInfo(method, runSeq, currentDate.getTime(), fileName, linenum);
		allMethods.add(info);
		methodStack.push(info);
//		if (innerFeature.featureKeySet.size() > 0) {
//			Iterator iter = innerFeature.featureKeySet.entrySet().iterator();
//			while (iter.hasNext()) {
//				Map.Entry pairs = (Map.Entry)iter.next();
//				String key = (String) pairs.getKey();
//				int value = innerFeature.featureValSet.get(pairs.getValue());
//				currentFeatKeyVals.put(key, value);
//			}
//		}
	}
	
	public static void endMethod(String method){
//		System.out.println("end calling: " + method);
//		Log.e("Profile", "end calling: " + method);
		Date currentDate = new Date();
		MethodInfo info = methodStack.pop();
		info.setRunTime(currentDate.getTime());
//		System.out.println("  Seq: " + info.runSeq);
//		System.out.println("  Runtime: " + info.runTime);
//		Log.e("Profile", "  Runtime: " + info.runTime);
		printInfo(info);
	}
	
	public static void runningMethod(String method){
//		System.out.println("running: " + method);
	}
	
	
	public static void setNonPrimBase(Object obj, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setBase(type, obj.toString(), false, size);
//		System.out.println("    Base: " + obj.toString() + ", and the size is: " + size);
	}
	
	public static void setPrimBase(String type){
		MethodInfo info = methodStack.peek();
		info.setBase(type, null, true, -1);
	}
	
	public static void setNonPrimReturnVal(Object obj, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setReturnVal(type, obj.toString(), false, size);
//		System.out.println("    ReturnVal: " + obj.toString() + ", and the size is: " + size);
	}
	
	public static void setPrimReturnVal(String type){
		MethodInfo info = methodStack.peek();
		info.setReturnVal(type, null, true, -1);
	}
	
	public static void setNonPrimParam(Object obj, int index, String type){
		MethodInfo info = methodStack.peek();
		int size = getSerializedSize(obj);
		info.setParam(index, type, obj.toString(), false, size);
//		System.out.println("    Param: " + obj.toString() + ", and the size is: " + size);
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
	
	public static void printInfo(MethodInfo info){
		PrintWriter writer;
		try {
			writer = new PrintWriter(new BufferedWriter(new FileWriter("/mnt/sdcard/log.txt", true)));
			writer.println("Time:  " + info.runTime + " : " + info.fileName + " : line" 
					+ info.lineNum + " : " + info.methodSig + " : " + info.runSeq);
//			innerFeature.testPrint(writer);
			
			if(currentFeats.containsKey(info.runSeq)){
				List<String> featList = currentFeats.get(info.runSeq);
				for(String feat : featList){
					writer.println(feat);
				}
				currentFeats.remove(info.runSeq);
			}
//			Iterator iter = currentFeatKeyVals.entrySet().iterator();
//			while (iter.hasNext()) {
//				Map.Entry pairs = (Map.Entry)iter.next();
//				String key = (String) pairs.getKey();
//				System.out.println(key + " : " + innerFeature.featureKeySet.get(key) + " : " + pairs.getValue());
//				writer.println(key + " : " + innerFeature.featureKeySet.get(key) + " : " + pairs.getValue());
//			}
//			currentFeatKeyVals.clear();
			writer.close();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	public static void printAllInfo(){
		PrintWriter writer;
		try {
			writer = new PrintWriter(new BufferedWriter(new FileWriter("/mnt/sdcard/log.txt", true)));
			for(int i = 0; i < allMethods.size(); i++){
				MethodInfo info = allMethods.get(i);
				writer.println("Time:  " + info.runTime + " : " + info.fileName + " : line" 
						+ info.lineNum + " : " + info.methodSig + " : " + info.runSeq);
			}
			writer.close();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}
