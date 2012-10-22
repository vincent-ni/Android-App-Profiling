package gov.nasa.jpf.symbolic.array;

public class DoubleArrayConstant extends Array
{
	private DoubleArrayConstant(ArrayInternal array)
	{
		super(array);
	}

	public static DoubleArrayConstant get(double[] elems)
	{
		int count = elems.length;
		double[] copy = new double[count];
		System.arraycopy(elems, 0, copy, 0, count);
		return new DoubleArrayConstant(new DoubleConstantArrayInternal(copy));
	}
}