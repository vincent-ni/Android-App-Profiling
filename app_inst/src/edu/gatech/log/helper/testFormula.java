package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

public class testFormula {  
	//private static String folder = "/home/yjy/pag/app-profile/Android-App-Profiling/ml4profiler/";   	
	//private static String resultPath = "/home/yjy/workspace_profiler/output/m0/";
	private static String folder = "/home/vincent/Workspace/gitrepo/Android-App-Profiling/ml4profiler/"; 
	private static String resultPath = "/home/vincent/Workspace/output/m0/";
	private static String resultFile = "currently_chosen_features.txt";
	private static String featDataFile = "feature_data.txt";
	private static String execTimeFile = "exectime.txt";
	private static int expTimes = 50;
	private static int maxTerms = 3;
	private static List<Integer> featSelected = new ArrayList<Integer>();
	private static int[][] featData;
	private static int[] execTime;
	private static String formula;
	
	public static void main(String[] args) {
		runPerls();
		formula = formula.replace(" ", "");
		formula = formula.substring(formula.indexOf("=") + 1);
		
		readFeatureData();
		readExectime();
		double totalRatio = 0;
		for(int i = 0; i < execTime.length; i++){
			totalRatio += evaluateFormula(i);
		}
		System.out.println(totalRatio / execTime.length);
	}
	
	private static double evaluateFormula(int run){
		int startPos = 0, endPos = -1;
		boolean plusOp = true;
		double result = 0;
		for(int i = 0; i < formula.length(); i++){
			if(formula.charAt(i) == '+' || formula.charAt(i) == '-'){
				endPos = i;
				if(plusOp) 
					result += calExp(formula.substring(startPos, endPos), run);
				else 
					result -= calExp(formula.substring(startPos, endPos), run);
				startPos = endPos + 1;
				plusOp = (formula.charAt(i) == '+') ? true : false;
			}
		}
		endPos = formula.length();
		if(plusOp){
			result += calExp(formula.substring(startPos, endPos), run);
		} else {
			result -= calExp(formula.substring(startPos, endPos), run);
		}
		
		System.out.println("Actual: " + execTime[run] + ", estimated: " + result 
				+ ", ratio:" + result/execTime[run]);
		return result/execTime[run];
	}
	
	private static double calExp(String str, int run){
		double result = 1;
		int startPos = 0;
//		System.out.println(str);
		str += '*';
		for(int i = 0; i < str.length(); i++){
			char ch = str.charAt(i);
			if(ch == '*'){
				String subExp = str.substring(startPos, i);
				startPos = i + 1;
				if(subExp.charAt(0) >= '0' && subExp.charAt(0) <= '9'){
					result *= Double.parseDouble(subExp);
				} else if(subExp.charAt(0) == 'f'){
					int featIndex, pow = 1;
					if(subExp.contains("^")){
						featIndex = Integer.parseInt(subExp.substring(1, subExp.indexOf("^")));
						pow = Integer.parseInt(subExp.substring(subExp.indexOf("^") + 1));
					} else 
						featIndex = Integer.parseInt(subExp.substring(1));
					while(pow > 0){
						result *= featData[featIndex - 1][run];
						pow--;
					}
				} else {
					System.out.println("Missing informaion when processing this exp: " + str);
					return 0;
				}
			}
		}
//		System.out.println(result);
		return result;
	}
	
	private static void readFeatureData(){
		try{
			BufferedReader reader = new BufferedReader(new FileReader(new File(resultPath + featDataFile)));
			List<String> featDatas = new ArrayList<String>();
			String str = reader.readLine();
			
			while(str != null){
				featDatas.add(str);
				str = reader.readLine();
			}
			reader.close();
			
			int runNum = featDatas.get(0).split(" ").length;
			featData = new int[featDatas.size()][runNum];
			
			for(int i = 0; i < featDatas.size(); i++){
				String[] strs = featDatas.get(i).split(" ");
				for(int j = 0; j < strs.length; j++){
					if(strs[j].trim().length() > 0){
						featData[i][j] = Integer.parseInt(strs[j].trim());
					}
				}
			}
		} catch(Exception e){
			e.printStackTrace();
		}
	}
	
	private static void readExectime(){
		try{
			BufferedReader reader = new BufferedReader(new FileReader(new File(resultPath + execTimeFile)));
			List<String> exectimes = new ArrayList<String>();
			String str = reader.readLine();
			while(str != null){
				exectimes.add(str);
				str = reader.readLine();
			}
			reader.close();
			execTime = new int[exectimes.size()];
			
			for(int i = 0; i < exectimes.size(); i++){
				String data = exectimes.get(i).trim();
//				execTime[i] = Integer.parseInt(data.substring(0, data.indexOf(" ")));
				execTime[i] = Integer.parseInt(data);
			}
		} catch(Exception e){
			e.printStackTrace();
		}
	}
	
	private static void runPerls(){
		double miniError = 100;
		try {
			Runtime run = Runtime.getRuntime();
			for(int t = 0; t < expTimes; t++){
				Process process = run.exec("octave -qf -p " + folder + "ml/common " + folder
						+ "ml/stable/foba_poly_model_init.m " + resultPath + " 1 " + "1");
				int exitValue = process.waitFor();
				if(exitValue != 0){
					System.out.println("Fatal error");
					return;
				}
				process = run.exec("octave -qf -p " + folder + "ml/common " + folder
						+ "ml/stable/foba_poly_model.m " + resultPath + " 2 " + maxTerms);
				exitValue = process.waitFor();
				if(exitValue != 0){
					System.out.println("Fatal error");
					return;
				}
				
				BufferedReader reader = new BufferedReader(new FileReader(
						new File(resultPath + resultFile)));
				String str = reader.readLine();
				reader.close();
				String[] strs = str.split(" ");
				Double error = Double.parseDouble(strs[0]);
				if(error < miniError){
					miniError = error;
					featSelected.clear();
					for(int i = 1; i < strs.length; i++)
						featSelected.add(Integer.parseInt(strs[i]));
					BufferedReader procReader = new BufferedReader(new InputStreamReader(
													process.getInputStream()));
					
					String string = procReader.readLine();
					while(string != null){
						if(string.contains("M(f)"))
							formula = string;
//						System.out.println(string);
						string = procReader.readLine();
					}
				}
			}
			System.out.println(miniError);
			for(int i = 0; i < featSelected.size(); i++){
				System.out.print(featSelected.get(i) + " ");
			}
			System.out.println();
			System.out.println(formula);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

}
