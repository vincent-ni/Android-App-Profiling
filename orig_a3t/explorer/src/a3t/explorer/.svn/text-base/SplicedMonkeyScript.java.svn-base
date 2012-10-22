package a3t.explorer;

import java.util.Iterator;

public class SplicedMonkeyScript extends MonkeyScript
{
	public SplicedMonkeyScript(MonkeyScript preScript, MonkeyScript sufScript)
	{
		for(Iterator<Event> it = preScript.eventIterator(); it.hasNext();){
			Event ev = it.next();
			addEvent(ev);
		}
		
		for(Iterator<Event> it = sufScript.eventIterator(); it.hasNext();){
			Event ev = it.next();
			addEvent(ev);
		}
		
		copyCommentsFrom(preScript);
	}
}
