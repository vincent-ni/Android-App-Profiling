package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Map.Entry;

public class ReadLog1 {
	public static Map<String, List<MethodInfo>> path2methods = new HashMap<String, List<MethodInfo>>();
	 // only consider those methods whose runtime is bigger than 10
	private static int timeThrehold = 2000;
	private static String methodInfoFileName = "method_info.txt";
	private static String featureCostFileName = "feature_cost.txt";
	private static String execTimeFileName = "exectime.txt";
	private static String featureDataFileName = "feature_data.txt";
	private static String featureNameFileName = "feature_name.txt";
	
	public void readLog(String logPath){
		Map<Integer, List<FeatInfo>> seq2features = new HashMap<Integer, List<FeatInfo>>();;
		List<MethodInfo> methods = new ArrayList<MethodInfo>();
		
		try{
			BufferedReader reader = new BufferedReader(new FileReader(new File(logPath)));
			String str;
			List<String> featStrList = new ArrayList<String>();
			List<String> paraStrList = new ArrayList<String>();
			while((str = reader.readLine()) != null){
				String[] strs = str.split(":");
				if(strs[0].contains("Time")){
					MethodInfo methodInfo = new MethodInfo(strs);
					methodInfo.setLogPath(logPath);
					methods.add(methodInfo);
					if(featStrList.size() > 0){
						List<FeatInfo> features = new ArrayList<FeatInfo>();
						for(String featStr : featStrList){
							FeatInfo featInfo = new FeatInfo(featStr);
							features.add(featInfo);
						}
						seq2features.put(methodInfo.seq, features);
						featStrList.clear();
					}
					if(paraStrList.size() > 0) {
						if (seq2features.containsKey(methodInfo.seq - 1)) {
							List<FeatInfo> features = seq2features.get(methodInfo.seq - 1);
							for(String paraStr : paraStrList){
								FeatInfo featInfo = new FeatInfo(paraStr);
								features.add(featInfo);
							}
						} else {
							List<FeatInfo> features = new ArrayList<FeatInfo>();
							for(String paraStr : paraStrList){
								FeatInfo featInfo = new FeatInfo(paraStr);
								features.add(featInfo);
							}
							seq2features.put(methodInfo.seq - 1, features);
						}
						paraStrList.clear();
					}
				} else if(strs[0].contains("ret") || strs[0].contains("loop")){
					featStrList.add(str);
				} else if(strs[0].contains("para")){
					paraStrList.add(str);
				}
			}
			reader.close();

			for(MethodInfo info : methods){
				if(seq2features.containsKey(info.seq)){
					info.setFeatList(seq2features.get(info.seq));
				}
			}
			path2methods.put(logPath, methods);
		} catch(Exception e){
			e.printStackTrace();
		}
	}
	
	/*
	 * Must generate: exectime.txt  feature_cost.txt  feature_data.txt
	 *                (essential for ml part)
	 * May generate: feature_name.txt method_info.txt
	 */
	public void writeLog(String outputPath){
		// key: fileName + lineNum + methodSig
		Map<String, List<MethodInfo>> key2MethodMap = new HashMap<String, List<MethodInfo>>();
		
		// find out how many unique methods we have. Each method will have its own folder later
		for(Entry<String, List<MethodInfo>> entry : path2methods.entrySet()){
			for(MethodInfo info : entry.getValue()){
				String key = info.generateKey();
				if(key2MethodMap.containsKey(key)){
					key2MethodMap.get(key).add(info);
				} else {
					List<MethodInfo> methodList = new ArrayList<MethodInfo>();
					methodList.add(info);
					key2MethodMap.put(key, methodList);
				}
			}
		}
		
		/*
		 * For each method call in the app,
		 * 1. check how many features it has
		 * 2. allocate the feature data, according to the number of features and runs
		 * 3. write them to files in format which ml needs 
		 */
		int methodCount = 0;
		for(Entry<String, List<MethodInfo>> entry : key2MethodMap.entrySet()){
			List<MethodInfo> methodList = entry.getValue();
			String key = entry.getKey();
			Set<String> featureSet = new HashSet<String>();
			int maxRunTime = 0;
			for(MethodInfo info : methodList){
				if(info.runTime > maxRunTime) 
					maxRunTime = info.runTime;
				List<FeatInfo> featList = info.feats;
				if(featList != null){
					for(FeatInfo featInfo : featList){
						featureSet.add(featInfo.featName);
					}
				}
			}
			if(maxRunTime < timeThrehold) 
				continue;
			
			String folderPath = outputPath + "/m" + (methodCount++);
			File folder = new File(folderPath);
			if(!folder.exists()) 
				folder.mkdir();
			writeMethodInfoFile(folderPath, key);
			
			int featureNum = featureSet.size();
			writeFeatureCostFile(folderPath, featureNum);
			
			writeExecTimeFile(folderPath, methodList);
			
			Map<String, Integer> featName2index = new HashMap<String, Integer>();
			int featIndex = 0;
			for(String entry2 : featureSet){
				featName2index.put(entry2, featIndex);
				featIndex++;
			}
			
			int[][] featData = new int[featureNum][methodList.size()];
			for(int i = 0; i < featureNum; i++)
				for(int j = 0; j < methodList.size(); j++){
					featData[i][j] = 0;
				}
			String[] featFullInfos = new String[featureNum];
			for(int j = 0; j < methodList.size(); j++){
				MethodInfo info = methodList.get(j);
				List<FeatInfo> featList = info.feats;
				if(featList != null){
					for(FeatInfo featInfo : featList){
						int i = featName2index.get(featInfo.featName);
						featData[i][j] = featInfo.value;
						if(featFullInfos[i] == null){
							featFullInfos[i] = featInfo.generateFullInfo();
						}
					}
				}
			}
			writeFeatureDataFile(folderPath, featData, featureNum, methodList.size());
			writeFeatureNameFile(folderPath, featFullInfos, featureNum);
		}
		
		
	}
	
	private void writeFeatureNameFile(String folderPath, String[] featFullInfos, int featureNum){
		try{
			PrintWriter writer = new PrintWriter(new File(folderPath + "/" + featureNameFileName));
			for(int i = 0; i < featureNum; i++){
				writer.println(featFullInfos[i]);
			}
			writer.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	private void writeExecTimeFile(String folderPath, List<MethodInfo> methodList){
		try{
			PrintWriter writer = new PrintWriter(new File(folderPath + "/" + execTimeFileName));
			for(int i = 0; i < methodList.size(); i++){
				writer.println(methodList.get(i).runTime);
//				for(int j = 0; j < methodList.size(); j++){
//					writer.print(methodList.get(i).runTime + " ");
//				}
//				writer.println();
			}
			writer.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	private void writeFeatureDataFile(String folderPath, int[][] featData, int featNum, int runs){
		try{
			PrintWriter writer = new PrintWriter(new File(folderPath + "/" + featureDataFileName));
			for(int i = 0; i < featNum; i++){
				for(int j = 0; j < runs; j++){
					writer.print(featData[i][j] + " ");
				}
				writer.println();
			}
			writer.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	private void writeFeatureCostFile(String folderPath, int featureCount){
		try{
			PrintWriter writer = new PrintWriter(new File(folderPath + "/" + featureCostFileName));
			for(int i = 0; i < featureCount; i++)
				writer.println(1);
			writer.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	private void writeMethodInfoFile(String folderPath, String toWrite){
		try{
			PrintWriter writer = new PrintWriter(new File(folderPath + "/" + methodInfoFileName));
			writer.println(toWrite);
			writer.close();
		} catch(Exception e) {
			e.printStackTrace();
		}
	}	
}
