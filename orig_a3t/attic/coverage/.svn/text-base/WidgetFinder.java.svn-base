package a3t.instrumentor.coverage;

import soot.SootClass;
import soot.SootField;
import soot.Unit;
import soot.Scene;
import soot.SootMethod;
import soot.Transform;
import soot.Body;
import soot.BodyTransformer;
import soot.PackManager;
import soot.tagkit.Tag;
import soot.tagkit.ConstantValueTag;

import java.io.File;
import java.io.PrintWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Map;

public class WidgetFinder extends BodyTransformer
{
	private static String pkgName;
    private static String inJars;
    private static String outFile;
    private static String libraryJars;

	protected void internalTransform(Body body, String phase, Map options)
	{
		SootMethod method = body.getMethod();
		SootClass klass = method.getDeclaringClass();

		try{
			PrintWriter writer = new PrintWriter(new FileWriter(new File(outFile == null ? "all_widgets.txt" : outFile)));
			
			for(SootField fld : klass.getFields()){
				for(Tag tag : fld.getTags()){
					if(tag instanceof ConstantValueTag){
						writer.println(fld.getName() + " " + tag.toString().replace("ConstantValue:", ""));
					break;
					}
				}
			}
			writer.close();
		}catch(IOException e){
			throw new Error(e);
		}
	}

	public static void main(String[] args)
	{
		processArgs(args);
		
		Scene.v().setSootClassPath(inJars + File.pathSeparator + libraryJars);
				
		PackManager.v().getPack("jtp").add(new Transform("jtp.widget", new WidgetFinder()));

		String className = pkgName + "." + "R$id";
		
		StringBuilder builder = new StringBuilder();
		builder.append("-soot-classpath ");
		builder.append(inJars + File.pathSeparator + libraryJars + " ");
		builder.append("-f none ");
		builder.append(className);

		String[] sootArgs = builder.toString().split(" ");
		soot.Main.main(sootArgs);
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
			if (s.equals("-pkg")) {
				pkgName = args[i+1];
				i++;
			} else if (s.equals("-injars")) {
				inJars = args[i+1];
				i++;
			} else if (s.equals("-widgetsfile")) {
				outFile = args[i+1];
				i++;
			} else if (s.equals("-libraryjars")) {
				libraryJars = args[i+1];
				i++;
			}
			else
				throw new RuntimeException("unknown arg " + s);
		}
    }
}
