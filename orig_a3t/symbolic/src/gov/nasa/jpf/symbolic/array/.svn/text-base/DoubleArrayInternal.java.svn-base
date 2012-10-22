package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.DoubleExpression;

class DoubleArrayInternal extends ArrayInternal
{
	DoubleArrayInternal()
	{
		super();
	}

	DoubleArrayInternal(String name)
	{
		super(name);
	}

	public Expression get(Expression index)
	{
		return new DoubleArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedDoubleArrayInternal(this, (IntegerExpression) index, (DoubleExpression) value);
	}
}