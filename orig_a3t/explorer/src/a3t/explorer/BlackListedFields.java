package a3t.explorer;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import java.util.regex.Pattern;

public class BlackListedFields
{
	private static List<String> fieldSigs;
	private static Pattern filter;

	static void setup(String fieldSigsFile, String blackListedFieldsFile)
	{
		if(blackListedFieldsFile == null)
			return;
		fieldSigs = readFileToList(fieldSigsFile);
		filter = readFilter(blackListedFieldsFile);
	}

	static boolean check(int fieldSigId)
	{
		if(filter == null)
			return false;

		if (fieldSigId == -1) 
			return false;

		String fld = fieldSigs.get(fieldSigId);
		if (filter.matcher(fld).matches())
			return true;
		
		return false;
	}

	private static Pattern readFilter(String filterFile) {
		if(!(new File(filterFile).exists()))
			return null;
		List<String> filters = readFileToList(filterFile);
		if(filters.size() == 0)
			return null;
		else if(filters.size() == 1){
			String f = "<"+filters.get(0)+">";
			System.out.println(f);
			return Pattern.compile(f);
		}
		String f = "(<"+filters.remove(0)+">)";
		StringBuilder builder = new StringBuilder(f);
		for(String g : filters) {
			if(g.trim().equals(""))
				continue;
			builder.append("|(<"+g+">)");
		}
		return Pattern.compile(builder.toString());
	}

	private static List<String> readFileToList(String fileName) {
		List<String> list = new ArrayList<String>();
		try {
            BufferedReader in = new BufferedReader(new FileReader(fileName));
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