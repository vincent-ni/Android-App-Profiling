package a3t.instrumentor;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import java.util.regex.Pattern;

public class Filter
{
	private final Pattern filter;

	Filter(String patterns)
	{
		filter = readFilter(patterns);
	}

	boolean matches(String sig)
	{
		if (filter != null && filter.matcher(sig).matches())
			return true;
		return false;
	}

	private String escape(String s) {
		return s.
			replaceAll("\\$", "\\\\\\$").
			replaceAll("\\<", "\\\\\\<").
			replaceAll("\\>", "\\\\\\>").
			replaceAll("\\.", "\\\\\\.").
			replaceAll("\\:", "\\\\\\:").
			replaceAll("\\[", "\\\\\\[").
			replaceAll("\\]", "\\\\\\]");
	}

	private Pattern readFilter(String filterFile) {
		if(!(new File(filterFile).exists()))
			return null;
		List<String> filters = readFileToList(filterFile);
		if(filters.size() == 0)
			return null;
		else if(filters.size() == 1){
			String f = filters.get(0);
			System.out.println(f);
			return Pattern.compile(f);
		}
		String f = filters.remove(0);
		f = escape(f.trim());
		StringBuilder builder = new StringBuilder("("+f+")");
		for(String g : filters) {
			g = g.trim();
			if(g.equals(""))
				continue;
			builder.append("|("+escape(g)+")");
		}
		String p = builder.toString();
		System.out.println("%% " + p);
		return Pattern.compile(p);
	}

	private List<String> readFileToList(String fileName) {
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