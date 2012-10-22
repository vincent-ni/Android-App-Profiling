import java.io.*;
import java.util.*;

public class FieldsToMonitor 
{
	public static void main(String[] args) 
	{
		String dirName = args[0];
		File dir = new File(dirName);
		if(!dir.exists())
			throw new RuntimeException();
		
		String sigsFileName = args[1];
		File sigsFile = new File(sigsFileName);
		if(!sigsFile.exists())
			throw new RuntimeException();

		File[] logcatFiles = dir.listFiles(new FileFilter() {
				public boolean accept(File f) {
					return f.getName().startsWith("logcatout.");
				}
			});
		
		Set<Integer> fieldIds = new HashSet();
		for(File f : logcatFiles) {
			try {
				BufferedReader in = new BufferedReader(new FileReader(f));
				String s;
				while ((s = in.readLine()) != null) {
					s = s.trim();
					if(s.startsWith("E/A3T_RW")) {
						int i = s.lastIndexOf(' ');
						assert i > 0 : s;
						int fieldId = Integer.parseInt(s.substring(i+1));
						fieldIds.add(fieldId);
						//System.out.println(fieldId);
					}
				}
				in.close();
			} catch (Exception ex) {
				throw new RuntimeException(ex);
			}
		}
		
		List<String> sigsList = readFileToList(sigsFile);
		for (Integer id : fieldIds) {
			System.out.println(sigsList.get(id));
		}
	}

	private static List<String> readFileToList(File file) {
		List<String> list = new ArrayList<String>();
		try {
            BufferedReader in = new BufferedReader(new FileReader(file));
            String s;
            while ((s = in.readLine()) != null) {
                list.add(s);
            }
            in.close();
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }
		return list;
	}
}