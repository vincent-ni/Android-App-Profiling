package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class BooleanExpression extends Expression implements Equality
{
	//to be used in models only
	public Expression _conjunct(Expression e)
	{
		return CONJUNCT.apply(this, e);
	}
	
	public Expression _eq(Expression e)
	{
		if(e instanceof IntegerConstant){
			int seed = ((IntegerConstant) e).seed;
			if(seed == 1)
				return this;
			else if(seed == 0)
				return NEGATION.apply(this);
			else
				assert false;
		}
		throw new RuntimeException("Take care");
	}
	
	public Expression _ne(Expression e)
	{
		if(e instanceof IntegerConstant){
			int seed = ((IntegerConstant) e).seed;
			if(seed == 1)
				return NEGATION.apply(this);
			else if(seed == 0)
				return this;
			else
				assert false;
		}
		throw new RuntimeException("Take care");
	}
	
	public static final BinaryOperator CONJUNCT = Operations.v.conjunct();
	public static final UnaryOperator NEGATION = Operations.v.negation();
}