package a3t.explorer;

import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.io.File;

public class FuzzedMonkeyScript extends MonkeyScript
{
	private static final Map<String,String> symbolPrefixMap = new HashMap();

	private Z3Model model;
	private MonkeyScript seedScript;
	private int tapEventCount = 0;

	static {
		symbolPrefixMap.put(EVENT_KEYWORD_TAP, 
							"$!F$deliverPointerEvent$android$view$MotionEvent$0$");
	}

	public FuzzedMonkeyScript(File seedScript, Z3Model model)
	{
		this.model = model;
		this.seedScript = new ElementaryMonkeyScript(seedScript);
		fuzz();
	}

	private void fuzz()
	{
		copyCommentsFrom(seedScript);

		for(Iterator<Event> it = seedScript.eventIterator(); it.hasNext();) {
			Event ev = it.next();
			addEvent(fuzzEvent((Event) ev));
		}
	}

	private Event fuzzEvent(Event event)
	{
		if(event instanceof TapEvent) {
			try {
				TapEvent ev = (TapEvent) event;

				String varName = symbolPrefixMap.get(EVENT_KEYWORD_TAP);
				varName += (tapEventCount++ * 2);

				float x, y;
				String alias = (String) model.get(varName);
				//System.out.println("varName " + varName + " alias " + alias);
				if(alias != null) {
					Z3Model.Array array = (Z3Model.Array) model.get(alias);
					x = ((Number) array.get(0)).floatValue();
					y = ((Number) array.get(1)).floatValue();
				}
				else {
					x = ev.x();
					y = ev.y();
				}
				return new TapEvent(x, y);
            } catch (NumberFormatException e) {
				throw new Error(e);
            }
		}
		throw new RuntimeException("only tap event supported. " + event.getClass().getName());
	}

}
