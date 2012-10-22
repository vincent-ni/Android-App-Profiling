package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class FloatConstantArrayInternal extends FloatArrayInternal
{
	private String arrayStr;

	FloatConstantArrayInternal(float[] constElems)
	{
		super();
		this.arrayStr = java.util.Arrays.toString(constElems);
	}
	
	public String toYicesString()
	{
		String newName = SymbolicFloatArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + arrayStr + ")");
		arrayStr = null;
		return newName;
	}
}