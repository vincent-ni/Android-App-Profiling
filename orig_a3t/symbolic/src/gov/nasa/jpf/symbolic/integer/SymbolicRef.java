package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public class SymbolicRef extends RefExpression
{
    private static int count = 0;
    private String name;

	public SymbolicRef(String name, Object seed)
	{
		super(seed);
		this.name = name == null ? ("$R$"+count++) : ("$R$"+name);
	}
	
	public SymbolicRef(Object seed)
	{
		this(null, seed);
	}
	
	public String toYicesString()
	{
		return name;
	}
}