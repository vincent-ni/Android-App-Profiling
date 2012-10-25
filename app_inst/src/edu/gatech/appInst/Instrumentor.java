package edu.gatech.appInst;

import java.util.Iterator;
import java.util.List;

import soot.ArrayType;
import soot.Body;
import soot.PrimType;
import soot.SootClass;
import soot.SootMethod;
import soot.SootMethodRef;
import soot.Type;
import soot.Unit;
import soot.Value;
import soot.jimple.AbstractStmtSwitch;
import soot.jimple.AssignStmt;
import soot.jimple.InstanceInvokeExpr;
import soot.jimple.IntConstant;
import soot.jimple.InvokeExpr;
import soot.jimple.Stmt;
import soot.jimple.StringConstant;
import soot.util.Chain;

public class Instrumentor extends AbstractStmtSwitch {
	private SootMethod currentMethod;
	private Chain<Unit> currentUnits;
	
	public Instrumentor() {
	}
	
	public void instrument(List<SootClass> classes) {
		for (SootClass klass : classes) {
			klass.setApplicationClass();
			System.out.println(klass.getName());
		}
		
		for (SootClass klass : classes) {
			if(klass.getName().contains("edu.gatech.util"))
				continue;
			List<SootMethod> origMethods = klass.getMethods();
			for (SootMethod m : origMethods) {
				if (!m.isConcrete()) // Returns true if this method is not phantom, abstract or native, i.e.
					continue;
				instrument(m);
			}
		}
	}
	
	private void instrument(SootMethod method) {
		System.out.println("Instrumenting " + method);
		Body body = method.retrieveActiveBody();		
	
		Chain<Unit> units = body.getUnits();
		Iterator iter = units.snapshotIterator();
		currentMethod = method;
		currentUnits = units;
		
		while(iter.hasNext()) {
			
			Stmt u = (Stmt)iter.next();
			if(u.containsInvokeExpr()){
				InvokeExpr exp = u.getInvokeExpr();
				
				if(exp.getMethod().getSignature().contains("<java.lang.Object: void <init>()>"))
					continue;
				
				currentUnits.insertBefore(G.jimple.newInvokeStmt(G.jimple.newStaticInvokeExpr(
						G.callMethodRef, StringConstant.v(exp.getMethod().getSignature()))), u);
				
				currentUnits.insertAfter(G.jimple.newInvokeStmt(G.jimple.newStaticInvokeExpr(
						G.endMethodRef, StringConstant.v(exp.getMethod().getSignature()))), u);
				
				currentUnits.insertBefore(G.jimple.newInvokeStmt(G.jimple.newStaticInvokeExpr(
						G.setParamCountRef, IntConstant.v(exp.getArgCount()))), u);
				
				for(int i = 0; i < exp.getArgCount(); i++){
					Value arg = exp.getArg(i);
					Type type = arg.getType();
					if(type instanceof PrimType){
						insertInvokeStmtForPrim(G.setPrimParamRef, arg, i, "param", true, u);
//						currentUnits.insertBefore(G.jimple.newInvokeStmt(
//								G.jimple.newStaticInvokeExpr(G.setPrimParamRef, IntConstant.v(i), 
//								StringConstant.v(type.toString()))), u);
					} else {
						insertInvokeStmtForNonPrim(G.setNonPrimParamRef, arg, i, "param", true, u);
//						currentUnits.insertBefore(G.jimple.newInvokeStmt(
//								G.jimple.newStaticInvokeExpr(G.setNonPrimParamRef, arg, IntConstant.v(i), 
//								StringConstant.v(type.toString()))), u);
					}
				}
				
				if(u instanceof AssignStmt){
					Value returnVal = ((AssignStmt) u).getLeftOp();
					
					Type type = returnVal.getType();
					if(type instanceof PrimType){
						insertInvokeStmtForPrim(G.setPrimReturnValRef, returnVal, -1, "returnVal", 
								false, u);
//						currentUnits.insertAfter(G.jimple.newInvokeStmt(
//								G.jimple.newStaticInvokeExpr(G.setPrimReturnValRef, StringConstant.v(
//										type.toString()))), u);
					} else {
						insertInvokeStmtForNonPrim(G.setNonPrimReturnValRef, returnVal, -1, 
								"returnVal", false, u);
//						currentUnits.insertAfter(G.jimple.newInvokeStmt(
//								G.jimple.newStaticInvokeExpr(G.setNonPrimReturnValRef, returnVal, 
//										StringConstant.v(type.toString()))), u);
					}
				}
				
				if(exp instanceof InstanceInvokeExpr){
					Value base = ((InstanceInvokeExpr) exp).getBase();

					Type type = base.getType();
					if(type instanceof PrimType){
						insertInvokeStmtForPrim(G.setPrimBaseRef, base, -1, "base", true, u);
//						currentUnits.insertBefore(G.jimple.newInvokeStmt(
//						G.jimple.newStaticInvokeExpr(G.setPrimBaseRef, StringConstant.v(
//								type.toString()))), u);
					} else {
						insertInvokeStmtForNonPrim(G.setNonPrimBaseRef, base, -1, "base", true, u);
//						currentUnits.insertBefore(G.jimple.newInvokeStmt(
//								G.jimple.newStaticInvokeExpr(G.setNonPrimBaseRef, base, 
//										StringConstant.v(type.toString()))), u);
					}
				}
			}
		}
	}
	
	/*
	 * which could be "base", "returnVal", "param"
	 */
	private void insertInvokeStmtForPrim(SootMethodRef ref, Value value, int index, String which, 
			boolean before, Stmt u){
		Type type = value.getType();
		InvokeExpr expr;
		
		if(which.equals("param")){
			expr = G.jimple.newStaticInvokeExpr(ref, IntConstant.v(index), 
					StringConstant.v(type.toString()));
		} else {
			expr = G.jimple.newStaticInvokeExpr(ref, StringConstant.v(type.toString()));
		}
		if(before){
			currentUnits.insertBefore(G.jimple.newInvokeStmt(expr), u);
		} else {
			currentUnits.insertAfter(G.jimple.newInvokeStmt(expr), u);
		}
	}
	
	/*
	 * which could be "base", "returnVal", "param"
	 */
	private void insertInvokeStmtForNonPrim(SootMethodRef ref, Value value, int index, 
			String which, boolean before, Stmt u){
		Type type = value.getType();
		InvokeExpr expr;
		
		if(which.equals("param")){
			expr = G.jimple.newStaticInvokeExpr(ref, value, IntConstant.v(index), 
					StringConstant.v(type.toString()));
		} else {
			expr = G.jimple.newStaticInvokeExpr(ref, value, StringConstant.v(type.toString()));
		}
		if(before){
			currentUnits.insertBefore(G.jimple.newInvokeStmt(expr), u);
		} else {
			currentUnits.insertAfter(G.jimple.newInvokeStmt(expr), u);
		}
		
		// TODO: update arrayType part
		if(type instanceof ArrayType){
			int numDim = ((ArrayType) type).numDimensions;
			Type elementType = ((ArrayType) type).getElementType();
			
			InvokeExpr exp;
			if(which.equals("param")){
				exp = G.jimple.newStaticInvokeExpr(G.updateArrayForParamRef, value,
						IntConstant.v(index), StringConstant.v(elementType.toString()), 
						IntConstant.v(numDim));
			} else if(which.equals("base")){
				exp = G.jimple.newStaticInvokeExpr(G.updateArrayForBaseRef, value, 
						StringConstant.v(elementType.toString()), IntConstant.v(numDim));
			} else {
				exp = G.jimple.newStaticInvokeExpr(G.updateArrayForReturnRef, value,
						StringConstant.v(elementType.toString()), IntConstant.v(numDim));
			}
			
			if(before){
				currentUnits.insertBefore(G.jimple.newInvokeStmt(exp), u);
			} else {
				currentUnits.insertAfter(G.jimple.newInvokeStmt(exp), u);
			}
		}
	}
}
