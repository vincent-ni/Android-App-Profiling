package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class ByteConstantArrayInternal extends IntegerArrayInternal
{
	ByteConstantArrayInternal(byte[] constElems)
	{
		super(constString(constElems));
	}

	private static String constString(byte[] constElems)
	{
		String str = java.util.Arrays.toString(constElems);
		String newName = SymbolicIntegerArray.makeName("$!B$");
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
}