package edu.gatech.appInst;

import soot.Scene;
import soot.SootClass;
import soot.SootMethodRef;
import soot.jimple.Jimple;

public class G {
	public static final Jimple jimple = Jimple.v();
	
	static final SootClass innerClass;
	static final String innerClassStr = "edu.gatech.util.innerClass";
	
	static final SootMethodRef testMethodRef;
	
	static final SootClass innerFeature;
	static final String innerFeatureStr = "edu.gatech.util.innerFeature";
	
	static final SootMethodRef addFeatureRef;
	
	static{
		innerClass = Scene.v().getSootClass(innerClassStr);
		innerFeature = Scene.v().getSootClass(innerFeatureStr);
		
		testMethodRef = innerClass.getMethod("void test()").makeRef();
		
		addFeatureRef = innerFeature.getMethod("void addFeature(java.lang.String)").makeRef();
	}
}
