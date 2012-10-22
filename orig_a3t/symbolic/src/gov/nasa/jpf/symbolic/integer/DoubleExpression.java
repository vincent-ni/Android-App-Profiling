package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class DoubleExpression extends Expression implements CMP, Algebraic
{  
    public double seed;

    public double seed()
    {
		return seed;
    }

	protected final String toYicesString(String str)
    {
		String newName = SymbolicDouble.makeName();
        Expression.pc.printConstraint("(= " + newName + " " + str + ")");
        return newName;
    }

    public Expression _cmpl(Expression right)
    {
		return DCMPL.apply(this, right);
    }

    public Expression _cmpg(Expression right)
    {
	return DCMPG.apply(this, right);
    }    

    public Expression _plus(Expression e)
    {
	return DADD.apply(this, e);
    }

    public Expression _minus(Expression e)
    {
	return DSUB.apply(this, e);
    }
    
    public Expression _mul(Expression e)
    {
	return DMUL.apply(this, e);
    }

    public Expression _div(Expression e)
    {
	return DDIV.apply(this, e);
    }
    
    public Expression _rem(Expression e)
    {
	return DREM.apply(this, e);
    }
    
    public Expression _neg()
    {
	return DNEG.apply(this);
    }
    
    public Expression _cast(int type)
    {
	if(type == Types.INT)
	    return D2I.apply(this);
	if(type == Types.LONG)
	    return D2L.apply(this);
	if(type == Types.FLOAT)
	    return D2F.apply(this);
	throw new RuntimeException("unexpected type " + type);
    }

    public static final BinaryOperator DADD  = Operations.v.dadd();
    public static final BinaryOperator DSUB  = Operations.v.dsub();
    public static final BinaryOperator DMUL  = Operations.v.dmul();
    public static final BinaryOperator DDIV  = Operations.v.ddiv();
    public static final BinaryOperator DREM  = Operations.v.drem();
    public static final BinaryOperator DCMPL = Operations.v.dcmpl();
    public static final BinaryOperator DCMPG = Operations.v.dcmpg();
    public static final UnaryOperator  DNEG  = Operations.v.dneg();
    
    public static final UnaryOperator D2I = Operations.v.d2i();
    public static final UnaryOperator D2L = Operations.v.d2l();
    public static final UnaryOperator D2F = Operations.v.d2f();
    
}





