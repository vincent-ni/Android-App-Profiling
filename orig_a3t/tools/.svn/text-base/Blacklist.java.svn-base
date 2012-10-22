
import java.io.*;
import java.util.*;

public class Blacklist
{
	public static void main(String[] args)
	{
		String dirName = args[0];
		String sigsFileName = args[1];//System.getenv().get("A3T_DIR")+File.separator+"a3t_sdk"+File.separator+"fieldsigs.txt";
		String readOnlyRunIds = Undispatched.process(dirName);
		//System.out.println(readOnlyRunIds);
		WriteSet.main(new String[]{dirName, sigsFileName, readOnlyRunIds});
	}
}