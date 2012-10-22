package a3t.explorer;

import java.util.Set;
import java.util.HashSet;
import java.util.List;

public class ReadOnlyLastTapDetector
{
	/*
	  if current event writes to field f, then writeSet
	  contains the id of the field f.
	 */
	private Set<Integer> writeSet;

	ReadOnlyLastTapDetector()
	{
	}

	boolean readOnlyLastTap()
	{
		return writeSet.isEmpty();
	}

	void iter()
	{
		writeSet = new HashSet();
	}

	void process(String line)
	{
		int fieldSigId = Integer.parseInt(line);
		//System.out.println("write " + fldSigId);

		if(BlackListedFields.check(fieldSigId))
			return;

		writeSet.add(fieldSigId);
	}

	void processArray(String line)
	{
		writeSet.add(-1);
	}

    Set<Integer> writeSet()
    {
        return writeSet;
    }

	void finish()
	{
	}	
}
