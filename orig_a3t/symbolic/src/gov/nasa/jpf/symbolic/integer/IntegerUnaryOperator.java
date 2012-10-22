package gov.nasa.jpf.symbolic.integer;

public class IntegerUnaryOperator extends UnaryOperator
{
    public IntegerUnaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression e)
    {
		if(e instanceof Constant)
			assert false;
		return new UnaryIntegerExpression(this, e);
    }
}
