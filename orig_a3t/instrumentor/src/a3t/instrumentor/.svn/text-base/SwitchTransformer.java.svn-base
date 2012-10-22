package a3t.instrumentor;

import soot.SootMethod;
import soot.Unit;
import soot.Body;
import soot.Local;
import soot.Immediate;
import soot.jimple.TableSwitchStmt;
import soot.jimple.LookupSwitchStmt;
import soot.jimple.Stmt;
import soot.jimple.IfStmt;
import soot.jimple.Constant;
import soot.jimple.IntConstant;
import soot.jimple.GotoStmt;
import soot.jimple.EqExpr;

import java.util.ArrayList;
import java.util.List;

public final class SwitchTransformer {

	private SwitchTransformer() { }

	public static void transform(SootMethod method) {		
		Body body = method.retrieveActiveBody();		
		G.editor.newBody(body, method);
		
		while (G.editor.hasNext()) {
			Stmt s = G.editor.next();
			//System.out.println(">> " + s);

			if (s.branches() && !(s instanceof GotoStmt)) {
				if (s instanceof LookupSwitchStmt) {
					LookupSwitchStmt ls = (LookupSwitchStmt) s;
					handleLookupSwitchStmt(ls);
				} else if (s instanceof TableSwitchStmt) {
					TableSwitchStmt ts = (TableSwitchStmt) s;
					handleTableSwitchStmt(ts);
				} else {
					if (!(s instanceof IfStmt))
						throw new RuntimeException("Unknown kind of branch stmt: " + s);
				}
			}	   
		}
	}

	private static void handleTableSwitchStmt(TableSwitchStmt ts) {
        Immediate key = (Immediate) ts.getKey();
        if (key instanceof Constant) return;
        int high = ts.getHighIndex();
        int low = ts.getLowIndex();
        List<IntConstant> values = new ArrayList();
		for (int i = low; i <= high; i++)
			values.add(IntConstant.v(i));
        List<Unit> targets = ts.getTargets();
        Unit defaultTarget = ts.getDefaultTarget();
		convertSwitchToBranches((Local) key, values, targets, defaultTarget);
	}

	private static void handleLookupSwitchStmt(LookupSwitchStmt ls) {
        Immediate key = (Immediate) ls.getKey();
        if (key instanceof Constant) return;
        List<IntConstant> values = ls.getLookupValues();
        List<Unit> targets = ls.getTargets();
        Unit defaultTarget = ls.getDefaultTarget();
		convertSwitchToBranches((Local) key, values, targets, defaultTarget);
	}

    private static void convertSwitchToBranches(Local key, List<IntConstant> values, List<Unit> targets, Unit defaultTarget) {
        int n = values.size();
        if (targets.size() != n)
            throw new RuntimeException("Number of values and targets do not match.");
        for (int i = 0; i < n; i++) {
			EqExpr cond = G.eqExpr(key, (IntConstant) values.get(i));
			IfStmt ifStmt = G.jimple.newIfStmt(cond, targets.get(i));
			G.editor.insertStmt(ifStmt);
		}
        G.editor.insertStmt(G.jimple.newGotoStmt(defaultTarget));
        G.editor.removeOriginalStmt();
    }
}
