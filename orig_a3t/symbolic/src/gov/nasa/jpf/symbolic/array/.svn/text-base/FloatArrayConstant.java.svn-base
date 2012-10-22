package gov.nasa.jpf.symbolic.array;

public class FloatArrayConstant extends Array
{
	private FloatArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static FloatArrayConstant get(float[] elems)
	{
		int count = elems.length;
		float[] copy = new float[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new FloatArrayConstant(new FloatConstantArrayInternal(copy));
	}

}