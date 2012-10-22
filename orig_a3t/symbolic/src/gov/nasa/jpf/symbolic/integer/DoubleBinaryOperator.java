package gov.nasa.jpf.symbolic.integer;

public class DoubleBinaryOperator extends BinaryOperator
{
    public DoubleBinaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression leftOp, Expression rightOp)
    {
		DoubleExpression left = (DoubleExpression) leftOp;
		DoubleExpression right = (DoubleExpression) rightOp;
		if(left instanceof Constant && right instanceof Constant)
			assert false;
		return new BinaryDoubleExpression(this, left, right);
    }
}
