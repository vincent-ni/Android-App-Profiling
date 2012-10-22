package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.LongExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;

public abstract class LongArrayExpression extends Array
{
	public LongArrayExpression(ArrayInternal array)
	{
		super(array);
	}
	
	public Expression _aget(Expression index)
	{
		return new LongArrayElem(this.array, (IntegerExpression) index);
	}

	public Expression _aset(Expression index, Expression value)
	{
		this.array = new UpdatedLongArrayInternal(this.array, (IntegerExpression) index, (LongExpression) value);
		return this;
	}
}