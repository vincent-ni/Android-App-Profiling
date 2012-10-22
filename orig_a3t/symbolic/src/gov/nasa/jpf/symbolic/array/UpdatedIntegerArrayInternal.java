package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

public class UpdatedIntegerArrayInternal extends UpdatedArrayInternal
{
	public UpdatedIntegerArrayInternal(ArrayInternal oldArray, IntegerExpression index, Expression value)
	{
		super(oldArray, index, value);
	}
	
	public String toYicesString()
	{
		String str = super.toYicesString();
		String newName = SymbolicIntegerArray.makeName();
		Expression.pc.printConstraint("(= " + newName + " " + str + ")");
		return newName;
	}
	
	public Expression get(Expression index)
	{
		return new IntegerArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedIntegerArrayInternal(this, (IntegerExpression) index, (IntegerExpression) value);
	}
}