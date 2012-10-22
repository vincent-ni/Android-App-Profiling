package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.LongExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.operation.Operations;

public class LongArrayElem extends LongExpression
{
	private ArrayInternal array;
	private IntegerExpression index;
	
	public LongArrayElem(ArrayInternal array,  IntegerExpression index)
	{
		this.array = array;
		this.index = index;
	}

	public String toYicesString()
	{
		return super.toYicesString(Operations.v.array_get(array.exprString(), index.exprString()));
	}
}