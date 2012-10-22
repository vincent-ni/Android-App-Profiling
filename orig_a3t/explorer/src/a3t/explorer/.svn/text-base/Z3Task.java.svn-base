package a3t.explorer;

import org.apache.tools.ant.taskdefs.ExecTask;
import org.apache.tools.ant.types.Commandline;
import org.apache.tools.ant.Target;
import org.apache.tools.ant.Project;
import java.io.File;

public class Z3Task extends ExecTask
{
	private static String args;
	private final Target target = new Target();

	public Z3Task()
	{
		setExecutable("/bin/sh");

		Project project = new Project();
		setProject(project);

		target.setName("runZ3");
		target.addTask(this);
		project.addTarget(target);
		target.setProject(project);

		project.init();
	}
	
	public void exec(File outFile, File errFile, String file)
	{
		Commandline.Argument cmdLineArgs = createArg();
		String args2 = args + file;

		args2 += " 1>"+outFile;
		args2 += " 2>"+errFile;

		args2 = args2 + "\"";

		cmdLineArgs.setLine(args2);

		//setError(errFile);
		//setOutput(outFile);

		target.execute();
	}

	public void execute()
	{
		super.execute();
	}
	
	static void setup(String z3Path)
	{
		if(z3Path == null)
			throw new Error("z3Path is null");
		File file = new File(z3Path);
		if(!file.exists())
			throw new Error("z3Path does not exist. " + z3Path);

		args = "-c \"" + file.getAbsolutePath() + " -m ";		
	}
}
