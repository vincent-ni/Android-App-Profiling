package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

abstract class ArrayInternal extends Expression
{
	protected Expression length;

	ArrayInternal()
	{
		super();
	}

	ArrayInternal(String name)
	{
		this.exprString = name;
	}

	public Expression _alen()
	{
		return this.length;
	}	

	public String toYicesString()
	{
		return exprString;
	}

	public abstract Expression get(Expression index);
	
	public abstract ArrayInternal set(Expression index, Expression value);
	
}