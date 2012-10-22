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
	
	static{
		innerClass = Scene.v().getSootClass(innerClassStr);
		
		testMethodRef = innerClass.getMethod("void test()").makeRef();
	}
}
