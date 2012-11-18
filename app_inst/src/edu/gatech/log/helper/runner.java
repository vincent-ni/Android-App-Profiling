package edu.gatech.log.helper;

import java.io.File;

/*
 * Input: logsPath -> point to a folder, which contains several logs
 *        outputPath -> point to a folder, in which the program generates files used for ml
 */
public class runner {

	public static void main(String[] args) {
//		String path = "/home/yjy/log.txt";
		String logsPath = "/home/yjy/workspace_profiler/logs";
		String outputPath = "/home/yjy/workspace_profiler/output";
		//String logsPath = "/home/vincent/Workspace/logs";
		//String outputPath = "/home/vincent/Workspace/output";
		
		ReadLog1 readLog = new ReadLog1();
		
		File logsFolder = new File(logsPath);
		for(File fileEntry : logsFolder.listFiles()){
			readLog.readLog(fileEntry.getAbsolutePath());
		}
		readLog.writeLog(outputPath);
	}

}
