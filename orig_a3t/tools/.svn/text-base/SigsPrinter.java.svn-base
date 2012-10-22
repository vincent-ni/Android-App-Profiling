import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

public class SigsPrinter {
	public static void main(String[] args) {
		String sigsFile = args[0];
		if (sigsFile == null || !(new File(sigsFile)).exists())
			throw new RuntimeException();
		String idsFile = args[1];
		if (idsFile == null || !(new File(idsFile)).exists())
			throw new RuntimeException();
		List<String> sigsList = readFileToList(sigsFile);
		List<String> idsList = readFileToList(idsFile);
		for (String s : idsList) {
			int id = Integer.parseInt(s);
			if (id == -1)
				System.out.println("array element");
			else
				System.out.println(sigsList.get(id));
		}
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
