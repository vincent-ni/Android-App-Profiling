package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class SymbolicLongArray extends Array
{
	private static int count = 0;
	
	public SymbolicLongArray(String name)
	{
		super(new LongArrayInternal("$!L$"+name));
	}

	static String makeName()
	{
		return "$!L$"+count++;
	}
}
	