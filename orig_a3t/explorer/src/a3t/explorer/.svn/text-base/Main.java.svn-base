package a3t.explorer;

import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.BufferedWriter;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.Set;
import java.util.HashSet;
import java.util.Properties;

public class Main
{
	public static void main(String[] args)
	{
		Config config = Config.g();

        MonkeyScript.setup(config.userWait);
        Executor.setup(config.emulatorPort, 
					   config.appPkgName, 
					   config.mainActivity, 
					   config.activityArgs, 
					   config.divergenceThreshold,
					   config.wildEmusThreshold);
		Z3Task.setup(config.z3Path);
		BlackListedFields.setup(config.fieldSigsFile, config.blackListedFieldsFile);

		Explorer explorer = new Explorer();
		if(config.restart) {
			//Properties props = loadProperties();
			//PathsRepo.restoreState(props);
			//explorer.restoreState(props);
			throw new Error();
		}

		explorer.perform(config.K, config.monkeyScript, config.checkReadOnly, config.checkIndep, config.pruneAfterLastStep);

        // CoverageMonitor.printDangBranches(config.condMapFile);
	}

	public static Properties loadProperties() {
		Properties props = new Properties();
		try{
			props.load(new FileInputStream(newOutFile("a3tstate.txt")));
		}catch(Exception e){
			throw new Error(e);
		}
		return props;
	}
	
	public static void saveProperties(Properties props) {
		try{
			props.store(new FileOutputStream(newOutFile("a3tstate.txt")), "");
		}catch(IOException e){
			throw new Error(e);
		}
	}

	public static File newOutFile(String name) {
		return new File(Config.g().outDir, name);
	}

	public static PrintWriter newWriter(File file) throws IOException {
		return new PrintWriter(new BufferedWriter(new FileWriter(file)));
	}

	public static PrintWriter newWriter(File file, boolean append) throws IOException {
		return new PrintWriter(new BufferedWriter(new FileWriter(file, append)));
	}

	public static PrintWriter newWriter(String name) throws IOException {
		return newWriter(newOutFile(name));
	}

	public static BufferedReader newReader(String name) throws FileNotFoundException {
		return newReader(newOutFile(name));
	}

	public static BufferedReader newReader(File file) throws FileNotFoundException {
		return new BufferedReader(new FileReader(file));
	}
}
