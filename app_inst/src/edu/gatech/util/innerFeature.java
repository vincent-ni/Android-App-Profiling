package edu.gatech.util;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import edu.gatech.log.helper.MethodInfo;

import android.util.Log;

public class innerFeature {
	static int counter = 0;

	private static Map<String, String> featureKeySet = new HashMap<String, String>();
	public static Map<String, Integer> featureValSet = new HashMap<String, Integer>();
	public static Map<String, List<String>> methodFeatureSet = new HashMap<String, List<String>>();

	public static void addFeature(String method, String tag, String name, int value) {
		System.out.println("Add feature: " + method + " " + tag + " " + name + " " + value);
		if(!featureKeySet.containsKey(tag)) {
			featureKeySet.put(tag, name);
			featureValSet.put(name, value);
//			System.out.println(tag + " " + name + " " + value);
//			Log.e("Profile", tag + " " + name + " " + value);
		}
		else {
			int newVal = featureValSet.get(name);
			if (value > 0) newVal++;
			featureValSet.put(name, newVal);
//			System.out.println(tag + " " + name + " " + newVal);
//			Log.e("Profile", tag + " " + name + " " + newVal);
		}
		if(!methodFeatureSet.containsKey(method)) {
//			System.out.println("methodFeatureSet is empty");
			List<String> localFeatures = new ArrayList<String>();
			localFeatures.add(name);
			methodFeatureSet.put(method, localFeatures);
		}
		else {
//			System.out.println("methodFeatureSet is not empty");
			List<String> features = methodFeatureSet.get(method);
			boolean isFound = false;
			for (String item : features) {
				if (item.equals(name)) {
//					System.out.println("Found name " + name + " equals to item " + item);
					isFound = true;
					break;
				}
			}
			if (!isFound) {
				features.add(name);
			}
		}
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
		for(Entry<String, List<String>> entry : methodFeatureSet.entrySet()){
			System.out.print("info:  " + entry.getKey());
			writer.print("info:  " + entry.getKey());
			for(String item : entry.getValue()){
				System.out.print(" : " + item);
				writer.print(" : " + item);
			}
			System.out.println();
			writer.println();
		}
	}
}