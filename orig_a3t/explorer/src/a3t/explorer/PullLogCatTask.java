package a3t.explorer;

import org.apache.tools.ant.Project;
import org.apache.tools.ant.Task;

import java.io.File;

public class PullLogCatTask extends Task
{
	private AdbTask pullKilledProcFile;
	private AdbTask pullLogCatFile;
	private AdbTask rmDeviceLogCatFile;
	private AdbTask rmDeviceKilledProcFile;
	private File killedFile;
	private final int port;

    private static final String LOG_FILE = "/mylog.txt";
    private static final String LOG_DIR_PREFIX = "/data/data/";
	private static final String KILLED_FILE = "a3t_killed_proc";
	private static final int MAX_TRY = 5;

	PullLogCatTask(int port, String appPkgName, File tmpLogCatFile)
	{
		this.port = port;
		String deviceLogCatFilePath = "/sdcard"+LOG_FILE;//LOG_DIR_PREFIX+appPkgName+LOG_FILE;
		String deviceKilledFilePath = LOG_DIR_PREFIX+appPkgName+"/"+KILLED_FILE;

		killedFile = Main.newOutFile(KILLED_FILE+".emu"+port);

		pullKilledProcFile = new AdbTask(port, "pull " + deviceKilledFilePath + " " + killedFile.getAbsolutePath());
		pullLogCatFile = new AdbTask(port, "pull " + deviceLogCatFilePath  + " " + tmpLogCatFile.getAbsolutePath());
		rmDeviceLogCatFile = new AdbTask(port, "shell rm " + deviceLogCatFilePath);
		rmDeviceKilledProcFile = new AdbTask(port, "shell rm " + deviceKilledFilePath);
	}

	void prepare()
	{
		if(killedFile.exists() && !killedFile.delete())
			throw new Error("cannot delete " +killedFile.getAbsolutePath());
		rmDeviceKilledProcFile.execute();
		rmDeviceLogCatFile.execute();
	}
	
	public void execute()
	{
		int count = 0;
		while(!killedFile.exists() && count < MAX_TRY) {
			try{
				pullKilledProcFile.execute();
			}catch(Exception e){
				count++;
				System.out.println("waiting for emulator-"+port+ " to finish.");
			}
			try{
				Thread.sleep(count*500);
			}catch(InterruptedException e){
				throw new Error("thread sleep");
			}
		}
		if(!killedFile.exists()) {
			throw new EmuGoneWildException(port);
		} else {
			System.out.println("");
			pullLogCatFile.execute();
		}
		
	}

	public void setProject(Project pr)
	{
		pullKilledProcFile.setProject(pr);
		pullLogCatFile.setProject(pr);
		rmDeviceLogCatFile.setProject(pr);
		rmDeviceKilledProcFile.setProject(pr);
	}

}
