package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Types;
import gov.nasa.jpf.symbolic.integer.Expression;

public class SymbolicIntegerArray extends Array
{
	private static int count = 0;

	public SymbolicIntegerArray(int type, String name)
	{
		super(new IntegerArrayInternal(name(type, name)));
	}

	private static String name(int type, String name)
	{
		switch(type){
		case Types.CHAR:
			return "$!C$"+name;
		case Types.BYTE:
			return "$!B$"+name;
		case Types.SHORT:
			return "$!S$"+name;
		case Types.INT:
			return "$!I$"+name;
		default:
			throw new RuntimeException("unexpected type " + type + " " + name);
		}
	}
	
	static String makeName()
	{
		return "$!I$"+count++;
	}
	
	static String makeName(String typePrefix)
	{
		return typePrefix+count++;
	}
}
	