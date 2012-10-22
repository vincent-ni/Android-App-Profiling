package gov.nasa.jpf.symbolic.integer;

public class NegatedBooleanBinaryOperator extends BooleanBinaryOperator
{
    public NegatedBooleanBinaryOperator(String op)
    {
		super(op);
    }
	
    public Expression apply(Expression left, Expression right)
    {
		return new NegatedBooleanExpression(super.apply(left, right));
    }
}
