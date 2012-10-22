package a3t.instrumentor.coverage;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.Set;
import java.util.HashSet;

import java.io.File;
import java.io.FilenameFilter;

import java.util.regex.Pattern;

public class WidgetCoverage
{
	private static String logcatDir;
	private static String widgetsFile;
	private static final String PREFIX = "logcatout.";
	private static final Pattern pat = Pattern.compile(": dipatched to id = ");

	public static void main(String[] args)
	{
		processArgs(args);
		
		File dir = new File(logcatDir);
		FilenameFilter filter = new FilenameFilter(){
				public boolean accept(File dir, String name){
					if(!name.startsWith(PREFIX))
						return false;
					try{
						Integer.parseInt(name.replace(PREFIX,""));
					}catch(NumberFormatException e){
						return false;
					}
					return true;
				}
			};

		Set<Integer> coveredWidgets = new HashSet();
		try{
			for(String file : dir.list(filter)){
				BufferedReader reader = new BufferedReader(new FileReader(file));
				String line = reader.readLine();
				while(line != null){
					if(pat.matcher(line).find()){
						int i = line.indexOf('=')+2;
						int j = line.indexOf(' ', i);
						//System.out.println(line);
						int id = Integer.parseInt(line.substring(i,j));
						coveredWidgets.add(id);
					}
					line = reader.readLine();
				}
				//System.out.println(file);
			}
		}catch(IOException e){
			throw new Error(e);
		}
		
		try{
			int total = 0; int covered = 0;
			BufferedReader reader = new BufferedReader(new FileReader(widgetsFile == null ? "all_widgets.txt" : widgetsFile));
			String line = reader.readLine();
			while(line != null){
				String[] ss = line.split("  ");
				String name = ss[0];
				int id = Integer.parseInt(ss[1]);
				if(coveredWidgets.contains(id)){
					covered++;
					System.out.println("+ " + name);
				}
				else
					System.out.println("- " + name);
				total++;
				line = reader.readLine();
			}
			System.out.println("% coverage = " + ((covered*100.0)/total) + " ("+covered+"/"+total+")");

		}catch(IOException e){
			throw new Error(e);
		}
	}

	private static void processArgs(String[] args)
    {
		System.out.println("A3T arguments: ");
		for (int i = 0; i < args.length; i++) {
			System.out.print(args[i] + " ");
		}
		System.out.println("\n");

		for (int i = 0; i < args.length; i++) {
			String s = args[i];
			//System.out.println(s);
			if (s.equals("-logcatdir")) {
				logcatDir = args[i+1];
				i++;
			} else if (s.equals("-widgetsfile")) {
				widgetsFile = args[i+1];
				i++;
			}
			else
				throw new RuntimeException("unknown arg " + s);
		}
    }
}
