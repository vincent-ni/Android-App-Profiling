package a3t.explorer;

import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.File;

public class ElementaryMonkeyScript extends MonkeyScript
{
	public ElementaryMonkeyScript(File script)
	{
		try{
			BufferedReader reader = new BufferedReader(new FileReader(script));
			String line = reader.readLine();
			while(line != null){
				line = line.trim();
				if(line.startsWith("//")) {
					line = line.replace("//","").trim();
					if(line.startsWith("Repeat of "))
						userWait += defaultUserWait;
					addComment(line);
				}
				else {
					Event ev = processLine(line);
					addEvent(ev);
				}
				line = reader.readLine();
			}
			reader.close();	
		}catch(IOException e){
			throw new Error(e);
		}
	}

    private Event processLine(String line) 
	{
        int index1 = line.indexOf('(');
        int index2 = line.indexOf(')');

        if (index1 < 0 || index2 < 0) {
			assert false;
        }

        String[] args = line.substring(index1 + 1, index2).split(",");

        for (int i = 0; i < args.length; i++) {
            args[i] = args[i].trim();
        }

		Event ev = createEvent(line, args);
		return ev;
    }

	private Event createEvent(String line, String[] args)
	{
		if(line.startsWith(EVENT_KEYWORD_TAP)){
			try{
				float x = Float.parseFloat(args[0]);
				float y = Float.parseFloat(args[1]);
				TapEvent ev = new TapEvent(x, y);
				return ev;
            } catch (NumberFormatException e) {
				throw new Error(e);
            }
		}
		throw new RuntimeException("only tap events are supported. " + line);
	}
	
}

