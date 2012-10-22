package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;
import java.util.*;

public class IntegerConstant extends IntegerExpression implements Constant
{
    protected IntegerConstant(int seed)
    {
		this.seed = seed;
    }
    
    public String toString () 
    {
		return String.valueOf(seed);
    }
    
    public String toYicesString()
    {
		return Operations.v.intConstant(seed);
    }
    
    public static IntegerConstant get(int c)
    {
		return cache.get(c);
    }
    
    public int seed()
    {
		return seed;
    }

	public Expression _eq(Expression e)
	{
		if(e instanceof BooleanExpression)
			return ((BooleanExpression) e)._eq(this);
		else 
			return super._eq(e);
	}
    
	public Expression _ne(Expression e)
	{
		if(e instanceof BooleanExpression)
			return ((BooleanExpression) e)._ne(this);
		else 
			return super._ne(e);
	}

    private static LRUCacheInteger cache = new LRUCacheInteger();
}

class LRUCacheInteger extends LinkedHashMap<Integer,IntegerConstant>
{
    private final int MAX_SIZE = 50;
    private final IntegerConstant ZERO = new IntegerConstant(0);
    private final IntegerConstant ONE = new IntegerConstant(1);
    
    protected boolean removeEldestEntry(Map.Entry eldest)
    {
		return size() > MAX_SIZE;
    }
    
    public IntegerConstant get(int c)
    {
		if(c == 0) return ZERO;
		if(c == 1) return ONE;
		return get(Integer.valueOf(c));
    }
    
    public IntegerConstant get(Integer num)
    {
		IntegerConstant constant = super.get(num);
		if(constant == null){
			constant = new IntegerConstant(num.intValue());
			this.put(num, constant);
		}
		return constant;
    }
}
