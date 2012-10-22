package gov.nasa.jpf.symbolic.integer;


public class BinaryBooleanExpression extends BooleanExpression
{
    Expression left;
    BinaryOperator   op;
    Expression right;
    
    public BinaryBooleanExpression (BinaryOperator o, Expression l, Expression r) 
    {
		this.left = l;
		this.op = o;
		this.right = r;
    }

    public String toString () 
    {
		return "(" + left.toString() + " " + op.toString() + right.toString() + ")"; // + "[" + seed + "]";
    }
    
    public String toYicesString()
    {
		return op.toYicesString(left.exprString(), right.exprString()); // +"[" + seed + "]";
    }
}
