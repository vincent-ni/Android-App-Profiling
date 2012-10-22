package edu.gatech.appInst;

import java.util.Iterator;
import java.util.List;

import soot.Body;
import soot.SootClass;
import soot.SootMethod;
import soot.Unit;
import soot.jimple.AbstractStmtSwitch;
import soot.jimple.IdentityStmt;
import soot.jimple.InvokeExpr;
import soot.jimple.ReturnStmt;
import soot.jimple.Stmt;
import soot.util.Chain;

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
	
		Chain<Unit> units = body.getUnits();
		Iterator iter = units.snapshotIterator();
		
		while(iter.hasNext()) {
			Stmt u = (Stmt)iter.next();
			if(u instanceof IdentityStmt){
				// do something here
			} 
			if(u instanceof ReturnStmt){
				// one example to add a method into the app
				InvokeExpr incExpr = G.jimple.newStaticInvokeExpr(G.testMethodRef);
				Stmt newStmt = G.jimple.newInvokeStmt(incExpr);
				units.insertBefore(newStmt, u);
			}
			// and more...
		}
	}
}
