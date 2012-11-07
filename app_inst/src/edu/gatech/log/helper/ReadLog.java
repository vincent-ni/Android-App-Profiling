package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

public class ReadLog {
	public String logPath;
	public List<MethodInfo> methods;
	public List<FeatInfo> features;
	
	public ReadLog(String logPath){
		this.logPath = logPath;
		methods = new ArrayList<MethodInfo>();
		features = new ArrayList<FeatInfo>();
	}
	
	public void readLog(){
		try{
			BufferedReader reader = new BufferedReader(new FileReader(new File(logPath)));
			String str;
			while((str = reader.readLine()) != null){
				
			}
		} catch(Exception e){
			e.printStackTrace();
		}
	}
	
	public void writeLog(){
		
	}
}
