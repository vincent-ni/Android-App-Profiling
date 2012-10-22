package gov.nasa.jpf.symbolic.integer;

public class DoubleUnaryOperator extends UnaryOperator
{
    public DoubleUnaryOperator(String op)
    {
		super(op);
    }

    public Expression apply(Expression e)
    {
		if(e instanceof Constant)
			assert false;
	    return new UnaryDoubleExpression(this, e);
    }
}
