import java.io.*;
import java.util.*;

public class WriteSet
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
			throw new RuntimeException(sigsFile.toString());

		String idsStr = args[2];
		String[] ids = idsStr.split(",");
		List<File> logcatFiles = new ArrayList();
		for(String id : ids) {
			File f = new File(dir, "logcatout."+id);
			if(f.exists())
				logcatFiles.add(f);
			else
				System.out.println(f + " does not exist.");
		}

		Set<Integer> fieldIds = new HashSet();
		for(File f : logcatFiles) {
			try {
				BufferedReader in = new BufferedReader(new FileReader(f));
				String s;
				while ((s = in.readLine()) != null) {
					s = s.trim();
					if(s.startsWith("E/A3T_WRITE")) {
						int i = s.lastIndexOf(' ');
						assert i > 0 : s;
						int fieldId = Integer.parseInt(s.substring(i+1));
						if(fieldId >= 0) {
							fieldIds.add(fieldId);
							//System.out.println(fieldId);
						}
					}
				}
				in.close();
			} catch (Exception ex) {
				throw new RuntimeException(ex);
			}
		}
		
		List<String> sigsList = readFileToList(sigsFile);
		int size = sigsList.size();
		for (Integer id : fieldIds) {
			if(id < size) {
				String sig = sigsList.get(id);
				System.out.println(sig);
			}
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