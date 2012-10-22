package gov.nasa.jpf.symbolic.integer;

public class SymbolicFloat extends FloatExpression
{
    private static int count = 0;
    private String name;

    public SymbolicFloat(String name, float seed)
    {
		this.seed = seed;
		this.name = name == null ? ("$F$"+count++) : ("$F$"+name);
    }
    
    public SymbolicFloat(float seed)
    {
		this(null, seed);
    }

	static String makeName()
    {
		return "$F$"+count++;
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