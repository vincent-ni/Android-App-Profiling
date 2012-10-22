package gov.nasa.jpf.symbolic.integer;

public class LongBinaryOperator extends BinaryOperator
{
    public LongBinaryOperator(String op)
    {
		super(op);
    }
    
    public Expression apply(Expression leftOp, Expression rightOp)
    {
		LongExpression left = (LongExpression) leftOp;
		if(left instanceof Constant && rightOp instanceof Constant)
			assert false;
		return new BinaryLongExpression(this, left, rightOp);
    }
}
