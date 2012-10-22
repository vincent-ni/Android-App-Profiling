package a3t.explorer;

import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Set;
import java.util.Map;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Properties;
import java.util.Iterator;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.File;
import java.io.FilenameFilter;

class PathInfo
{
	private static final String PATHINFO = "pathinfo.";

	Set<Integer> writeSet;
	List<Set<RWRecord>> rwSet;
	boolean endsWithPanelClick;
	int traceLength;
	int numEvents;

	void dump(File file)
	{
		Properties props = new Properties();
		props.setProperty("numevents", String.valueOf(numEvents));
		props.setProperty("tracelength", String.valueOf(traceLength));
		props.setProperty("endswithpanelclick", String.valueOf(endsWithPanelClick));

		StringBuilder builder = new StringBuilder();
		for(Integer fid : writeSet)
			builder.append(fid+" ");
		props.setProperty("writeset", builder.toString());

		builder = new StringBuilder();
		for(Set<RWRecord> rws : rwSet){
			for(RWRecord eid : rws)
				builder.append(eid.id+";"+eid.fldId + " ");
			builder.append(",");
		}
		props.setProperty("rwset", builder.toString());

		try{
			FileOutputStream fos = new FileOutputStream(file);
			props.store(fos, "");
			fos.close();
		}catch(IOException e){
			throw new Error(e);
		}
	}

	static PathInfo load(File file)
	{
		Properties props = new Properties();
		try{
			FileInputStream fis = new FileInputStream(file);
			props.load(fis);
			fis.close();
		}catch(Exception e){
			throw new Error(e);
		}

		int numEvents = Integer.parseInt(props.getProperty("numevents"));
		int traceLength = Integer.parseInt(props.getProperty("tracelength"));
		boolean endsWithPanelClick = Boolean.parseBoolean(props.getProperty("endswithpanelclick"));

		String writeSetStr = props.getProperty("writeset").trim();
		Set<Integer> writeSet = new HashSet();
		if(writeSetStr.length() > 0){
			String[] tokens = writeSetStr.split(" ");
			for(String token : tokens){
				writeSet.add(Integer.parseInt(token));
			}
		}

		String rwSetStr = props.getProperty("rwset").trim();
		List<Set<RWRecord>> rwSet = new ArrayList();
		String[] tokens1 = split(rwSetStr, ',');
		for(int i = 0; i < numEvents; i++){
			Set rws = new HashSet();
			rwSet.add(rws);
			String tk1 = tokens1[i];
			if(tk1.length() == 0)
				continue;
			String[] tokens2 = tk1.split(" ");
			int m = tokens2.length;
			for(int j = 0; j < m; j++){
				String tk2 = tokens2[j];
				if(tk2.length() == 0)
					continue;
				String[] tokens3 = tk2.split(";");
				rws.add(new RWRecord(Integer.parseInt(tokens3[0]), Integer.parseInt(tokens3[1])));
			}
		}

		return new PathInfo(numEvents, traceLength, writeSet, rwSet, endsWithPanelClick);
	}

	private static String[] split(String s, char c) 
	{
		int n = s.length();
		List<String> result = new LinkedList();
		int from = 0;
		while(from < n) {
			int next = s.indexOf(c, from);
			if(next < 0) {
				result.add(s.substring(from));
				break;
			}
			if(next == from) 
				result.add("");
			else 
				result.add(s.substring(from, next));
			from = next + 1;
		}
		return result.toArray(new String[0]);
	}

	PathInfo(int numEvents,
			 int traceLength, 
			 Set<Integer> writeSet,
			 List<Set<RWRecord>> rwSet,
			 boolean endsWithPanelClick)
	{
		this.writeSet = writeSet;
		this.rwSet = rwSet;
		this.endsWithPanelClick = endsWithPanelClick;
		this.traceLength = traceLength;
		this.numEvents = numEvents;
	}

	static String fileName(int id) {
		return PATHINFO+id;
	}

}
