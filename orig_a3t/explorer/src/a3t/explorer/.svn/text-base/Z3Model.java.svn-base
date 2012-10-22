package a3t.explorer;

import java.util.Map;
import java.util.HashMap;

public class Z3Model
{
	Map<String,Object> vals = new HashMap();
	
	void put(String varName, Object value)
	{
		vals.put(varName, value);
	}
	
	public Object get(String varName)
	{
		return vals.get(varName);
	}

	public void print()
	{
		for(Map.Entry<String,Object> e : vals.entrySet()){
			System.out.println(e.getKey() + " " + e.getValue());
		}
	}

	public static class Array
	{
		private Map<Integer,Number> vals = new HashMap();
		private Number defaultVal;
		
		void put(Integer index, Number value)
		{
			vals.put(index, value);
		}
		
		public Number get(int index)
		{
			Number elem = vals.get(index);
			if(elem != null)
				return elem;
			return defaultVal;
		}
		
		void setDefaultValue(Number defaultVal)
		{
			this.defaultVal = defaultVal;
		}
		
		public String toString()
		{
			StringBuilder buf = new StringBuilder();
			for(Map.Entry<Integer,Number> e : vals.entrySet()){
				buf.append("("+e.getKey()+", " + e.getValue()+") ");
			}
			buf.append("(?, "+defaultVal+")");
			return buf.toString();
		}
	}
}