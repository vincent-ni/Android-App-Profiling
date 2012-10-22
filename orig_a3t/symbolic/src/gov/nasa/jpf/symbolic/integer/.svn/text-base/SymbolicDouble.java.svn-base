package gov.nasa.jpf.symbolic.integer;

public class SymbolicDouble extends DoubleExpression
{
    private static int count = 0;
    private String name;

    public SymbolicDouble(String name, double seed)
    {
		this.seed = seed;
		this.name = name == null ? ("$D$"+count++) : ("$D$"+name);
    }
    
    public SymbolicDouble(double seed)
    {
		this(null, seed);
    }
    
	static String makeName()
    {
		return "$D$"+count++;
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