package a3t.explorer;

import org.apache.tools.ant.Project;
import org.apache.tools.ant.Task;

import java.io.File;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;

public class KillerTask extends Task
{
	private AdbTask push;
	private AdbTask rm;
	private static final String KILL_FILE = "a3t_kill_proc";
	private int port;

	KillerTask(int port)
	{
		this.port = port;
		push = new AdbTask(port, "push "+KILL_FILE+" /sdcard/"+KILL_FILE);		
		rm = new AdbTask(port, "shell rm /sdcard/"+KILL_FILE);
	}

	void prepare()
	{
		File file = new File(KILL_FILE);
		if(!file.exists()){
			try{
				BufferedWriter writer = new BufferedWriter(new FileWriter(file));
				writer.write(0);
				writer.close();
			}catch(IOException e){
				throw new Error(e);
			}
		}
		push.execute();
	}
	
	public void execute()
	{
		try{
			rm.execute();
		}catch(Exception e) {
			System.out.println("error occurred for " + port);
			throw new Error(e);
		}
	}
	
	public void setProject(Project pr)
	{
		push.setProject(pr);
		rm.setProject(pr);
	}
}