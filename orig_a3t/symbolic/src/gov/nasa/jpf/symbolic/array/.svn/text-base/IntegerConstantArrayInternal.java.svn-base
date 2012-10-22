package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class IntegerConstantArrayInternal extends IntegerArrayInternal
{
	private String arrayStr;

	IntegerConstantArrayInternal(int[] constElems)
	{
		super();
		this.arrayStr = java.util.Arrays.toString(constElems);
	}
	
	public String toYicesString()
	{
		String newName = SymbolicIntegerArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + arrayStr + ")");
		arrayStr = null;
		return newName;
	}
}