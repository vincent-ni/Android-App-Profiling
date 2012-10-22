package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class LongExpression extends Expression implements Algebraic, Bitwise
{  
    public long seed;

    public long seed()
    {
		return seed;
    }

	protected final String toYicesString(String str)
    {
		String newName = SymbolicLong.makeName();
        Expression.pc.printConstraint("(= " + newName + " " + str + ")");
        return newName;
    }

    public Expression _cmp(Expression right)
    {
		return LCMP.apply(this, right);
    }
	
    public Expression _plus(Expression e)
    {
		return LADD.apply(this, e);
    }
	
    public Expression _minus(Expression e)
    {
		return LSUB.apply(this, e);
    }
    
    public Expression _mul(Expression e)
    {
		return LMUL.apply(this, e);
    }
	
    public Expression _div(Expression e)
    {
		return LDIV.apply(this, e);
    }
    
    public Expression _rem(Expression e)
    {
		return LREM.apply(this, e);
    }
    
    public Expression _neg()
    {
		return LNEG.apply(this);
    }
    
    public Expression _or(Expression e)
    {
		return LOR.apply(this, e);
    }
	
    public Expression _and(Expression e)
    {
		return LAND.apply(this, e);
    }
    
    public Expression _xor(Expression e)
    {
		return LXOR.apply(this, e);
    }
    
    public Expression _shiftL(Expression e)
    {
		return LSHL.apply(this, e);
    }

    public Expression _shiftR(Expression e)
    {
		return LSHR.apply(this, e);
    }
    
    public Expression _shiftUR(Expression e)
    {
		return LUSHR.apply(this, e);
    }
	
    public Expression _cast(int type)
    {
		if(type == Types.INT)
			return L2I.apply(this);
		if(type == Types.FLOAT)
			return L2F.apply(this);
		if(type == Types.DOUBLE)
			return L2D.apply(this);
		throw new RuntimeException("unexpected type " + type);
    }
	
    public static final BinaryOperator LADD  = Operations.v.ladd();
    public static final BinaryOperator LSUB  = Operations.v.lsub();
    public static final BinaryOperator LMUL  = Operations.v.lmul();
    public static final BinaryOperator LDIV  = Operations.v.ldiv();
    public static final BinaryOperator LREM  = Operations.v.lrem();
    public static final UnaryOperator  LNEG  = Operations.v.lneg();
    public static final BinaryOperator LOR   = Operations.v.lor();
    public static final BinaryOperator LAND  = Operations.v.land(); 
    public static final BinaryOperator LXOR  = Operations.v.lxor();
    public static final BinaryOperator LSHR  = Operations.v.lshr();
    public static final BinaryOperator LSHL  = Operations.v.lshl();
    public static final BinaryOperator LUSHR = Operations.v.lushr();
    public static final BinaryOperator LCMP  = Operations.v.lcmp();

    public static final UnaryOperator L2I = Operations.v.l2i();
    public static final UnaryOperator L2F = Operations.v.l2f();
    public static final UnaryOperator L2D = Operations.v.l2d();
}





