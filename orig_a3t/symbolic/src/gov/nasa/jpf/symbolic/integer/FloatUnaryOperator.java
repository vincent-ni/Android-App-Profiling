package gov.nasa.jpf.symbolic.integer;

public class FloatUnaryOperator extends UnaryOperator
{
    public FloatUnaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression e)
    {
		if(e instanceof Constant)
			assert false;
		return new UnaryFloatExpression(this, e);
    }
}
