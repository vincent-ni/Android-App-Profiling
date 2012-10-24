package edu.gatech.appInst;

import java.util.Iterator;
import java.util.List;

import soot.ArrayType;
import soot.Body;
import soot.PrimType;
import soot.SootClass;
import soot.SootMethod;
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
				
				InvokeExpr newExp1 = G.jimple.newStaticInvokeExpr(G.callMethodRef, 
						StringConstant.v(exp.getMethod().getSignature()));
				Stmt newStmt1 = G.jimple.newInvokeStmt(newExp1);
				currentUnits.insertBefore(newStmt1, u);
				
				InvokeExpr newExpr2 = G.jimple.newStaticInvokeExpr(G.endMethodRef,
						StringConstant.v(exp.getMethod().getSignature()));
				Stmt newStmt2 = G.jimple.newInvokeStmt(newExpr2);
				currentUnits.insertAfter(newStmt2, u);
				
				currentUnits.insertBefore(G.jimple.newInvokeStmt(G.jimple.newStaticInvokeExpr(
						G.setParamCountRef, IntConstant.v(exp.getArgCount()))), u);
				for(int i = 0; i < exp.getArgCount(); i++){
					Value arg = exp.getArg(i);
					Type type = arg.getType();
					if(type instanceof PrimType)
					currentUnits.insertBefore(G.jimple.newInvokeStmt(
							G.jimple.newStaticInvokeExpr(G.setPrimParamRef, IntConstant.v(i), 
							StringConstant.v(type.toString()))), u);
					else {
						currentUnits.insertBefore(G.jimple.newInvokeStmt(
								G.jimple.newStaticInvokeExpr(G.setNonPrimParamRef, arg, IntConstant.v(i), 
								StringConstant.v(type.toString()))), u);
					}
					
					// TODO: update arrayType part
					if(type instanceof ArrayType){
						int numDim = ((ArrayType) type).numDimensions;
						Type type2 = ((ArrayType) type).getElementType();
					}
				}
				
				if(u instanceof AssignStmt){
					Value returnVal = ((AssignStmt) u).getLeftOp();
					
					Type type = returnVal.getType();
					if(type instanceof PrimType)
					currentUnits.insertAfter(G.jimple.newInvokeStmt(
							G.jimple.newStaticInvokeExpr(G.setPrimReturnValRef, StringConstant.v(
									type.toString()))), u);
					else {
						currentUnits.insertAfter(G.jimple.newInvokeStmt(
								G.jimple.newStaticInvokeExpr(G.setNonPrimReturnValRef, returnVal, 
										StringConstant.v(type.toString()))), u);
					}
				}
				
				if(exp instanceof InstanceInvokeExpr){
					Value base = ((InstanceInvokeExpr) exp).getBase();
					
					Type type = base.getType();
					if(type instanceof PrimType)
					currentUnits.insertBefore(G.jimple.newInvokeStmt(
							G.jimple.newStaticInvokeExpr(G.setPrimBaseRef, StringConstant.v(
									type.toString()))), u);
					else {
						currentUnits.insertBefore(G.jimple.newInvokeStmt(
								G.jimple.newStaticInvokeExpr(G.setNonPrimBaseRef, base, 
										StringConstant.v(type.toString()))), u);
					}
				}
			}
		}
	}
}
