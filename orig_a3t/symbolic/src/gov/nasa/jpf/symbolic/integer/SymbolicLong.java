package gov.nasa.jpf.symbolic.integer;

public class SymbolicLong extends LongExpression
{
    private static int count = 0;
    private String name;

    public SymbolicLong(String name, long seed)
    {
		this.seed = seed;
		this.name = name == null ? ("$L$"+count++) : ("$L$"+name);
    }
    
    public SymbolicLong(long seed)
    {
		this(null, seed);
    }

	static String makeName()
    {
		return "$L$"+count++;
    }

    public String toString() 
    {
		return name + "[" + seed + "]";
    }
    
    public String toYicesString()
    {
		return name;
    }
	
}