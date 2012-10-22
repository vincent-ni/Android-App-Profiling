package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.DoubleExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class UpdatedDoubleArrayInternal extends UpdatedArrayInternal
{
	public UpdatedDoubleArrayInternal(ArrayInternal oldArray, IntegerExpression index, Expression value)
	{
		super(oldArray, index, value);
	}
	
	public String toYicesString()
	{
		String str = super.toYicesString();
		String newName = SymbolicDoubleArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
		
	public Expression get(Expression index)
	{
		return new DoubleArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedDoubleArrayInternal(this, (IntegerExpression) index, (DoubleExpression) value);
	}
}