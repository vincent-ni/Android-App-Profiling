package edu.gatech.appInst;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import edu.gatech.util.innerFeature;

import soot.Body;
import soot.IntType;
import soot.PrimType;
import soot.SootClass;
import soot.SootMethod;
import soot.Unit;
import soot.Value;
import soot.jimple.AbstractStmtSwitch;
import soot.jimple.AddExpr;
import soot.jimple.IdentityStmt;
import soot.jimple.IntConstant;
import soot.jimple.InvokeExpr;
import soot.jimple.ReturnStmt;
import soot.jimple.Stmt;
import soot.util.Chain;
import soot.Local;
import soot.Type;
import soot.jimple.Jimple;
import soot.jimple.AssignStmt;
import soot.jimple.StringConstant;
import soot.toolkits.graph.LoopNestTree;
import soot.jimple.toolkits.annotation.logic.Loop;

public class Instrumentor extends AbstractStmtSwitch {
	
	public Instrumentor() {
	}
	
	public void instrument(List<SootClass> classes) {
		for (SootClass klass : classes) {
			klass.setApplicationClass();
			System.out.println(klass.getName());
		}
		
		for (SootClass klass : classes) {
			if(klass.getName().equals("edu.gatech.util.innerClass"))
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
		Iterator<Unit> iter = units.snapshotIterator();
		
//		System.out.println(body);
//		System.out.println("loopNestTree size "+loopNestTree.size());
		
		if(!loopNestTree.isEmpty()) {
			int i = 0;
			Local[] l = new Local[loopNestTree.size()];
			for (Loop loop : loopNestTree) {
//				Stmt u = loop.getHead();
//				System.out.println("Found a loop with head: " + u);
//				System.out.println("JumpBack stmt " + loop.getBackJumpStmt());
				l[i] = G.jimple.newLocal(innerFeature.getNextName(), IntType.v());
				body.getLocals().add(l[i]);
				AssignStmt assign = G.jimple.newAssignStmt(l[i], IntConstant.v(0));
				units.insertAfter(assign, units.getFirst());
				AddExpr incExpr = G.jimple.newAddExpr(l[i], IntConstant.v(1));
				AssignStmt incStmt = G.jimple.newAssignStmt(l[i], incExpr);
				units.insertBefore(incStmt, loop.getBackJumpStmt());
				i++;
			}
			Collection<Stmt> stmts = loopNestTree.last().getLoopExits();
			for (Stmt stmt : stmts) {
				for (int j = 0; j < loopNestTree.size(); j++) {
					InvokeExpr incExpr1 = G.jimple.newStaticInvokeExpr(G.addFeatureRef, 
							StringConstant.v(l[j].getName()));
					Stmt incStmt1 = G.jimple.newInvokeStmt(incExpr1);
					units.insertAfter(incStmt1, stmt);
				}
			}
		}
		
		while(iter.hasNext()) {
			Stmt u = (Stmt)iter.next();
			if(u.containsInvokeExpr()) {
				if(u instanceof AssignStmt){
					Value returnVal = ((AssignStmt) u).getLeftOp();
					Type type = returnVal.getType();
					if (type instanceof PrimType) {
						InvokeExpr incExpr = G.jimple.newStaticInvokeExpr(G.addFeatureRef,
								StringConstant.v(innerFeature.getNextName()));
						Stmt incStmt = G.jimple.newInvokeStmt(incExpr);
						units.insertAfter(incStmt, u);
					}
				}
			}
		}
	}
}
