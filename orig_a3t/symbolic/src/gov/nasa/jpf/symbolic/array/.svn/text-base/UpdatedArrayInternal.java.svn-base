package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.FloatExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.operation.Operations;

abstract class UpdatedArrayInternal extends ArrayInternal
{
	ArrayInternal oldArray;
	IntegerExpression index;
	Expression value;

	public UpdatedArrayInternal(ArrayInternal oldArray, IntegerExpression index, Expression value)
	{
		this.oldArray = oldArray;
		this.index = index;
		this.value = value;
	}

	public String toYicesString()
	{
		return Operations.v.array_set(oldArray.exprString(), index.exprString(), value.exprString());
	}	

}