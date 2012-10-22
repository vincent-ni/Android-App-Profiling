package gov.nasa.jpf.symbolic.array;

public class ShortArrayConstant extends Array
{
	private ShortArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static ShortArrayConstant get(short[] elems)
	{
		int count = elems.length;
		short[] copy = new short[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new ShortArrayConstant(new ShortConstantArrayInternal(copy));
	}
}