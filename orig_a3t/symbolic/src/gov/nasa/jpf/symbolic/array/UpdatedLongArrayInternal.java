package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.LongExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class UpdatedLongArrayInternal extends UpdatedArrayInternal
{
	public UpdatedLongArrayInternal(ArrayInternal oldArray, IntegerExpression index, Expression value)
	{
		super(oldArray, index, value);
	}
	
	public String toYicesString()
	{
		String str = super.toYicesString();
		String newName = SymbolicLongArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
	
	public Expression get(Expression index)
	{
		return new LongArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedLongArrayInternal(this, (IntegerExpression) index, (LongExpression) value);
	}
}