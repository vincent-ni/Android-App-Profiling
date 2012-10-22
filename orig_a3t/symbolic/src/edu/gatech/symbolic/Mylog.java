package edu.gatech.symbolic;

import java.io.File;
import java.io.BufferedWriter;
import java.io.PrintWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.FileReader;

public class Mylog
{
	private static final String MYLOG = "/mylog.txt";
	private static final String LOG_DIR_PREFIX = "/data/data/";
	private static final String PKG_FILE = "/sdcard/pkg.txt";
	private static final String KILLED_FILE = "/a3t_killed_proc";
	
	private static PrintWriter writer;

	static {
		new ShutDownHook().start();
	}

	private static PrintWriter writer()
	{
		if(writer == null) {
			try{
				writer = new PrintWriter(new BufferedWriter(new FileWriter("/sdcard"+MYLOG)));
			}catch(IOException e){
				throw new Error(e);
			}
		}
		return writer;
	}

	public static void e(String tag, String msg)
	{
		writer().println("E/"+tag+" : "+msg);
	}
	
	public static void println(String msg)
	{
		writer().println(msg);
	}

	private static class ShutDownHook extends Thread
	{
		ShutDownHook()
		{
		}

		public void run()
		{
			File file = new File("/sdcard/a3t_kill_proc");
			while(file.exists()){
				try{
					sleep(500);
				}catch(InterruptedException e){
					break;
				}
			}
			// e("A3T_METHS", Util.reachedMethsStr());
			android.util.Slog.e("Mylog", "Shutting down");
			
			if(writer != null) {
				if(writer.checkError())
					throw new Error("error in writing to mylog.txt");
				writer.flush();
				writer.close();
			}

			try{
				String pkg = new BufferedReader(new FileReader(PKG_FILE)).readLine();
				File killedfile = new File(LOG_DIR_PREFIX+pkg+KILLED_FILE);
				BufferedWriter writer = new BufferedWriter(new FileWriter(killedfile));
				writer.write(0);
				writer.close();
			}catch(IOException e){
				throw new Error(e);
			}
			android.util.Slog.e("Mylog", "About to exit.");
			
			System.exit(0);
		}
	}
}
