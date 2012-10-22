package gov.nasa.jpf.symbolic.integer;

public class FloatBinaryOperator extends BinaryOperator
{
    public FloatBinaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression leftOp, Expression rightOp)
    {
		FloatExpression left = (FloatExpression) leftOp;
		FloatExpression right = (FloatExpression) rightOp;
		if(left instanceof Constant && right instanceof Constant)
			assert false;
		return new BinaryFloatExpression(this, left, right);
    }
}
