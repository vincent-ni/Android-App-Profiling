package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.DoubleExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;

public abstract class DoubleArrayExpression extends Array
{
	public DoubleArrayExpression(ArrayInternal array)
	{
		super(array);
	}
	
	public Expression _aget(Expression index)
	{
		return new DoubleArrayElem(this.array, (IntegerExpression) index);
	}

	public Expression _aset(Expression index, Expression value)
	{
		this.array = new UpdatedDoubleArrayInternal(this.array, (IntegerExpression) index, (DoubleExpression) value);
		return this;
	}
}