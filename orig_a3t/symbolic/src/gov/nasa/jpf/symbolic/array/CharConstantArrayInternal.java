package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class CharConstantArrayInternal extends IntegerArrayInternal
{
	CharConstantArrayInternal(char[] constElems)
	{
		super(constString(constElems));
	}

	private static String constString(char[] constElems)
	{
		String str = java.util.Arrays.toString(constElems);
		String newName = SymbolicIntegerArray.makeName("$!C$");
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
}