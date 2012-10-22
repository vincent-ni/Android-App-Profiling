package gov.nasa.jpf.symbolic.integer;

public class BooleanBinaryOperator extends BinaryOperator
{
    public BooleanBinaryOperator(String op)
    {
		super(op);
    }
	
    public Expression apply(Expression left, Expression right)
    {
		if(left instanceof Constant && right instanceof Constant){
			assert false;
		}
		return new BinaryBooleanExpression(this, left, right);
    }
}
