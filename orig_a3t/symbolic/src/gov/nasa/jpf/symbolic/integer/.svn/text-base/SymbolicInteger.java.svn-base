package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.SException;

public class SymbolicInteger extends IntegerExpression
{
    private static int count = 0;

    //public SymbolicInteger()
    //{
    //    this("int",null);
    //}

    public SymbolicInteger(int type, int seed)
    {
		this(type, null, seed);
    }

    public SymbolicInteger(int type, String name, int seed) 
    {
		super();
		
		switch(type){
		case Types.CHAR:
			exprString = makeName("$C$", name);
			break;
		case Types.BYTE:
			exprString = makeName("$B$", name);
			break;
		case Types.SHORT:
			exprString = makeName("$S$", name);
			break;
		case Types.INT:
			exprString = makeName("$I$", name);
			break;
		case Types.BOOLEAN:
			exprString = makeName("$Z$", name);
			break;
		default:
			throw new RuntimeException("unexpected type " + type + " " + name);
		}
		this.seed = seed;
    }
	
    private String makeName(String type, String name)
    {
		if(name == null)
			return type+count++;
		else
			return type+name;
    }

    static String makeName()
    {
		return "$I$"+count++;
    }
	
    public String toString() 
    {
		return exprString + "[" + seed + "]";
    }
    
    public String toYicesString()
    {
		return exprString;
    }
    
}
