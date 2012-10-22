package gov.nasa.jpf.symbolic.integer;

public class IntegerBinaryOperator extends BinaryOperator
{
    public IntegerBinaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression leftOp, Expression rightOp)
    {
		IntegerExpression left = (IntegerExpression) leftOp;
		IntegerExpression right = (IntegerExpression) rightOp;
		if(left instanceof Constant && right instanceof Constant)
			assert false;
		return new BinaryIntegerExpression(this, left, right);
    }
}
