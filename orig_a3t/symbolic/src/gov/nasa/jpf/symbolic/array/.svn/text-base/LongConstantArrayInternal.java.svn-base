package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class LongConstantArrayInternal extends LongArrayInternal
{
	private String arrayStr;

	LongConstantArrayInternal(long[] constElems)
	{
		super();
		this.arrayStr = java.util.Arrays.toString(constElems);
	}

	public String toYicesString()
	{
		String newName = SymbolicLongArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + arrayStr + ")");
		arrayStr = null;
		return newName;
	}
}