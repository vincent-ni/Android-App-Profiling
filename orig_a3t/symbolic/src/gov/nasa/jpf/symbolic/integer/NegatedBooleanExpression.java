package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public class NegatedBooleanExpression extends BooleanExpression
{
	public Expression e;

	public NegatedBooleanExpression(Expression e)
	{
		this.e = e;
	}

	public String toYicesString()
	{
		return BooleanExpression.NEGATION.toYicesString(e.toYicesString());
	}
	
	public String toString()
	{
		return toYicesString();
	}
	
}