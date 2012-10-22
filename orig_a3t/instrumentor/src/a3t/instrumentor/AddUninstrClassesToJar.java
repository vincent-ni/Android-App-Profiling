package a3t.instrumentor;

import org.apache.tools.ant.taskdefs.Jar ;
import org.apache.tools.ant.types.ZipFileSet;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.Target;
import org.apache.tools.ant.Project;
import org.apache.tools.ant.Task;

import java.io.File;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

/**
   copies the uninstrumented classes from the original
   jar file to the instrumented jar file
*/
public class AddUninstrClassesToJar extends Jar
{
	public AddUninstrClassesToJar(Map<String,List<String>> uninstrumentedClasses,
								  String instrumentedJarName)
	{
		super();
		
		setDestFile(new File(instrumentedJarName));
		setUpdate(true);

		for (Map.Entry<String,List<String>> e : uninstrumentedClasses.entrySet()) {
			String originalJarName = e.getKey();
			ZipFileSet originalJar = new ZipFileSet();
			originalJar.setSrc(new File(originalJarName));
			System.out.println("original jar: " + originalJarName);

			List<String> classes = e.getValue();
			int numFilesToCopy = classes.size();
			String[] array = new String[numFilesToCopy];
			int i = 0;
			for (String className : classes) {
				className = className.replace('.', File.separatorChar) + ".class";
				array[i++] = className;
				System.out.println("copy from original jar: "+ className);
			}
			originalJar.appendIncludes(array);

			addZipfileset(originalJar);
		}
	}

	public void apply()
	{
		Project project = new Project();
		setProject(project);

		Target target = new Target();
		target.setName("addtojar");
		target.addTask(this);
		project.addTarget(target);
		target.setProject(project);

		project.init();
		target.execute();
	}

	public static void main(String[] args)
	{
		String originalJarName = args[0];
		String instrumentedJarName = args[1];
		List uninstrumentedClasses = new ArrayList();
		
		for(int i = 2; i < args.length; i++)
			uninstrumentedClasses.add(args[i]);

		Map map = new HashMap();
		map.put(originalJarName, uninstrumentedClasses);
		new AddUninstrClassesToJar(map, instrumentedJarName).apply();
	}
}
