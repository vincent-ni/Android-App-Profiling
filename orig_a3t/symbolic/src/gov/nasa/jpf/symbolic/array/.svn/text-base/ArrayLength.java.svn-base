package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class ArrayLength extends IntegerExpression
{
	private Array array;
	
	public ArrayLength(Array array)
	{
		this.array = array;
	}
	
	public String toYicesString()
	{
		return super.toYicesString("array_len("+array.exprString()+")");
	}
}