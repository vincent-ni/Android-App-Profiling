package a3t.explorer;

import java.io.FileWriter;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.File;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

public class MonkeyScript
{
	protected static final String EVENT_KEYWORD_TAP = "Tap";
	protected static int defaultUserWait;

	protected List<Event> events = new ArrayList();
	protected List<String> comments = new ArrayList();
	protected int userWait = defaultUserWait;

	/* 
	   wait in seconds before and after each event
	 */
	static void setup(int wait)
	{
		defaultUserWait = wait*1000;
	}

	protected void addEvent(Event ev)
	{
		events.add(ev);
	}

	protected void addComment(String comment)
	{
		comments.add(comment);
	}

	protected void copyCommentsFrom(MonkeyScript other)
	{
		comments.addAll(other.comments);
	}

	protected Iterator<Event> eventIterator()
	{
		return events.iterator();
	}

	public void saveAs(File file) 
	{
		try{
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
			for(String c : comments) 
				writer.write("//"+c+"\n");

			for(Event event : events){
				writer.write(event.toString());
				writer.newLine();
			}
			writer.close();
		}catch(IOException e){
			throw new Error(e);
		}
	}

	public int length()
	{
		return events.size();
	}
	
	public void generate(File file) 
	{
		try{
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
						
			//write prelude
			writer.write("count = " + (1+1+events.size()) + "\n"); 
			writer.write("speed = 1000\n");
			writer.write("start data >>\n");
			writer.write("DispatchKey(0,0,0,82,0,0,0,0)\n");

			for(Event event : events){
				writer.write("UserWait("+userWait+")\n");
				writer.write(event.toString()+"\n");
			}
			writer.write("UserWait("+userWait+")\n");
			writer.close();
		}catch(IOException e){
			throw new Error(e);
		}
	}
}

abstract class Event
{
}
  
class TapEvent extends Event
{
	private float mX = -1;
	private float mY = -1;

	TapEvent(float x, float y)
	{
		this.mX = x;
		this.mY = y;
	}

	float x()
	{
		return this.mX;
	}

	float y()
	{
		return this.mY;
	}
	
	public String toString()
	{
		return MonkeyScript.EVENT_KEYWORD_TAP+"("+mX+","+mY+")";
	}
}
