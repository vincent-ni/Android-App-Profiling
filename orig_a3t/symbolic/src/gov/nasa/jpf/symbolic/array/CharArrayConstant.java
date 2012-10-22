package gov.nasa.jpf.symbolic.array;

public class CharArrayConstant extends Array
{
	private CharArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static CharArrayConstant get(char[] elems)
	{
		int count = elems.length;
		char[] copy = new char[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new CharArrayConstant(new CharConstantArrayInternal(copy));
	}

}