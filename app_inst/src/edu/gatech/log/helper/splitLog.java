package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/*
 * Usage:
 *   1. change the logsPath string, which means the original log folder (.../example)
 *   2. this program will generate all the separated logs into a new folder (.../example_new)
 *   3. use the new folder in runner (for ReadLog)
 */
public class splitLog {
	public static void main(String[] args) {
		String logsPath = "/home/yjy/experiment/chess/tablet";
		int logCount;
		
		try {	
			String newFolderPath = logsPath + "_new/";
			File newFolder = new File(newFolderPath);
			if(!newFolder.exists())
				newFolder.mkdir();
			
			File logsFolder = new File(logsPath);
			for(File fileEntry : logsFolder.listFiles()){
				BufferedReader reader = new BufferedReader(new FileReader(fileEntry));
				String fileName = fileEntry.getName();
				fileName = fileName.substring(0, fileName.indexOf(".txt"));
				List<String> stringList = new ArrayList<String>();
				logCount = 0;
				String str = reader.readLine();
				while(str != null){
					if(str.contains("Time:  ")){
						if(stringList.size() > 0){
							PrintWriter writer = new PrintWriter(new File(newFolderPath 
									+ fileName + "_" + logCount++ + ".txt"));
							for(int i = 0; i < stringList.size(); i++){
								writer.println(stringList.get(i));
							}
							writer.close();
						}
						stringList.clear();	
					}
					stringList.add(str);
					str = reader.readLine();
				}
				reader.close();
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

}
