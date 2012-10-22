package gov.nasa.jpf.symbolic.integer;

public class LongUnaryOperator extends UnaryOperator
{
    public LongUnaryOperator(String op)
    {
		super(op);
    }

    public Expression apply(Expression e)
    {
		if(e instanceof Constant)
			assert false;
		return new UnaryLongExpression(this, e);
    }
}
