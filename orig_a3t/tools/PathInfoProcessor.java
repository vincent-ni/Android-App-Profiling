// Properties: a3t.max.iters, a3t.out.dir, a3t.writemap.file, a3t.methsigs.file, a3t.fieldsigs.file
// If you run this program in the app's home dir, then all you need to set is a3t.max.iters

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.regex.Pattern;

public class PathInfoProcessor {
	public static void main(String[] args) {
		int maxIters = Integer.getInteger("a3t.max.iters", -1);
		if (maxIters == -1)
			throw new RuntimeException("a3t.max.iters undefined");
		String dir = System.getProperty("a3t.out.dir", "a3t_out");
		if (dir == null || !(new File(dir)).exists())
			throw new RuntimeException();
	
		//impureMethods(maxIters);
		writeOnly(maxIters, dir, args);
	}

	private static void writeOnly(int maxIters, String dir, String[] args) {
		
		Pattern filter = args.length > 0 ? readFilter(args[0]) : null;

		String fieldSigsFile = System.getProperty("a3t.fieldsigs.file", "bin/a3t/fieldsigs.txt");
		if (fieldSigsFile == null || !(new File(fieldSigsFile)).exists())
			throw new RuntimeException();
		List<String> fieldSigs = readFileToList(fieldSigsFile);
		for (int i = 0; i < maxIters; i++) {
			int[] fieldSigIds = readWriteSet(dir + "/pathinfo." + i);
			if (fieldSigIds == null)
				continue;
			System.out.println("***** Run: " + i);
			boolean rolt = true;
			for (int fieldSigId : fieldSigIds) {
				String fld;
				if (fieldSigId != -1) {
					fld = fieldSigs.get(fieldSigId);
					if (filter != null && filter.matcher(fld).matches())
						continue;
				} else {
					fld = "array elem";
				}
				rolt = false;
				System.out.println(fld);
			}
			if(rolt)
				System.out.println("read-only-last-tap");
		}
	}

	private static Pattern readFilter(String filterFile) {
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

	private static void impureMethods(int maxIters, String dir) {
		String writeMapFile = System.getProperty("a3t.writemap.file", "bin/a3t/writemap.txt");
		if (writeMapFile == null || !(new File(writeMapFile)).exists())
			throw new RuntimeException();
		String methSigsFile = System.getProperty("a3t.methsigs.file", "bin/a3t/methsigs.txt");
		if (methSigsFile == null || !(new File(methSigsFile)).exists())
			throw new RuntimeException();
		String fieldSigsFile = System.getProperty("a3t.fieldsigs.file", "bin/a3t/fieldsigs.txt");
		if (fieldSigsFile == null || !(new File(fieldSigsFile)).exists())
			throw new RuntimeException();
	
		Map<Integer, String> writemap = loadWriteMap(writeMapFile, methSigsFile, fieldSigsFile);

		for (int i = 0; i < maxIters; i++) {
			int[] methSigIds = readImpureMeths(dir + "/pathinfo." + i);
			if (methSigIds == null)
				continue;
			System.out.println("***** Run: " + i);
			for (int methSigId : methSigIds)
				System.out.println(writemap.get(methSigId));
		}
	}

	private static Map<Integer, String> loadWriteMap(String writeMapFileName,
			String methSigsFileName, String fieldSigsFileName) {
		List<String> writeMap = readFileToList(writeMapFileName);
		List<String> methSigs = readFileToList(methSigsFileName);
		List<String> fieldSigs = readFileToList(fieldSigsFileName);
		Map<Integer, String> map = new HashMap<Integer, String>();
		for (String s : writeMap) {
			int at = s.indexOf('@');
			int methSigId = Integer.parseInt(s.substring(0, at));
			String methSig = methSigs.get(methSigId);
			String t = s.substring(at + 1);
			String[] a = t.split(" ");
			for (String r : a) {
				int fieldSigId = Integer.parseInt(r);
				String fieldSig = fieldSigId == -1 ? "array elem" : fieldSigs.get(fieldSigId);
				methSig += "\n\t" + fieldSig; 
			}
			map.put(methSigId, methSig);
		}
		return map;
	}

	private static int[] readImpureMeths(String fileName) {
		File file = new File(fileName);
		if (!file.exists())
			return null;
		List<String> lines = readFileToList(fileName);
		for (String line : lines) {
			if (line.startsWith("impuremeths=")) {
				String[] a = line.substring(12).split(" ");
				int[] methSigIds = new int[a.length];
				for (int i = 0; i < a.length; i++)
					methSigIds[i] = Integer.parseInt(a[i]);
				return methSigIds;
			}
		}
		throw new RuntimeException("Failed to find impuremeths in: " + fileName);
	}

	private static int[] readWriteSet(String fileName) {
		File file = new File(fileName);
		if (!file.exists())
			return null;
		List<String> lines = readFileToList(fileName);
		for (String line : lines) {
			if (line.startsWith("writeset=")) {
				line = line.substring(9);
				if(line.equals(""))
					return null;
				String[] a = line.split(" ");
				int[] fldSigIds = new int[a.length];
				for (int i = 0; i < a.length; i++)
					fldSigIds[i] = Integer.parseInt(a[i]);
				return fldSigIds;
			}
		}
		throw new RuntimeException("Failed to find writeset in: " + fileName);
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
