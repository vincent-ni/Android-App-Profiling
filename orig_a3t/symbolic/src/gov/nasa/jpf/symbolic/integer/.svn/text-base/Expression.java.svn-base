package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.SException;

public abstract class Expression
{
    public static final PathCondition pc = new PathCondition();
    
    protected String exprString;
    
    public abstract String toYicesString();

    //this should be faster at the cost of memory
    public String exprString()
    {
	if(exprString == null)
	    exprString = toYicesString();
	return exprString;
    }
}
