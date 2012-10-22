package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public class BooleanArrayConstant extends Array
{
	private BooleanArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static BooleanArrayConstant get(boolean[] elems)
	{
		int count = elems.length;
		boolean[] copy = new boolean[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new BooleanArrayConstant(new BooleanConstantArrayInternal(copy));
	}
}