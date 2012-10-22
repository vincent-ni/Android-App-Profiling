package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.FloatExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class UpdatedFloatArrayInternal extends UpdatedArrayInternal
{
	public UpdatedFloatArrayInternal(ArrayInternal oldArray, IntegerExpression index, Expression value)
	{
		super(oldArray, index, value);
	}
	
	public String toYicesString()
	{
		String str = super.toYicesString();
		String newName = SymbolicFloatArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
	
	public Expression get(Expression index)
	{
		return new FloatArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedFloatArrayInternal(this, (IntegerExpression) index, (FloatExpression) value);
	}
}