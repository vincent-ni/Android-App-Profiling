package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public final class FloatConstant extends FloatExpression implements Constant
{
    protected FloatConstant(float seed)
    {
	this.seed = seed;
    }
    
    public String toString () 
    {
	return seed + "f";
    }
    
    public String toYicesString()
    {
	return Operations.v.floatConstant(seed);
    }
    
    public static FloatConstant get(float c)
    {
	return new FloatConstant(c);
    }

    public float seed()
    {
	return seed;
    }
}

