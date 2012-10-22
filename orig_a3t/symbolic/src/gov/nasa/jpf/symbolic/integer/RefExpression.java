package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class RefExpression extends Expression implements Equality
{
	public Object seed; //TODO: fix this possible memory leak
	
	public RefExpression(Object seed)
	{
		this.seed = seed;
	}
	
	public Expression _eq(Expression e)
	{
		return ACMPEQ.apply(this, e);
	}
	
	public Expression _ne(Expression e)
    {
		return ACMPNE.apply(this, e);
    }

	public static final BinaryOperator ACMPEQ  = Operations.v.acmpeq();
    public static final BinaryOperator ACMPNE  = Operations.v.acmpne();
}