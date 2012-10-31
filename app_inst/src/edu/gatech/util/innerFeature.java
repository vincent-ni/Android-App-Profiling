package edu.gatech.util;

import java.io.PrintWriter;
import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;

import android.util.Log;

public class innerFeature {
	static int counter = 0;

	private static Map<String, String> featureKeySet = new HashMap<String, String>();
	public static Map<String, Integer> featureValSet = new HashMap<String, Integer>();

	public static void addFeature(String tag, String name, int value) {
		System.out.println("Add feature: " + tag +" " + name + " " + value);
		if(!featureKeySet.containsKey(tag)) {
			featureKeySet.put(tag, name);
			featureValSet.put(name, value);
			System.out.println(tag + " " + name + " " + value);
		}
		else {
			int newVal = featureValSet.get(name);
			if (value > 0) newVal++;
			featureValSet.put(name, newVal);
			System.out.println(tag + " " + name + " " + newVal);
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
			System.out.println(pairs.getKey() + " " + pairs.getValue() + featureValSet.get(pairs.getValue()));
			writer.println("--" + pairs.getKey() + " " + pairs.getValue() + featureValSet.get(pairs.getValue()));
		}
	}
}