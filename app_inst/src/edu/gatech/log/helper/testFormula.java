package edu.gatech.log.helper;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

public class testFormula {
	private static String folder = "/home/yjy/pag/app-profile/Android-App-Profiling/ml4profiler/"; 
	private static String resultPath = "/home/yjy/workspace_profiler/output/m0/";
	private static String resultFile = "currently_chosen_features.txt";
	private static int expTimes = 50;
	private static List<Integer> featSelected = new ArrayList<Integer>();
	private static String formula;
	
	public static void main(String[] args) {
		runPerls();
//		formula = formula.substring(formula.indexOf("=") + 1);
//		for(int i = 0; i < formula.length(); i++){
//			char ch = formula.charAt(i);
//		}
	}
	
	private static void runPerls(){
		double miniError = 100;
		try {
			Runtime run = Runtime.getRuntime();
			for(int t = 0; t < expTimes; t++){
				Process process = run.exec("octave -qf -p " + folder + "ml/common " + folder
						+ "ml/stable/foba_poly_model_init.m " + resultPath + " 1 9");
				int exitValue = process.waitFor();
				if(exitValue != 0){
					System.out.println("Fatal error");
					return;
				}
				process = run.exec("octave -qf -p " + folder + "ml/common " + folder
						+ "ml/stable/foba_poly_model.m " + resultPath + " 2 9");
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
