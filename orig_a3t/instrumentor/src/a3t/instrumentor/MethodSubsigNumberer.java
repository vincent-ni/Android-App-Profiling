package a3t.instrumentor;

import soot.SootMethod;

public class MethodSubsigNumberer extends SigNumberer {
	public int getOrMakeId(SootMethod m) {
		String s = m.getSubSignature();
		return getNumber(s);
	} 
}
