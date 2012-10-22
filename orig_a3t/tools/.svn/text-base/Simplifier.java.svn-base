import org.w3c.tools.sexpr.Symbol;
import org.w3c.tools.sexpr.SimpleSExprStream;

import org.lsmp.djep.xjep.XJep;
import org.lsmp.djep.sjep.PolynomialCreator;

import java.util.Vector;
import java.io.FileInputStream;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.ByteArrayInputStream;

public class Simplifier
{
	private static final String prefix = "!F$deliverPointerEvent$android$view$MotionEvent$0";

	private SimpleSExprStream p;
	
	void simplify(String file) throws Exception
	{
		int count = 1;
		BufferedReader reader = new BufferedReader(new FileReader(file));
		String line = reader.readLine();
		while(line != null){
			//System.out.println(line);
			boolean flag = false;
			if(line.startsWith("*")){
				flag = true;
				line = line.substring(1);
			}
			p = new SimpleSExprStream(new ByteArrayInputStream(line.getBytes()));
			p.setListsAsVectors(true);
			Vector result = (Vector) visit((Vector) p.parse());
			if(!flag){
				System.out.println(count++ + ".\t" + vectorString(result));
			}
			else 
				System.out.println("\t"+vectorString(result));
			line = reader.readLine();
		}
		reader.close();
	}
	
	private Object visit(Object node)
	{
		if(node instanceof Vector){
			Vector v = (Vector) node;
			Object e1 = v.elementAt(0);
			if(e1 instanceof Symbol){
				String s = ((Symbol) e1).toString();
				if(s.equals("select")){
					Object base = visit(v.elementAt(1));
					Integer index = (Integer) visit(v.elementAt(2));
					String st = index == 0 ? "X" : "Y";
					return Symbol.makeSymbol(base.toString()+st, p.getSymbols());
				}
				/*
				else if(s.equals("+") || s.equals("-")){
					if(v.size() == 3){
						Object op1 = visit(v.elementAt(1));
						Object op2 = visit(v.elementAt(2));
						if(op1 instanceof Number){
							if(op1.toString().equals("0.0")){
								return op2;
							}
						}
						else if(op2 instanceof Number){
							if(op2.toString().equals("0.0")){
								return op1;
							}
						}
						else if(s.equals("-")){
							if(op1.toString().equals(op2.toString())){
								return new Double(0.0);
							}
						}
					}
				}
				*/
				else if(s.equals("and")){
					Object op1 = v.elementAt(1);
					Object op2 = v.elementAt(2);
					if(op1 instanceof Vector && op2 instanceof Vector){
						Vector v1 = (Vector) op1;
						Vector v2 = (Vector) op2;
						if(((Symbol) v1.elementAt(0)).toString().equals("<") &&
						   ((Symbol) v2.elementAt(0)).toString().equals(">=") &&
						   (v1.elementAt(1) instanceof Symbol) && 
						   (v2.elementAt(1) instanceof Symbol) &&
						   (v1.elementAt(1) == v2.elementAt(1))){
							
							Vector ret = new Vector(4);
							Symbol newSym = Symbol.makeSymbol("in", p.getSymbols());
							ret.add(newSym);
							ret.add(visit(v1.elementAt(1)));
							ret.add(visit(v2.elementAt(2)));
							ret.add(visit(v1.elementAt(2)));
							return ret;
						}
					}
				}
			}
			int size = v.size();
			for(int i = 0; i < size; i++)
				v.setElementAt(visit(v.elementAt(i)), i);
			return v;
		}
		else if(node instanceof Number){
			return node;
		}
		else if(node instanceof Symbol){
			String str = ((Symbol) node).toString();
			if(str.startsWith("$"))
				str = str.substring(1);
			str = str.replace(prefix, "CLICK");
			str = str.replace('$', '_');
			return Symbol.makeSymbol(str, p.getSymbols());
		}	
		throw new RuntimeException(node.toString());
	}

	private String toInfix(Object o)
	{
		if(o instanceof Vector){
			Vector v = (Vector) o;
			Object e1 = v.elementAt(0);
			if(e1 instanceof Symbol){
				String s = ((Symbol) e1).toString();
				if(s.equals("+")){
					return "("+ toInfix(v.elementAt(1)) + " " + s + " " + toInfix(v.elementAt(2)) + ")";
				} else if(s.equals("-")){
					if(v.size() == 3)
						return "("+ toInfix(v.elementAt(1)) + " " + s + " " + toInfix(v.elementAt(2)) + ")";
					else
						//unary -ve
						return "("+s+toInfix(v.elementAt(1))+")";
				}
			}
		}
		return o.toString();
	}

	private String simplifyJEP(String str)
	{
		XJep j = new XJep();
        j.setAllowUndeclared(true);
		try {
			//return j.toString(j.simplify(j.preprocess(j.parse(str))));
			return j.toString(new PolynomialCreator(j).simplify(j.preprocess(j.parse(str))));
		}catch(Exception e){
			throw new Error(e);
		}
	}

	private String vectorString(Vector v)
	{
		Object e1 = v.elementAt(0);
		if(e1 instanceof Symbol){
			String s = ((Symbol) e1).toString();
			if(s.equals("+") || s.equals("-")){
				String infix = toInfix(v);
				//System.out.println("infix: " + infix);
				return simplifyJEP(infix);
				

			}
		}

		StringBuilder builder = new StringBuilder();
		int size = v.size();
		builder.append("(");
		for(int i = 0; i < size; i++){
			Object e = v.elementAt(i);
			String str;
			if(e instanceof Vector){
				str = vectorString((Vector) e);
			}
			else
				str = e.toString();
			if(i != 0)
				builder.append(" ");
			builder.append(str);
		}
		builder.append(")");
		return builder.toString();
	}
	
	//take pc.* filename
	public static void main(String[] args) throws Exception
	{
		new Simplifier().simplify(args[0]);
	}
	
}
