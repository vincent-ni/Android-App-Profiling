package gov.nasa.jpf.symbolic.integer.operation;

import gov.nasa.jpf.symbolic.integer.*;

public class NEGATION extends UnaryOperator
{
	public NEGATION(String str)
	{
		super(str);
	}
	
	public Expression apply(Expression operand)
	{
		if(operand instanceof NegatedBooleanExpression)
			return ((NegatedBooleanExpression) operand).e;
		else
			return new NegatedBooleanExpression(operand);
	}
}