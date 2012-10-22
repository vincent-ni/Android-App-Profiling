package a3t.instrumentor;

import java.io.File;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Iterator;
import java.util.Map;

import soot.Scene;
import soot.SootClass;
import soot.SootMethod;
import soot.Local;
import soot.Immediate;
import soot.Modifier;
import soot.Type;
import soot.VoidType;
import soot.RefType;
import soot.PrimType;
import soot.BooleanType;
import soot.Unit;
import soot.Body;
import soot.util.Chain;
import soot.jimple.Stmt;
import soot.jimple.NullConstant;
import soot.jimple.IntConstant;
import soot.jimple.StringConstant;
import soot.jimple.IdentityStmt;
import soot.jimple.ParameterRef;

public class InputMethodsHandler
{
	static void instrument(String fileName)
	{
		if(fileName == null)
			return;

		File file = new File(fileName);
		if (!file.exists()) {
			System.out.println("input_methods.txt not found.");
			return;
		}
		try{
			BufferedReader reader = new BufferedReader(new FileReader(file));
			String line = reader.readLine();
			while (line != null) {
				int index = line.indexOf(' ');
				String className = line.substring(0, index);
				if (!Scene.v().containsClass(className)) {
					System.out.println("will not inject symbolic values into method: " + line);
				}
				else {
					String methodSig = line.substring(index+1).trim();
					SootClass declClass = Scene.v().getSootClass(className);
					if (declClass.declaresMethod(methodSig)) {
						System.out.println("inject symbolic values into method: " + line);
						SootMethod method = declClass.getMethod(methodSig);
						apply(method);
					}
					else {
						System.out.println("will not inject symbolic values into method: " + line);
					}
				}
				line = reader.readLine();
			}
			reader.close();
		}
		catch(IOException e){
			throw new Error(e);
		}
	}

	static void apply(SootMethod method)
	{
		Body body = method.retrieveActiveBody();
		List<Local> params = new ArrayList();
		Chain<Unit> units = body.getUnits().getNonPatchingChain();
		for (Unit u : units) {
			Stmt s = (Stmt) u;
			if (Instrumentor.paramOrThisIdentityStmt(s)) {
				if (((IdentityStmt) s).getRightOp() instanceof ParameterRef) {
					//that is, dont add ThisRef's to params
					params.add((Local) ((IdentityStmt) s).getLeftOp());
				}
				continue;
			}
			else {
				SootMethod injector = addInjector(method);
				units.insertBefore(G.jimple.newInvokeStmt(G.staticInvokeExpr(injector.makeRef(), params)), s);
				return;
			}
		}		
	}

	public static SootMethod addInjector(SootMethod method) 
	{
		StringBuilder builder = new StringBuilder();
		builder.append(method.getName());
		for (Iterator pit = method.getParameterTypes().iterator(); pit.hasNext();) {
			Type ptype = (Type) pit.next();
			builder.append("$");
			builder.append(ptype.toString().replace('.','$'));
		}

		String id = builder.toString();

		SootMethod injector = new SootMethod(method.getName()+"$sym",
				new ArrayList(method.getParameterTypes()),
				VoidType.v(),
				Modifier.STATIC | Modifier.PRIVATE);
		method.getDeclaringClass().addMethod(injector);

		G.addBody(injector);
		List paramLocals = G.paramLocals(injector);

		List symArgs = new ArrayList();
		symArgs.add(IntConstant.v(-1));

		if (!method.isStatic())
			symArgs.add(NullConstant.v());

		int i = 0;
		for (Iterator pit = method.getParameterTypes().iterator(); pit.hasNext();) {
			Type ptype = (Type) pit.next();
			if (((ptype instanceof PrimType) && !(ptype instanceof BooleanType)) || ptype instanceof RefType) {
				SootMethod m = G.symValueInjectorFor(ptype);
				Local l = G.newLocal(G.EXPRESSION_TYPE);
				String symbol = id + "$" + i;
				G.assign(l, G.staticInvokeExpr(m.makeRef(), (Local) paramLocals.get(i), StringConstant.v(symbol)));
				symArgs.add(l);
			} else {
				symArgs.add(NullConstant.v());
			}
			i++;
		}
		G.invoke(G.staticInvokeExpr(G.argPush[symArgs.size()-1], symArgs));
		G.retVoid();
		G.debug(injector, G.DEBUG);
		return injector;
	}
}
