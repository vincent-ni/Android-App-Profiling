package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class IntegerExpression extends Expression implements Equality, Algebraic, Bitwise
{  
    public int seed;

    public int seed()
    {
		return seed;
    }

	protected final String toYicesString(String str)
    {
		String newName = SymbolicInteger.makeName();
        Expression.pc.printConstraint("(= " + newName + " " + str + ")");
        return newName;
    }

    public Expression _eq(Expression e)
    {
		return ICMPEQ.apply(this, e);
    }
	
    public Expression _ne(Expression e)
    {
		return ICMPNE.apply(this, e);
    }

    public Expression _lt(Expression e)
    {
		return ICMPLT.apply(this, e);
    }
    
    public Expression _gt(Expression e)
    {
		return ICMPGT.apply(this, e);
    }
	
    public Expression _le(Expression e)
    {
		return ICMPLE.apply(this, e);
    }
	
    public Expression _ge(Expression e)
    {
		return ICMPGE.apply(this, e);
    }
	
    public Expression _plus(Expression e)
    {
		return IADD.apply(this, e);
    }
	
    public Expression _minus(Expression e)
    {
		return ISUB.apply(this, e);
    }
    
    public Expression _mul(Expression e)
    {
		return IMUL.apply(this, e);
    }
	
    public Expression _div(Expression e)
    {
		return IDIV.apply(this, e);
    }
    
    public Expression _rem(Expression e)
    {
		return IREM.apply(this, e);
    }
    
    public Expression _neg()
    {
		return INEG.apply(this);
    }
    
    public Expression _or(Expression e)
    {
		return IOR.apply(this, e);
    }

    public Expression _and(Expression e)
    {
		return IAND.apply(this, e);
    }
    
    public Expression _xor(Expression e)
    {
		return IXOR.apply(this, e);
    }

    public Expression _shiftL(Expression e)
    {
		return ISHL.apply(this, e);
    }

    public Expression _shiftR(Expression e)
    {
		return ISHR.apply(this, e);
    }
    
    public Expression _shiftUR(Expression e)
    {
		return IUSHR.apply(this, e);
    }
    
    public Expression _cast(int type)
    {
		if(type == Types.SHORT)
			return I2S.apply(this);
		if(type == Types.BYTE)
			return I2B.apply(this);
		if(type == Types.CHAR)
			return I2C.apply(this);
		if(type == Types.LONG)
			return I2L.apply(this);
		if(type == Types.FLOAT)
			return I2F.apply(this);
		if(type == Types.DOUBLE)
			return I2D.apply(this);
		throw new RuntimeException("unexpected type " + type);
    }

    
    public static final BinaryOperator ICMPEQ  = Operations.v.icmpeq();
    public static final BinaryOperator ICMPNE  = Operations.v.icmpne();
    public static final BinaryOperator ICMPLT  = Operations.v.icmplt();
    public static final BinaryOperator ICMPGT  = Operations.v.icmpgt();
    public static final BinaryOperator ICMPLE  = Operations.v.icmple();
    public static final BinaryOperator ICMPGE  = Operations.v.icmpge();

    public static final BinaryOperator IADD  = Operations.v.iadd();
    public static final BinaryOperator ISUB  = Operations.v.isub();
    public static final BinaryOperator IMUL  = Operations.v.imul();
    public static final BinaryOperator IDIV  = Operations.v.idiv();
    public static final BinaryOperator IREM  = Operations.v.irem();
    public static final UnaryOperator  INEG  = Operations.v.ineg();
    public static final BinaryOperator IOR   = Operations.v.ior();
    public static final BinaryOperator IAND  = Operations.v.iand(); 
    public static final BinaryOperator IXOR  = Operations.v.ixor();
    public static final BinaryOperator ISHR  = Operations.v.ishr();
    public static final BinaryOperator ISHL  = Operations.v.ishl();
    public static final BinaryOperator IUSHR = Operations.v.iushr();

    public static final UnaryOperator I2S = Operations.v.i2s();
    public static final UnaryOperator I2B = Operations.v.i2b();
    public static final UnaryOperator I2C = Operations.v.i2c();
    public static final UnaryOperator I2L = Operations.v.i2l();
    public static final UnaryOperator I2F = Operations.v.i2f();
    public static final UnaryOperator I2D = Operations.v.i2d();
	
}





