package gov.nasa.jpf.symbolic.array;

public class IntegerArrayConstant extends Array
{
	private IntegerArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static IntegerArrayConstant get(int[] elems)
	{
		int count = elems.length;
		int[] copy = new int[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new IntegerArrayConstant(new IntegerConstantArrayInternal(copy));
	}
}