package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public final class DoubleConstant extends DoubleExpression implements Constant
{
    protected DoubleConstant(double seed)
    {
	this.seed = seed;
    }
    
    public String toString () 
    {
	return seed + "d";
    }
    
    public String toYicesString()
    {
	return Operations.v.doubleConstant(seed);
    }
    
    public static DoubleConstant get(double c)
    {
	return new DoubleConstant(c);
    }

    public double seed()
    {
	return seed;
    }
}

