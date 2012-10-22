package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class SymbolicDoubleArray extends Array
{
	private static int count = 0;

	public SymbolicDoubleArray(String name)
	{
		super(new DoubleArrayInternal("$!D$"+name));
	}
	
	static String makeName()
	{
		return "$!D$"+count++;
	}
}
	