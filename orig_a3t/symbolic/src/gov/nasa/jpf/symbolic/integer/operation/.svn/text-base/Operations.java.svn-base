package gov.nasa.jpf.symbolic.integer.operation;

import gov.nasa.jpf.symbolic.integer.*;

public class Operations
{
    static{
		//String klassName = System.getProperty("pc.printer", "gov.nasa.jpf.symbolic.integer.operation.OperationsBV");
		//String klassName = System.getProperty("pc.printer", "gov.nasa.jpf.symbolic.integer.operation.OperationsCoral");
		String klassName = System.getProperty("pc.printer", "gov.nasa.jpf.symbolic.integer.operation.Operations");
		try{
			v = (Operations) Class.forName(klassName).newInstance();
		}
		catch(ClassNotFoundException e){
			throw new Error(e);
		}
		catch(InstantiationException e){
			throw new Error(e);
		}
		catch(IllegalAccessException e){
			throw new Error(e);
		}
    }
    
    public static final Operations v;

	public String array_get(String array, String index)
	{
		return "(select "+array+" "+index+")";
	}

	public String array_set(String array, String index, String value)
	{
		return "(store "+array+" "+index+" "+value+")";
	}

    public String intConstant(int v)      { return (v >= 0) ? String.valueOf(v) : "(- "+String.valueOf(-v)+")"; }    
    public String longConstant(long v)    { return (v >= 0) ? String.valueOf(v) : "(- "+String.valueOf(-v)+")"; }
    public String floatConstant(float v)  { return (v >= 0) ? String.valueOf(v) : "(- "+String.valueOf(-v)+")"; }
    public String doubleConstant(double v){ return (v >= 0) ? String.valueOf(v) : "(- "+String.valueOf(-v)+")"; }
	public String refConstant(Object o)   { return (o == null ? "null" : o.getClass().getName())+"@"+System.identityHashCode(o); }

	public BinaryOperator conjunct(){ return new BooleanBinaryOperator("and"); }
	public UnaryOperator negation(){ return new NEGATION("not"); }

    public BinaryOperator icmpeq(){ return new BooleanBinaryOperator("=");        }
    public BinaryOperator icmpne(){ return new NegatedBooleanBinaryOperator("="); }
    public BinaryOperator icmplt(){ return new BooleanBinaryOperator("<");        }
    public BinaryOperator icmple(){ return new BooleanBinaryOperator("<=");       }
    public BinaryOperator icmpgt(){ return new BooleanBinaryOperator(">");        }
    public BinaryOperator icmpge(){ return new BooleanBinaryOperator(">=");       }

    public BinaryOperator iadd() { return new IntegerBinaryOperator("+");     }
    public BinaryOperator isub() { return new IntegerBinaryOperator("-");     }
    public BinaryOperator imul() { return new IntegerBinaryOperator("*");     }
    public BinaryOperator idiv() { return new IntegerBinaryOperator("div");   }
    public BinaryOperator irem() { return new IntegerBinaryOperator("rem");   }
    public BinaryOperator ior()  { return new IntegerBinaryOperator("ior");   }
    public BinaryOperator iand() { return new IntegerBinaryOperator("iand");  }
    public BinaryOperator ixor() { return new IntegerBinaryOperator("ixor");  }
    public BinaryOperator ishr() { return new IntegerBinaryOperator("ishr");  }
    public BinaryOperator ishl() { return new IntegerBinaryOperator("ishl");  }
    public BinaryOperator iushr(){ return new IntegerBinaryOperator("iushr"); }

    public UnaryOperator ineg(){ return new IntegerUnaryOperator("~");       }    
    public UnaryOperator i2s() { return new IntegerUnaryOperator("i2s");     }
    public UnaryOperator i2b() { return new IntegerUnaryOperator("i2b");     }
    public UnaryOperator i2c() { return new IntegerUnaryOperator("i2c");     }
    public UnaryOperator i2l() { return new LongUnaryOperator("i2l");     }
    public UnaryOperator i2d() { return new DoubleUnaryOperator("to_real"); }
    public UnaryOperator i2f() { return new FloatUnaryOperator("to_real"); }

    public BinaryOperator ladd() { return new LongBinaryOperator("+");       }
    public BinaryOperator lsub() { return new LongBinaryOperator("-");       }
    public BinaryOperator lmul() { return new LongBinaryOperator("*");       }
    public BinaryOperator ldiv() { return new LongBinaryOperator("div");     }
    public BinaryOperator lrem() { return new LongBinaryOperator("rem");     }
    public BinaryOperator lor()  { return new LongBinaryOperator("ior");     }
    public BinaryOperator land() { return new LongBinaryOperator("iand");    }
    public BinaryOperator lxor() { return new LongBinaryOperator("ixor");    }
    public BinaryOperator lshr() { return new LongBinaryOperator("ishr");    }
    public BinaryOperator lshl() { return new LongBinaryOperator("ishl");    }
    public BinaryOperator lushr(){ return new LongBinaryOperator("iushr");   }
    public BinaryOperator lcmp() { return new IntegerBinaryOperator("lcmp"); }

    public UnaryOperator lneg(){ return new LongUnaryOperator("~");      }    
    public UnaryOperator l2i() { return new IntegerUnaryOperator("l2i"); }
    public UnaryOperator l2f() { return new FloatUnaryOperator("l2f");   }
    public UnaryOperator l2d() { return new DoubleUnaryOperator("l2d");  }

    public BinaryOperator fadd() { return new FloatBinaryOperator("+");       }
    public BinaryOperator fsub() { return new FloatBinaryOperator("-");       }
    public BinaryOperator fmul() { return new FloatBinaryOperator("*");       }
    public BinaryOperator fdiv() { return new FloatBinaryOperator("div");     }
    public BinaryOperator frem() { return new FloatBinaryOperator("rem");     }
    public BinaryOperator fcmpl(){ return new IntegerBinaryOperator("fcmpl"); }
    public BinaryOperator fcmpg(){ return new IntegerBinaryOperator("fcmpg"); }

    public UnaryOperator fneg(){ return new FloatUnaryOperator("fneg"); }
    public UnaryOperator f2i() { return new IntegerUnaryOperator("my_to_int"); }
    public UnaryOperator f2l() { return new LongUnaryOperator("my_to_int"); }
    public UnaryOperator f2d() { return new DoubleUnaryOperator("f2d"); }
    
    public BinaryOperator dadd() { return new DoubleBinaryOperator("dadd");   }
    public BinaryOperator dsub() { return new DoubleBinaryOperator("dsub");   }
    public BinaryOperator dmul() { return new DoubleBinaryOperator("dmul");   }
    public BinaryOperator ddiv() { return new DoubleBinaryOperator("ddiv");   }
    public BinaryOperator drem() { return new DoubleBinaryOperator("drem");   }
    public BinaryOperator dcmpl(){ return new IntegerBinaryOperator("dcmpl"); }
    public BinaryOperator dcmpg(){ return new IntegerBinaryOperator("dcmpg"); }

    public UnaryOperator dneg(){ return new DoubleUnaryOperator("dneg"); }
    public UnaryOperator d2i(){ return new IntegerUnaryOperator("my_to_int"); }
    public UnaryOperator d2l(){ return new IntegerUnaryOperator("my_to_int"); }
    public UnaryOperator d2f(){ return new FloatUnaryOperator("d2f"); }
	
	public BinaryOperator acmpeq(){ return new BooleanBinaryOperator("=");        }
    public BinaryOperator acmpne(){ return new NegatedBooleanBinaryOperator("="); }

	public BinaryOperator req(){ return new BooleanBinaryOperator("=");        }
    public BinaryOperator rne(){ return new NegatedBooleanBinaryOperator("="); }
}