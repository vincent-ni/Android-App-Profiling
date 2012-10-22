package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class ShortConstantArrayInternal extends IntegerArrayInternal
{
	ShortConstantArrayInternal(short[] constElems)
	{
		super(constString(constElems));
	}
	
	private static String constString(short[] constElems)
	{
		String str = java.util.Arrays.toString(constElems);
		String newName = SymbolicIntegerArray.makeName("$!S$");
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
}