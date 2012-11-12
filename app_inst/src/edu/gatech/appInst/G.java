package edu.gatech.appInst;

import soot.Scene;
import soot.SootClass;
import soot.SootMethodRef;
import soot.jimple.Jimple;

public class G {
	public static final Jimple jimple = Jimple.v();
	
	static final SootClass innerClass;
	static final String innerClassStr = "edu.gatech.util.innerClass";
	
	static final SootClass innerFeature;
	static final String innerFeatureStr = "edu.gatech.util.innerFeature";
	
	static final SootMethodRef testMethodRef;
	static final SootMethodRef callMethodRef;
	static final SootMethodRef endMethodRef;
	static final SootMethodRef runningMethodRef;
	static final SootMethodRef setNonPrimBaseRef;
	static final SootMethodRef setNonPrimReturnValRef;
	static final SootMethodRef setNonPrimParamRef;
	static final SootMethodRef setPrimBaseRef;
	static final SootMethodRef setPrimReturnValRef;
	static final SootMethodRef setPrimParamRef;
	static final SootMethodRef setParamCountRef;
	static final SootMethodRef updateArrayForBaseRef;
	static final SootMethodRef updateArrayForReturnRef;
	static final SootMethodRef updateArrayForParamRef;
	static final SootMethodRef addFeatureRef;
	
	static{
		innerClass = Scene.v().getSootClass(innerClassStr);
		innerFeature = Scene.v().getSootClass(innerFeatureStr);
		
		testMethodRef = innerClass.getMethod("void test()").makeRef();
		callMethodRef = innerClass.getMethod("void callMethod(java.lang.String,java.lang.String,int)").makeRef();
		endMethodRef = innerClass.getMethod("void endMethod(java.lang.String)").makeRef();
		runningMethodRef = innerClass.getMethod("void runningMethod(java.lang.String)").makeRef();
		setNonPrimBaseRef = innerClass.getMethod("void setNonPrimBase(java.lang.Object,java.lang.String)").makeRef();
		setNonPrimReturnValRef = innerClass.getMethod("void setNonPrimReturnVal(java.lang.Object,java.lang.String)").makeRef();
		setNonPrimParamRef = innerClass.getMethod("void setNonPrimParam(java.lang.Object,int,java.lang.String)").makeRef();
		setPrimBaseRef = innerClass.getMethod("void setPrimBase(java.lang.String)").makeRef();
		setPrimReturnValRef = innerClass.getMethod("void setPrimReturnVal(java.lang.String)").makeRef();
		setPrimParamRef = innerClass.getMethod("void setPrimParam(int,java.lang.String)").makeRef();
		setParamCountRef = innerClass.getMethod("void setParamCount(int)").makeRef();
		updateArrayForBaseRef = innerClass.getMethod("void updateArrayForBase(java.lang.Object,java.lang.String,int)").makeRef();
		updateArrayForReturnRef = innerClass.getMethod("void updateArrayForReturn(java.lang.Object,java.lang.String,int)").makeRef();
		updateArrayForParamRef = innerClass.getMethod("void updateArrayForParam(java.lang.Object,int,java.lang.String,int)").makeRef();
		addFeatureRef = innerFeature.getMethod("void addFeature(java.lang.String,java.lang.String,java.lang.String,int)").makeRef();
	}
}
