package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public class RefConstant extends RefExpression implements Constant
{
	public static RefConstant get(Object obj)
	{
		if(obj == null)
			return NULL;
		return new RefConstant(obj);
	}

	public RefConstant(Object obj)
	{
		super(obj);
	}

    public String toString () 
    {
		int hash = System.identityHashCode(seed);
		return (seed == null ? "null" : seed.getClass().getName())+"@"+hash;
    }
    
    public String toYicesString()
    {
		return Operations.v.refConstant(seed);
    }
	
	private static final RefConstant NULL = new RefConstant(null);
}