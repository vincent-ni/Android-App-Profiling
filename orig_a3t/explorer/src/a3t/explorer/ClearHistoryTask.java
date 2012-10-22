package a3t.explorer;

import org.apache.tools.ant.Project;
import org.apache.tools.ant.Task;

public class ClearHistoryTask extends Task
{
	private AdbTask rmShared_prefs;
	private AdbTask rmDatabases;

	ClearHistoryTask(int port, String appPkgName)
	{
		rmShared_prefs = new AdbTask(port, "shell rm " + "/data/data/"+appPkgName+"/shared_prefs/*");
		rmDatabases = new AdbTask(port, "shell rm " + "/data/data/"+appPkgName+"/databases/*");
	}

	public void execute()
	{
		rmShared_prefs.execute();
		rmDatabases.execute();
	}

	public void setProject(Project pr)
	{
		rmShared_prefs.setProject(pr);
		rmDatabases.setProject(pr);
	}

}
