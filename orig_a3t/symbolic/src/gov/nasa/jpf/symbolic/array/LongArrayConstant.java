package gov.nasa.jpf.symbolic.array;

public class LongArrayConstant extends Array
{
	private LongArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static LongArrayConstant get(long[] elems)
	{
		int count = elems.length;
		long[] copy = new long[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new LongArrayConstant(new LongConstantArrayInternal(copy));
	}
}