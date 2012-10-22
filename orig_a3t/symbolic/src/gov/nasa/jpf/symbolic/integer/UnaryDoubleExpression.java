package gov.nasa.jpf.symbolic.integer;

public class UnaryDoubleExpression extends DoubleExpression 
{
    Expression operand;
    UnaryOperator   op;
    
    public UnaryDoubleExpression(UnaryOperator o, Expression operand) 
    {
		this.operand = operand;
		this.op = o;
    }
    
    public String toString () 
    {
		return "(" + op.toString() + " " + operand.toString() + ")";
    }
    
    public String toYicesString()
    {
		return super.toYicesString(op.toYicesString(operand.exprString()));
    }
}
