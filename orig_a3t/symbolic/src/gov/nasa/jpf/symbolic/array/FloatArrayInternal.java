package gov.nasa.jpf.symbolic.array;

import gov.nasa.jpf.symbolic.integer.IntegerConstant;
import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.FloatExpression;
import gov.nasa.jpf.symbolic.integer.IntegerExpression;

class FloatArrayInternal extends ArrayInternal
{
	private int[] symIndices;

	FloatArrayInternal()
	{
		super();
	}

	FloatArrayInternal(String name)
	{
		super(name);
	}

	FloatArrayInternal(String name, int[] symIndices)
	{
		super(name);
		this.symIndices = symIndices;
	}

	public Expression get(Expression index)
	{
		if(symIndices != null){
			if(index instanceof IntegerConstant){
				int ind = ((IntegerConstant) index).seed();
				for(int i = 0; i < symIndices.length; i++){
					if(symIndices[i] == ind)
						return new FloatArrayElem(this, (IntegerExpression) index);
				} 
			}
		}
		return new FloatArrayElem(this, (IntegerExpression) index);
	}
	
	public ArrayInternal set(Expression index, Expression value)
	{
		return new UpdatedFloatArrayInternal(this, (IntegerExpression) index, (FloatExpression) value);
	}
}