package edu.gatech.appInst;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import edu.gatech.util.innerFeature;

import soot.ArrayType;
import soot.Body;
import soot.IntType;
import soot.Local;
import soot.PrimType;
import soot.SootClass;
import soot.SootMethod;
import soot.SootMethodRef;
import soot.Type;
import soot.Unit;
import soot.Value;
import soot.jimple.AbstractStmtSwitch;
import soot.jimple.AddExpr;
import soot.jimple.AssignStmt;
import soot.jimple.InstanceInvokeExpr;
import soot.jimple.IntConstant;
import soot.jimple.InvokeExpr;
import soot.jimple.Stmt;
import soot.jimple.StringConstant;
import soot.jimple.toolkits.annotation.logic.Loop;
import soot.tagkit.LineNumberTag;
import soot.toolkits.graph.LoopNestTree;
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
		LoopNestTree loopNestTree = new LoopNestTree(body);
	
		Chain<Unit> units = body.getUnits();
		Iterator iter = units.snapshotIterator();
		currentMethod = method;
		currentUnits = units;
		
		if(!loopNestTree.isEmpty()) {
			int i = 0;
//			Local[] l = new Local[loopNestTree.size()];
//			AssignStmt[] incStmt = new AssignStmt[loopNestTree.size()];
//			String[] name = new String[loopNestTree.size()];
			int num = method.getParameterCount();
			Unit u = units.getFirst();
			while (num > 0) {
				u = units.getSuccOf(u);
				num--;
            		}
			for (Loop loop : loopNestTree) {
				//Stmt stmtHead = loop.getHead();
				//System.out.println("Found a loop with head: " + u);
				//System.out.println("JumpBack stmt " + loop.getBackJumpStmt());
				String name = method.getSignature() + ":(loop)line" + ((LineNumberTag)loop.getHead().getTag("LineNumberTag")).getLineNumber();
				Local l = G.jimple.newLocal(innerFeature.getName(name), IntType.v());
				body.getLocals().add(l);
				AssignStmt assign = G.jimple.newAssignStmt(l, IntConstant.v(0));
				units.insertAfter(assign, u);
				AddExpr incExpr = G.jimple.newAddExpr(l, IntConstant.v(1));
				AssignStmt incStmt = G.jimple.newAssignStmt(l, incExpr);
				units.insertBefore(incStmt, loop.getBackJumpStmt());
				InvokeExpr incExpr1 = G.jimple.newStaticInvokeExpr(G.addFeatureRef,
					StringConstant.v(name), StringConstant.v(l.getName()), incStmt.getLeftOp());
				Stmt incStmt1 = G.jimple.newInvokeStmt(incExpr1);
				units.insertBefore(incStmt1, loop.getBackJumpStmt());
				//i++;
			}
//			Collection<Stmt> stmts = loopNestTree.last().getLoopExits();
//			for (Stmt stmt : stmts) {
//				for (int j = 0; j < loopNestTree.size(); j++) {
//					InvokeExpr incExpr1 = G.jimple.newStaticInvokeExpr(G.addFeatureRef,
//							StringConstant.v(name[j]), StringConstant.v(l[j].getName()), incStmt[j].getLeftOp());
//					Stmt incStmt1 = G.jimple.newInvokeStmt(incExpr1);
//					units.insertBefore(incStmt1, stmt);
//				}
//			}
		}
		
		while(iter.hasNext()) {
			
			Stmt u = (Stmt)iter.next();
			int lineNum = 0;
			LineNumberTag tag = (LineNumberTag)u.getTag("LineNumberTag");
			if(tag == null){
				System.out.println("Fatal error! Can't get the line number info");
				System.out.println("In " + u + " of " + method.getSignature());
				return;
			}
			lineNum = tag.getLineNumber();
			
			if(u.containsInvokeExpr()){
				InvokeExpr exp = u.getInvokeExpr();
				
				if(exp.getMethod().getSignature().contains("void <init>()>"))
					continue;
				
				currentUnits.insertBefore(G.jimple.newInvokeStmt(G.jimple.newStaticInvokeExpr(
						G.callMethodRef, StringConstant.v(exp.getMethod().getSignature()),
						StringConstant.v(method.getDeclaringClass().getName()), 
						IntConstant.v(lineNum))), u);
				
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
					if(type instanceof IntType){
						Local l = G.jimple.newLocal(innerFeature.getNextName(), IntType.v());
						body.getLocals().add(l);
						InvokeExpr incExpr = G.jimple.newStaticInvokeExpr(G.addFeatureRef,
						StringConstant.v(l.getName()), returnVal);
						Stmt incStmt = G.jimple.newInvokeStmt(incExpr);
						units.insertAfter(incStmt, u);
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
		
		if(true) return;
		
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
		
		if(true) return;

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
