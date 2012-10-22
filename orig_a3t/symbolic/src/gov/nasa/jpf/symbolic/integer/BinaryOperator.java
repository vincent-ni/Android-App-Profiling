package gov.nasa.jpf.symbolic.integer;
 
public abstract class BinaryOperator extends Operator
{
    public abstract Expression apply(Expression leftOp, Expression rightOp);
    
    public BinaryOperator(String op)
    {
		super(op);
    }
    
    public String toYicesString(String leftOp, String rightOp)
    {
		//return op + "(" + leftOp + "," + rightOp + ")";
		return "(" + op + " " + leftOp + " " + rightOp + ")"; // +"[" + seed + "]";
    }
}