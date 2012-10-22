package a3t.instrumentor;

import java.io.*;
import java.util.Map;
import java.util.HashMap;
import soot.SootMethod;

public class SigNumberer {
	private final Map<String, Integer> sigToId = new HashMap<String, Integer>();

	public int getNumber(String s) {
		Integer i = sigToId.get(s);
		if (i != null)
			return i.intValue();
		int id = sigToId.size();
		sigToId.put(s, id);
		return id;
	}

	public void save(String fileName) {
		String[] a = new String[sigToId.size()];
		for (Map.Entry<String, Integer> e : sigToId.entrySet()) {
			int id = e.getValue();
			a[id] = e.getKey();
		}
        try {
            PrintWriter out = new PrintWriter(new File(fileName));
			for (String s : a)
				out.println(s);
            out.close();
        } catch (IOException ex) {
            ex.printStackTrace();
            System.exit(1);
        }
	}

	public void load(String fileName) {
        try {
			File file = new File(fileName);
			if (!file.exists())
				return;
            BufferedReader reader = new BufferedReader(new FileReader(file));
            String s;
			for (int id = 0; (s = reader.readLine()) != null; id++) {
				sigToId.put(s, id);
			}
			reader.close();
        } catch (IOException ex) {
            ex.printStackTrace();
            System.exit(1);
        }
	}
}
