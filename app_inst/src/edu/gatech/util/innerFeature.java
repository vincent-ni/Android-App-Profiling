package edu.gatech.util;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public class innerFeature {
	static int counter = 0;

	private static Map<String, String> featureKeySet = new HashMap<String, String>();
	public static Map<String, Integer> featureValSet = new HashMap<String, Integer>();
//	public static Map<String, List<String>> methodFeatureSet = new HashMap<String, List<String>>();
//	public static Map<String, List<Integer>> methodIntParaSet = new HashMap<String, List<Integer>>();
	
//	public static void setMethodPara(String method, String tag, String name, int value) {
//		System.out.println("setMethodPara: " + method + " " + tag + " " + name + " " + tag);
//		if (!methodIntParaSet.containsKey(method)) {
//			List<Integer> paraList = new ArrayList<Integer>();
//			paraList.add(value);
//			methodIntParaSet.put(method, paraList);
//		}
//		else {
//			List<Integer> paraList = methodIntParaSet.get(method);
//			paraList.add(value);
//		}
//	}

	public static void addFeature(String tag, String name, int value, int reset) {
		System.out.println("Add feature: " + tag + " " + name + " " + value + " " + reset);
		if(!featureKeySet.containsKey(tag)) {
			featureKeySet.put(tag, name);
			featureValSet.put(name, value);
//			System.out.println(tag + " " + name + " " + value);
//			Log.e("Profile", tag + " " + name + " " + value);
		}
		else {
			int newVal;
			if (reset == 1) {
				newVal = value;
			}
			else {
				newVal = featureValSet.get(name);
				if (value > 0) newVal++;
			}
			featureValSet.put(name, newVal);
//			System.out.println(tag + " " + name + " " + newVal);
//			Log.e("Profile", tag + " " + name + " " + newVal);
		}
//		if(!methodFeatureSet.containsKey(method)) {
////			System.out.println("methodFeatureSet is empty");
//			List<String> localFeatures = new ArrayList<String>();
//			localFeatures.add(name);
//			methodFeatureSet.put(method, localFeatures);
//		}
//		else {
////			System.out.println("methodFeatureSet is not empty");
//			List<String> features = methodFeatureSet.get(method);
//			boolean isFound = false;
//			for (String item : features) {
//				if (item.equals(name)) {
////					System.out.println("Found name " + name + " equals to item " + item);
//					isFound = true;
//					break;
//				}
//			}
//			if (!isFound) {
//				features.add(name);
//			}
//		}
	}

	public static String getName(String tag) {
		if (featureKeySet.containsKey(tag)) {
			return featureKeySet.get(tag);
		}
		else {
			String name = "c" + (counter++);
			featureKeySet.put(tag, name);
			featureValSet.put(name, 0);
			return name;
		}
	}
	
	public static void testPrint(PrintWriter writer) {
		Iterator iter = featureKeySet.entrySet().iterator();
		while (iter.hasNext()) {
			Map.Entry pairs = (Map.Entry)iter.next();
			System.out.println(pairs.getKey() + " : " + pairs.getValue() + " : " + 
						featureValSet.get(pairs.getValue()));
			writer.println(pairs.getKey() + " : " + pairs.getValue() + " : " 
						+ featureValSet.get(pairs.getValue()));
		}
//		for(Entry<String, List<String>> entry : methodFeatureSet.entrySet()){
//			System.out.print("info:  " + entry.getKey());
//			writer.print("info:  " + entry.getKey());
//			for(String item : entry.getValue()){
//				System.out.print(" : " + item);
//				writer.print(" : " + item);
//			}
//			System.out.println();
//			writer.println();
//		}
//		for(Entry<String, Map<String, Integer>> entry : methodIntParaSet.entrySet()) {
//			System.out.print("para:  " + entry.getKey());
//			writer.print("para:  " + entry.getKey());
//			for (Entry<String, Integer> innerEntry : entry.getValue().entrySet()) {
//				System.out.print(" : " + innerEntry.getKey() + " " + innerEntry.getValue());
//				writer.print(" : " + innerEntry.getKey() + " " + innerEntry.getValue());
//			}
//			System.out.println();
//			writer.println();
//		}
	}
}