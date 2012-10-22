import java.io.*;
import java.util.*;

public class ExtractResult
{
	final static String generatedStr = "(stat) No. of tests generated in round ";
	final static String extendedStr = "(stat) No. of tests to be extended in round ";

	public static void main(String[] args) throws IOException
	{
		String outDir = args[0];

		List<Integer> readOnlyNums = new ArrayList();
		List<Integer> naiveNums = new ArrayList();

		process(new File(outDir, "readonly/out.txt"), readOnlyNums);
		process(new File(outDir, "naive/out.txt"), naiveNums);
		
		int size = readOnlyNums.size();
		assert size == naiveNums.size();
		
		System.out.println("#   DART feasible | Contest feasible");
		for(int i = 0; i < size; i++)
			System.out.println((i+1) + "\t\t" + naiveNums.get(i) + "\t\t" + readOnlyNums.get(i));
	}

	private static void process(File outFile, List<Integer> nums) throws IOException
	{
		BufferedReader reader = new BufferedReader(new FileReader(outFile));
		String line;
		while((line = reader.readLine()) != null){
			int ret = check(line, generatedStr);
			if(ret >= 0) {
				nums.add(ret);
				continue;
			} 
			ret = check(line, extendedStr);
			if(ret > 0) {
				nums.add(ret);
				continue;
			}
		}
		reader.close();
	}
	
	private static int check(String line, String str)
	{
		int index = line.indexOf(str);
		if(index > 0) {
			Integer num = Integer.parseInt(line.substring(line.indexOf("=")+2));
			System.out.println(line);
			return num;
		}
		return -1;
	}
}