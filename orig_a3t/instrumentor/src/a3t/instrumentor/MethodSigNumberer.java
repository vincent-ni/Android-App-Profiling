package a3t.instrumentor;

import soot.SootMethod;

public class MethodSigNumberer extends SigNumberer {
	public int getOrMakeId(SootMethod m) {
		String s = m.getSignature();
		return getNumber(s);
	} 
}
