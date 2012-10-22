package gov.nasa.jpf.symbolic.integer;

public abstract class UnaryOperator extends Operator
{
    public abstract Expression apply(Expression operand);

    public UnaryOperator(String op)
    {
		super(op);
    }

    public String toYicesString(String operand) 
    {
		//return op + "(" + operand + ")"; // + "[" + seed + "]";
		return "(" + op + " " + operand + ")"; // + "[" + seed + "]";
    }
}