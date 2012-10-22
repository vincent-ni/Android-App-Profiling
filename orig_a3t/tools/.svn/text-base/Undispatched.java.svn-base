import java.io.*;
import java.util.*;

public class Undispatched
{
	public static String process(String dirName)
	{
		File dir = new File(dirName);
		if(!dir.exists())
			throw new RuntimeException();

		File[] syslogFiles = dir.listFiles(new FileFilter() {
				public boolean accept(File f) {
					return f.getName().startsWith("syslog.");
				}
			});

		List<Integer> ids = new ArrayList();
		for(File f : syslogFiles) {
			try {
				BufferedReader in = new BufferedReader(new FileReader(f));
				String s;
				boolean readOnly = false;
				int eventCount = 0;
				while ((s = in.readLine()) != null) {
					s = s.trim();
					if(s.startsWith("I/A3T_DISPATCH(")) {
						readOnly = false;
					} 
					else if(s.indexOf("symbolic motion event injected") >= 0) {
						if(eventCount++ % 2 == 0) {
							readOnly = true;
						}	
					}
				}
				in.close();
				if(readOnly) {
					String fname = f.getName();
					String id = fname.substring(fname.lastIndexOf('.')+1);
					ids.add(Integer.parseInt(id));
				}
			} catch (Exception ex) {
				throw new RuntimeException(ex);
			}
		}

		boolean first = true;
		StringBuilder builder = new StringBuilder();
		Collections.sort(ids);
		for(Integer id : ids) {
			if(!first)
				builder.append(",");
			else
				first = false;
			builder.append(id);
		}
		return builder.toString();		
	}
	
	public static void main(String[] args)
	{
		String dirName = args[0];
		System.out.println(process(dirName));
	}
}