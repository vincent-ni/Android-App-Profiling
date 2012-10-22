package gov.nasa.jpf.symbolic.integer;

//import android.util.Slog;
import edu.gatech.symbolic.Mylog;

public class PathCondition
{
    private boolean inited = false;
    private int printST  = 8;

    public PathCondition()
    {	
    }

	
    public void assumeDet(Expression e)
    {
		printConstraint(e, true, true);
    }

    public void assumeTru(Expression e)
    {
		printConstraint(e, true, false);
    }

	public void assumeFls(Expression e)
	{
		printConstraint(e, false, false);  // TODO: negate
	}

    private void printConstraint(Expression e, boolean flag, boolean det)
    {
		//Mylog.e("A3T_DEBUG", "e = " + e + " flag = " + flag + " det = " + det);

		//printST();
		if(!flag){
			if(e instanceof BooleanExpression){
				e = BooleanExpression.NEGATION.apply(e);
			}
			else 
				assert false;
			//str = "BNOT("+str+")";
			//str = "(not " + str + ")";
		}
		String str = e.toYicesString();
		if(det)
			str = "*"+str;
		Mylog.e("A3T_PC", str);
    }

	public void printConstraint(String constraint)
	{
		Mylog.e("A3T_PC", "*"+constraint);
	}

	/*
    public void recordSeed(long l, String name)
    {
		if(!inited)
			init();
		printer.println("(assert+ (= " + name + " " + l + "))"); 
    }
	*/
    
    private void printST()
    {
		StackTraceElement[] elems = new Throwable().getStackTrace();
		int len = elems.length;
		for(int i = 0; i < printST; i++){
			StackTraceElement e = elems[i];
			Mylog.e("A3T_ST", e.toString());
		}	
    }
}
