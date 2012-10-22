package a3t.explorer;

import org.apache.tools.ant.taskdefs.ExecTask;
import org.apache.tools.ant.types.Commandline;

public class AdbTask extends ExecTask
{
	protected int port;

	public AdbTask(int port, String args)
	{
		this.port = port;
		setExecutable("adb");
		Commandline.Argument cmdLineArgs = createArg();
		args = "-s emulator-"+port+" " + args;
		cmdLineArgs.setLine(args);

		setFailonerror(true);
		
		setAppend(true);
		setOutput(Main.newOutFile("adbout."+port));
	}
	
	public void execute()
	{
		super.execute();
	}
}