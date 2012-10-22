package gov.nasa.jpf.symbolic.integer;

import gov.nasa.jpf.symbolic.integer.operation.Operations;

public abstract class FloatExpression extends Expression implements CMP, Algebraic
{  
    public float seed;

    public float seed()
    {
		return seed;
    }

	protected final String toYicesString(String str)
    {
		String newName = SymbolicFloat.makeName();
        Expression.pc.printConstraint("(= " + newName + " " + str + ")");
        return newName;
    }

	private Expression _realeq(Expression e)
	{
		return REQ.apply(this, e);
	}

	private Expression _realne(Expression e)
	{
		return RNE.apply(this, e);
	}

    public Expression _cmpl(Expression right)
    {
		return FCMPL.apply(this, right);
    }

    public Expression _cmpg(Expression right)
    {
		return FCMPG.apply(this, right);
    }

    public Expression _plus(Expression e)
    {
		return FADD.apply(this, e);
    }

    public Expression _minus(Expression e)
    {
		return FSUB.apply(this, e);
    }
    
    public Expression _mul(Expression e)
    {
		return FMUL.apply(this, e);
    }

    public Expression _div(Expression e)
    {
		return FDIV.apply(this, e);
    }
    
    public Expression _rem(Expression e)
    {
		return FREM.apply(this, e);
    }
    
    public Expression _neg()
    {
		return FNEG.apply(this);
    }
        
    public Expression _cast(int type)
    {
		if(type == Types.INT){
			if(F2I == null){
				SymbolicInteger i = new SymbolicInteger(Types.INT, (int) seed);
				FloatExpression e = (FloatExpression) IntegerExpression.I2F.apply(i);
				Expression.pc.assumeDet(e._realeq(this));
				return i;
			}
			else{
				return F2I.apply(this);
			}
		}
		if(type == Types.LONG){
			if(F2L == null){
				SymbolicLong i = new SymbolicLong((long) seed);
				FloatExpression e = (FloatExpression) LongExpression.L2F.apply(i);
				Expression.pc.assumeDet(e._realeq(this));
				return i;
			}
			else{
				return F2L.apply(this);
			}
		}
		if(type == Types.DOUBLE)
			return F2D.apply(this);
		throw new RuntimeException("unexpected type " + type);
    }

    public static final BinaryOperator FADD  = Operations.v.fadd();
    public static final BinaryOperator FSUB  = Operations.v.fsub();
    public static final BinaryOperator FMUL  = Operations.v.fmul();
    public static final BinaryOperator FDIV  = Operations.v.fdiv();
    public static final BinaryOperator FREM  = Operations.v.frem();
    public static final BinaryOperator FCMPL = Operations.v.fcmpl();
    public static final BinaryOperator FCMPG = Operations.v.fcmpg();
    public static final UnaryOperator  FNEG  = Operations.v.fneg();
    
    public static final UnaryOperator F2I = Operations.v.f2i();
    public static final UnaryOperator F2L = Operations.v.f2l();
    public static final UnaryOperator F2D = Operations.v.f2d();

	public static final BinaryOperator REQ = Operations.v.req();
	public static final BinaryOperator RNE = Operations.v.rne();
}





