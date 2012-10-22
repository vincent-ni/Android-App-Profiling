package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.FloatExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;

public abstract class FloatArrayExpression extends Array
{
	public FloatArrayExpression(ArrayInternal array)
	{
		super(array);
	}
	
	//public Expression _aget(Expression index)
	//{
	//	return new FloatArrayElem(this.array, (IntegerExpression) index);
	//}

	public Expression _aset(Expression index, Expression value)
	{
		this.array = new UpdatedFloatArrayInternal(this.array, (IntegerExpression) index, (FloatExpression) value);
		return this;
	}
}