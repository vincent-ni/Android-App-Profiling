package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;
import java.util.*;

public final class LongConstant extends LongExpression implements Constant
{
    protected LongConstant(long seed)
    {
	this.seed = seed;
    }
    
    public String toString () 
    {
	return seed+"l";
    }
    
    public String toYicesString()
    {
	return Operations.v.longConstant(seed);
    }
    
    public static LongConstant get(long c)
    {
	return cache.get(c);
    }

    public long seed()
    {
	return seed;
    }
    
    private static LRUCacheLong cache = new LRUCacheLong();

}

class LRUCacheLong extends LinkedHashMap<Long,LongConstant>
{
    private final int MAX_SIZE = 50;
    private final LongConstant ZERO = new LongConstant(0L);
    private final LongConstant ONE = new LongConstant(1L);
    
    protected boolean removeEldestEntry(Map.Entry eldest)
    {
	return size() > MAX_SIZE;
    }
    
    public LongConstant get(long c)
    {
	if(c == 0L) return ZERO;
	if(c == 1L) return ONE;
	return get(Long.valueOf(c));
    }
    
    public LongConstant get(Long num)
    {
	LongConstant constant = super.get(num);
	if(constant == null){
	    constant = new LongConstant(num.longValue());
	    this.put(num, constant);
	}
	return constant;
    }
}
