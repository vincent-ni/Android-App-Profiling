package gov.nasa.jpf.symbolic.integer;

public class BinaryDoubleExpression extends DoubleExpression
{
    Expression left;
    BinaryOperator   op;
    Expression right;
    
    public BinaryDoubleExpression(BinaryOperator o, Expression l, Expression r) 
    {
		this.left = l;
		this.op = o;
		this.right = r;
    }
    
    public String toString () 
    {
		return "(" + left.toString() + " " + op.toString() + right.toString() + ")";
    }
    
    public String toYicesString()
    {
		return super.toYicesString(op.toYicesString(left.exprString(), right.exprString())); // +"[" + seed + "]";
    }
    
}
