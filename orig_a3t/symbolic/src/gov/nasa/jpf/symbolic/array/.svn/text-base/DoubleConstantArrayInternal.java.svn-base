package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class DoubleConstantArrayInternal extends DoubleArrayInternal
{
	private String arrayStr;

	DoubleConstantArrayInternal(double[] constElems)
	{
		super();
		this.arrayStr = java.util.Arrays.toString(constElems);
	}

	public String toYicesString()
	{
		String newName = SymbolicDoubleArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + arrayStr + ")");
		arrayStr = null;
		return newName;
	}
}