package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class BooleanConstantArrayInternal extends IntegerArrayInternal
{
	BooleanConstantArrayInternal(boolean[] constElems)
	{
		super(constString(constElems));
	}

	private static String constString(boolean[] constElems)
	{
		String str = java.util.Arrays.toString(constElems);
		String newName = SymbolicIntegerArray.makeName("$!Z$");
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
}