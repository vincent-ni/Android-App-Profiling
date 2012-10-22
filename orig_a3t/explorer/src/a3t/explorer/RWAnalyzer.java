package a3t.explorer;

import java.util.Set;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

public class RWAnalyzer
{
	/*
	  if i^{th} event reads what j^{th} (j < i) wrote then
	  i^{th} set in the rwSet list contains j.
	 */
	private final List<Set<RWRecord>> rwSet = new ArrayList();

	private final List<Set<RWRecord>> arrayWriteSet = new ArrayList();
	private Set<RWRecord> curRWSet;
	private Set<RWRecord> curArrayWriteSet;

	RWAnalyzer()
	{		
	}

	void iter(String iter)
	{
		curRWSet = new HashSet();
		rwSet.add(curRWSet);
		
		curArrayWriteSet = new HashSet();
		arrayWriteSet.add(curArrayWriteSet);
	}

	void rw(String str)
	{
		String[] tokens = str.split(" ");
		int writeEventId = Integer.parseInt(tokens[0]);
		int fldId = Integer.parseInt(tokens[1]);
		if(BlackListedFields.check(fldId))
			return;
		curRWSet.add(new RWRecord(writeEventId, fldId));
	}
	
	void aread(String str)
	{
		String[] tokens = str.split(" ");
		int arrayObjId = Integer.parseInt(tokens[0]);
		int index = Integer.parseInt(tokens[1]);
		RWRecord rec = new RWRecord(arrayObjId, index);		
		int count = findLastWrite(rec);
		if(count >= 0)
			curRWSet.add(new RWRecord(count, -1));
	}

	void awrite(String str)
	{
		String[] tokens = str.split(" ");
		int arrayObjId = Integer.parseInt(tokens[0]);
		int index = Integer.parseInt(tokens[1]);
		RWRecord rec = new RWRecord(arrayObjId, index);
		int count = findLastWrite(rec);
		if(count >= 0)
			curRWSet.add(new RWRecord(count, -1));
		curArrayWriteSet.add(rec);
	}

	private int findLastWrite(RWRecord rec) 
	{
		int count = arrayWriteSet.size() - 1;		
		if(arrayWriteSet.get(count).contains(rec))
			return -1;
		count--;
		for(; count >= 0; count--){
			if(arrayWriteSet.get(count).contains(rec))
				break;
		}
		return count;
	}

	void finish()
	{
	}
	
	List<Set<RWRecord>> rwSet()
	{
		return rwSet;
	}
}

