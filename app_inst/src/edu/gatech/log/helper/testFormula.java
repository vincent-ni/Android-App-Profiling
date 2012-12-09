package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.List;

public class testFormula {  
	private static String folder = "/home/yjy/workspace_profiler/ml/";   	
	private static String resultPath = "/home/yjy/experiment/data_file/chess_tablet/m0/";
//	private static String folder = "/home/vincent/Workspace/gitrepo/Android-App-Profiling/ml4profiler/"; 
//	private static String resultPath = "/home/vincent/Workspace/output/m0/";
	private static String resultFile = "currently_chosen_features.txt";
	private static String featDataFile = "feature_data.txt";
	private static String execConstFile = "exectime_const.txt";
	private static String execTimeFile = "exectime.txt";
	private static int expTimes = 10;
	private static int maxTerms = 5;
	private static List<Integer> featSelected = new ArrayList<Integer>();
	private static int[][] featData;
	private static double[][] normalizedData;
	private static double[] execTime;
	private static String formula;
	private static List<String> outputStreamList = new ArrayList<String>();
	private static List<String> randomStreamList = new ArrayList<String>();
	private static List<Integer> randomArrayList = new ArrayList<Integer>();
	private static int num_train = 0;
	private static int exect_const = 0; 
	
	public static void main(String[] args) {
		readFeatureData();
		readExectime();
		readExectConst();
		runPerls();
		
//		testOneFormula("M(f) =    0.232 + 1.527*f1 + 7.260*f1*f3 + 0.901*f3");
		dealWithRandomStream();
		testOneFormula(formula);
//		testOneFormula("M(f) =    0.241 + 2.082*f1 + 6.777*f1*f3 + 0.631*f3");
		printOutputStream();
	}
	
	private static void testOneFormula(String formula){
		formula = formula.replace(" ", "");
		formula = formula.substring(formula.indexOf("=") + 1);
		
		System.out.println(formula);
		
		double totalRatio = 0;
//		for(int i = num_train; i < randomArrayList.size(); i++){
//			totalRatio += evaluateFormula(randomArrayList.get(i) - 1, formula);
//		}
//		System.out.println(totalRatio / (randomArrayList.size() - num_train));
		for(int i = 0; i < execTime.length; i++){
			totalRatio += evaluateFormula(i, formula);
		}
		System.out.println(totalRatio / execTime.length);
	}
	
	private static double evaluateFormula(int run, String formula){
		int startPos = 0, endPos = -1;
		boolean plusOp = true;
		double result = 0;
		if(formula.charAt(0) == '-') {
			plusOp = false;
			startPos = 1;
		}
		for(int i = 1; i < formula.length(); i++){
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
		
//		System.out.println("Actual: " + execTime[run] + ", estimated: " + result 
//				+ ", ratio:" + result/execTime[run]);
		DecimalFormat df = new DecimalFormat("#.##");
		DecimalFormat df1 = new DecimalFormat("#.####");
		System.out.println(df.format(execTime[run]) + "\t" + df.format(result) + "\t" 
					+ (int)(execTime[run]*exect_const) + "\t" + (int)(result*exect_const) + "\t" 
					+ df1.format(result/execTime[run]));
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
						result *= normalizedData[featIndex - 1][run];
//						result *= featData[featIndex - 1][run];
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
			normalizedData = new double[featDatas.size()][runNum];
			
			for(int i = 0; i < featDatas.size(); i++){
				String[] strs = featDatas.get(i).split(" ");
				for(int j = 0; j < strs.length; j++){
					if(strs[j].trim().length() > 0){
						featData[i][j] = Integer.parseInt(strs[j].trim());
					}
				}
			}
			
			// do the normalization
			for(int i = 0; i < featDatas.size(); i++){
				int max = featData[i][0];
				int min = featData[i][0];
				for(int j = 0; j < runNum; j++){
					if(featData[i][j] > max) max = featData[i][j];
					if(featData[i][j] < min) min = featData[i][j];
				}
				double range = max - min;
				for(int j = 0; j < runNum; j++){
					normalizedData[i][j] = (featData[i][j] - min) / range;
				}
//				System.out.println(min + " " + range);
			}
			
			
		} catch(Exception e){
			e.printStackTrace();
		}
	}
	
	private static void readExectConst(){
		try{
			BufferedReader reader = new BufferedReader(new FileReader(new File(resultPath + execConstFile)));
			String string = reader.readLine();
			exect_const = Integer.parseInt(string.trim());
			reader.close();
			System.out.println("exect_const: " + exect_const);
		} catch(Exception e){
			
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
			execTime = new double[exectimes.size()];
			
			for(int i = 0; i < exectimes.size(); i++){
				String data = exectimes.get(i).trim();
//				execTime[i] = Integer.parseInt(data.substring(0, data.indexOf(" ")));
				execTime[i] = Double.parseDouble(data);
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
							+ "ml/stable/foba_poly_model_init.m " + resultPath + " 1 1");
				int exitValue = process.waitFor();
				if(exitValue != 0){
					System.out.println("Fatal error");
					return;
				}
				BufferedReader firstReader = new BufferedReader(new InputStreamReader(
						process.getInputStream()));
				List<String> tempStrList = new ArrayList<String>();
				String firstStr = firstReader.readLine();
				while(firstStr != null){
//					System.out.println("1: " + firstStr);
					tempStrList.add(firstStr);
					firstStr = firstReader.readLine();
				}
				firstReader.close();
//				System.out.println("3");
				
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
				Double error = (double) 100;
				if(!strs[0].contains("Inf"))
					error = Double.parseDouble(strs[0]);
				if(error < miniError){
					miniError = error;
					featSelected.clear();
					outputStreamList.clear();
					for(int i = 1; i < strs.length; i++)
						featSelected.add(Integer.parseInt(strs[i]));
					BufferedReader procReader = new BufferedReader(new InputStreamReader(
													process.getInputStream()));
					
					String string = procReader.readLine();
					while(string != null){
//						System.out.println("2: " + string);
						outputStreamList.add(string);
						if(string.contains("M(f)"))
							formula = string;
//						System.out.println(string);
						string = procReader.readLine();
					}
					randomStreamList.clear();
					randomStreamList.addAll(tempStrList);
//					testOneFormula(formula);
				}
//				if(error < 0.1){
//					BufferedReader procReader = new BufferedReader(new InputStreamReader(
//							process.getInputStream()));
//
//					String string = procReader.readLine();
//					while(string != null){
//						if(string.contains("M(f)")){
////							System.out.println(error + " " + string);
////							testOneFormula(string);
//						}
//						string = procReader.readLine();
//					}
//				}
			}
			System.out.println("error: " + miniError);
			for(int i = 0; i < featSelected.size(); i++){
				System.out.print(featSelected.get(i) + " ");
			}
			System.out.println();
			System.out.println(formula);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public static void printOutputStream(){
		for(int i = 0; i < randomStreamList.size(); i++){
			System.out.println(randomStreamList.get(i));
		}
		for(int i = 0; i < outputStreamList.size(); i++){
			System.out.println(outputStreamList.get(i));
		}
	}
	
	private static void dealWithRandomStream(){
		for(int i = 0; i < randomStreamList.size(); i++){
			if(randomStreamList.get(i).contains("Columns")){
				String toAnalyze = randomStreamList.get(i + 2);
				String[] strs = toAnalyze.split(" ");
				for(String numStr : strs){
					numStr = numStr.trim();
					if(numStr.length() > 0){
						int number = Integer.parseInt(numStr);
						randomArrayList.add(number);
					}
				}
//				System.out.println(strs);
			}
		}
		for(int i = 0; i < outputStreamList.size(); i++){
			if(outputStreamList.get(i).contains("num_train")){
				String temp = outputStreamList.get(i + 1).trim();
				num_train = Integer.parseInt(temp);
				break;
			}
		}
		System.out.println(num_train);
		System.out.println(randomArrayList);
		System.out.println(randomArrayList.size());
	}
}
