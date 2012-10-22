package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.LongExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

class LongArrayInternal extends ArrayInternal
{
	LongArrayInternal()
	{
		super();
	}


	LongArrayInternal(String name)
	{
		super(name);
	}

	public Expression get(Expression index)
	{
		return new LongArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedLongArrayInternal(this, (IntegerExpression) index, (LongExpression) value);
	}
}