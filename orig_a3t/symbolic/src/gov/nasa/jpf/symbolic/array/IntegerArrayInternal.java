package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;

class IntegerArrayInternal extends ArrayInternal
{
	IntegerArrayInternal()
	{
		super();
	}

	IntegerArrayInternal(String name)
	{
		super(name);
	}

	public Expression get(Expression index)
	{
		return new IntegerArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedIntegerArrayInternal(this, (IntegerExpression) index, (IntegerExpression) value);
	}
}