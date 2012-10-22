package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.FloatExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.operation.Operations;

public class FloatArrayElem extends FloatExpression
{
	private ArrayInternal array;
	private IntegerExpression index;
	
	public FloatArrayElem(ArrayInternal array, IntegerExpression index)
	{
		this.array = array;
		this.index = index;
	}
	
	public String toYicesString()
	{
		return super.toYicesString(Operations.v.array_get(array.exprString(), index.exprString()));
	}

}