package edu.gatech.appInst;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import edu.gatech.util.innerFeature;

import soot.PackManager;
import soot.Scene;
import soot.SceneTransformer;
import soot.SootClass;
import soot.Transform;

public class Main extends SceneTransformer {
	private static List<SootClass> classes = new ArrayList<SootClass>();
	private static Map<String,List<String>> uninstrumentedClasses = new HashMap<String, List<String>>();
	private static final String dummyMainClassName = "edu.gatech.appInst.DummyMain";
	private static boolean DEBUG = true;

	static String inJars = "libs/app.jar";
	static String libJars = "libs/core.jar:libs/ext.jar:libs/junit.jar:libs/bouncycastle.jar:bin:libs/android.jar";
	static String outJar = "output/intrumentedApp.jar";
	
	@Override
	protected void internalTransform(String arg0, Map arg1) {
		if (DEBUG) printClasses("bef_instr.txt");
		Instrumentor ci = new Instrumentor();
		ci.instrument(classes);
		Scene.v().getApplicationClasses().remove(Scene.v().getSootClass(dummyMainClassName));
				
		if (DEBUG) printClasses("aft_instr.txt");
	}
	
	public static void main(String[] args) {
		Scene.v().setSootClassPath(inJars + File.pathSeparator + libJars);

		loadClassesToInstrument();
		
		loadOtherClasses();
		
		/* add a phase to transformer pack by call Pack.add */
		PackManager.v().getPack("wjtp").add(new Transform("wjtp.a3t", new Main()));
		
		StringBuilder builder = new StringBuilder();
		builder.append("-w -p cg off -keep-line-number -keep-bytecode-offset ");
		builder.append("-dynamic-class ");
		builder.append("edu.gatech.util.innerClass ");
		builder.append("-soot-classpath ");
		builder.append(inJars + File.pathSeparator + libJars + " ");
		builder.append("-dynamic-package ");
		builder.append("edu.gatech.util. ");
//		builder.append("-dynamic-package ");
//		builder.append("models. ");
		builder.append("-outjar -d ");
		builder.append(outJar+" ");
		builder.append("-O ");
		builder.append("-validate ");
		builder.append(dummyMainClassName);
		String[] sootArgs = builder.toString().split(" ");
		soot.Main.main(sootArgs);

		new AddUninstrClassesToJar(uninstrumentedClasses, outJar).apply();
	}
	
	private static void loadClassesToInstrument()
    {
		for (String pname : inJars.split(File.pathSeparator)) {
			if (pname.endsWith(".jar")) {
				System.out.println("pname "+pname);
				JarFile jar = null;
				try {
					jar = new JarFile(pname);
				} catch(IOException e) {
					throw new RuntimeException(e.getMessage() + " " + pname);
				}
				for (Enumeration<JarEntry> e = jar.entries(); e.hasMoreElements();) {
					JarEntry entry = e.nextElement();
					String name = entry.getName();
					if (name.endsWith(".class")) {
						name = name.replace(".class", "").replace(File.separatorChar, '.');
						if(!toBeInstrumented(name)){
							System.out.println("Skipped instrumentation of class: " + name);
							addUninstrumentedClass(pname, name);
							continue;
						}
						try{
							SootClass klass = Scene.v().loadClassAndSupport(name);
							classes.add(klass);
						}catch(RuntimeException ex) {
							System.out.println("Failed to load class: " + name);
							addUninstrumentedClass(pname, name);
							if (ex.getMessage().startsWith("couldn't find class:")) {
								System.out.println(ex.getMessage());
							}
							else
								throw ex;
						}
					}
				}
			}
		}
    }
	
	private static void loadOtherClasses()
	{
		String[] classNames = new String[]{
			"edu.gatech.util.innerClass",
			"edu.gatech.util.innerFeature"
		};
		
		for(String cname : classNames){
			SootClass klass = Scene.v().loadClassAndSupport(cname);
			classes.add(klass);
		}
	}
	
//	private static Pattern excludePat = Pattern.compile(
//			 "(java\\..*)|(dalvik\\..*)|(android\\.os\\.(Parcel|Parcel\\$.*))|(android\\.util\\.Slog)|(android\\.util\\.(Log|Log\\$.*))");

	private static boolean toBeInstrumented(String className)
	{
		//return excludePat.matcher(className).matches() ? false : true;
		return true;
	}
	
	private static void addUninstrumentedClass(String jarName, String className) {
		List<String> cs = uninstrumentedClasses.get(jarName);
		if (cs == null) {
			cs = new ArrayList<String>();
			uninstrumentedClasses.put(jarName, cs);
		}
		cs.add(className);
	}
	
	private static void printClasses(String fileName) {
		try {
			PrintWriter out = new PrintWriter(new FileWriter(fileName));
			for (SootClass klass : classes) {
				Printer.v().printTo(klass, out);
			}
			out.close();
		} catch (IOException ex) {
			ex.printStackTrace();
			System.exit(1);
		}
	}
}
