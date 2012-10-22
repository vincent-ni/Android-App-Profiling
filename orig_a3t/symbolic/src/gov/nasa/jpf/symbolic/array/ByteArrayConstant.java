package gov.nasa.jpf.symbolic.array;

public class ByteArrayConstant extends Array
{
	private ByteArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static ByteArrayConstant get(byte[] elems)
	{
		int count = elems.length;
		byte[] copy = new byte[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new ByteArrayConstant(new ByteConstantArrayInternal(copy));
	}
}