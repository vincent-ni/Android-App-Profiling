package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;

public abstract class Array extends Expression
{
	protected ArrayInternal array;

	protected Array(ArrayInternal a)
	{
		this.array = a;
	}

	public Array()
	{
		super();
	}
	
	public Expression _aset(Expression index, Expression value)
	{
		this.array = this.array.set(index, value);
		return this;
	}
	
	public Expression _aget(Expression index)
	{
		return this.array.get(index);
	}

	public Expression _alen()
	{
		return array._alen();
	}
	
	public String toString()
	{
		return this.array.toYicesString();
	}

	public String exprString()
	{
		return this.array.exprString();
	}
	
	public final String toYicesString()
	{
		throw new RuntimeException();
	}
}