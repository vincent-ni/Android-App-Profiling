package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;

public abstract class IntegerArrayExpression extends Array
{
	public IntegerArrayExpression(ArrayInternal array)
	{
		super(array);
	}
	
	//public Expression _aget(Expression index)
	//{
	//	return new IntegerArrayElem(this.array, (IntegerExpression) index);
	//}

	//public Expression _aset(Expression index, Expression value)
	//{
	//	this.array = new UpdatedIntegerArrayInternal(this.array, (IntegerExpression) index, (IntegerExpression) value);
	//	return this;
	//}
}