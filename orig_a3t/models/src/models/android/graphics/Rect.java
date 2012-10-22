package models.android.graphics;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;
import gov.nasa.jpf.symbolic.integer.BooleanExpression;
import gov.nasa.jpf.symbolic.integer.IntegerConstant;
import gov.nasa.jpf.symbolic.integer.SymbolicInteger;
import gov.nasa.jpf.symbolic.integer.Types;
import android.util.Slog;

public class Rect
{
	public static Expression contains__II__Z(java.lang.Object obj, Expression obj$sym, 
											 int x, Expression x$sym,
											 int y, Expression y$sym)
	{
		if(x$sym == null && y$sym == null)
			return null;
		
		android.graphics.Rect rect = (android.graphics.Rect) obj;
		
		Slog.e("A3T_DEBUG", "left = " + rect.left + " right = " + rect.right +
			   "top = " + rect.top + " bottom = " + rect.bottom);
		Slog.e("A3T_DEBUG", "x = " + x + " y = " + y);

		Expression xExpr = null;
		Expression yExpr = null;
		if(x$sym != null){
			IntegerExpression x$symb = (IntegerExpression) x$sym;
			xExpr = ((BooleanExpression) x$symb._lt(IntegerConstant.get(rect.right)))._conjunct(x$symb._ge(IntegerConstant.get(rect.left)));     
		}
		if(y$sym != null){
			IntegerExpression y$symb = (IntegerExpression) y$sym;
			yExpr = ((BooleanExpression) y$symb._lt(IntegerConstant.get(rect.bottom)))._conjunct(y$symb._ge(IntegerConstant.get(rect.top)));
		}
		if(xExpr == null)
		    return yExpr;
		if(yExpr == null)
		    return xExpr;
		return ((BooleanExpression) xExpr)._conjunct(yExpr);
	}
}
