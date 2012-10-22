package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class SymbolicFloatArray extends Array
{
	private static int count = 0;

	public SymbolicFloatArray(String name)
	{
		super(new FloatArrayInternal("$!F$"+name));
	}

	public SymbolicFloatArray(String name, int[] symIndices)
	{
		super(new FloatArrayInternal("$!F$"+name, symIndices));
	}
	
	static String makeName()
	{
		return "$!F$"+count++;
	}
}
	