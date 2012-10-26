package edu.gatech.util;

import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.Map.Entry;

public class innerFeature {
	static int counter = 0;
	
	public static Map<String, Integer> allFeatures = new HashMap<String, Integer>();
		
	public static void addFeature(String name, int value) {
		System.out.println("Addfeature " + name);
		allFeatures.put(name, value);
	}
	
	public static String getNextName() {
		return "c" + (counter++);
	}
	
	public static void testPrint() {
		Iterator iter = allFeatures.entrySet().iterator();
		while (iter.hasNext()) {
			 Map.Entry pairs = (Map.Entry)iter.next();
		     System.out.println(pairs.getKey() + " = " + pairs.getValue());
		}
	}
}
