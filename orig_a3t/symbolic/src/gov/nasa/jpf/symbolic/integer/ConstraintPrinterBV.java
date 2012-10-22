package gov.nasa.jpf.symbolic.integer;

public class ConstraintPrinterBV extends ConstraintPrinter
{
    public String intConstant(int i)
    { 
	if(i < 0){
	    i = Integer.MAX_VALUE + (i + 1);
	    return "(concat bit1 bv"+i+"[31])";
	}
	else
	    return "bv"+i+"[32]";
    }
    
    public String longConstant(long l)
    {
	if(l < 0){
	    l = Long.MAX_VALUE + (l + 1);
	    return "(concat bit1 bv"+l+"[63])";
	}
	else
	    return "bv"+l+"[64]"; 
    }
    
    public String floatConstant(float f){ return String.valueOf(f); }
    
    public String doubleConstant(double d){ return String.valueOf(d); }
    
    public String lt(){ return "bvslt"; }
    
    public String le(){ return "bvsle"; }
    
    public String gt(){ return "bvsgt"; }
    
    public String ge(){ return "bvsge"; }
    
    public String iadd(){ return "bvadd"; }
    
    public String isub(){ return "bvsub"; }
    
    public String imul(){ return "bvmul"; }
    
    public String idiv(){ return "bvsdiv"; }
    
    public String irem(){ return "bvsrem"; }

    public String ineg(){ return "bvneg"; }

    public String ior(){ return "bvor"; }
    
    public String iand(){ return "bvand"; }
    
    public String ixor(){ return "bvxor"; }
    
    public String ishr(){ return "bvashr"; }  //arithmetic shift right

    public String ishl(){ return "bvshl"; }
    
    public String iushr(){ return "bvlshr"; } //logical shift right
    
    public String i2s(){ return "i2s"; }
    
    public String i2b(){ return "i2b"; }
    
    public String i2c(){ return "i2c"; }
    
    public String i2l(){ return "i2l"; }
    
    public String i2d(){ return "i2d"; }
    
    public String i2f(){ return "i2f"; }

    public String ladd(){ return "bvadd"; }
    
    public String lsub(){ return "bvsub"; }

    public String lmul(){ return "bvmul"; }
    
    public String ldiv(){ return "bvsdiv"; }
    
    public String lrem(){ return "bvsrem"; }

    public String lneg(){ return "bvneg"; }

    public String lor(){ return "bvor"; }
    
    public String land(){ return "bvand"; }
    
    public String lxor(){ return "bvxor"; }
    
    public String lshr(){ return "bvashr"; }

    public String lshl(){ return "bvshl"; }
    
    public String lushr(){ return "bvlshr"; }
    
    public String l2i(){ return "l2i"; }
    
    public String l2f(){ return "l2f"; }
    
    public String l2d(){ return "l2d"; }
}