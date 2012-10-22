package a3t.explorer;

import java.util.List;
import java.util.ArrayList;
import java.io.PrintWriter;
import java.util.Properties;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

public class PathsRepo
{
	private static final BlockingQueue<Path> allPaths = new LinkedBlockingQueue();
	private static final AtomicInteger globalId = new AtomicInteger(0);

	static int pathsCount()
	{
		return globalId.get();
	}

	static int nextPathId()
	{
		return globalId.getAndIncrement();
	}
	
	static void addPath(Path p)
	{
		allPaths.add(p);
	}
	
	static Path getNextPath()
	{
		//if(allPaths.isEmpty())
		//	return null;
		//return allPaths.remove(0);
		try{
			return allPaths.poll(2L, TimeUnit.SECONDS);
		}catch(InterruptedException e){
			throw new Error(e);
		}
	}

	/*
	static void saveState(Properties props, Path swbPath)
	{
		StringBuilder builder = new StringBuilder();
		builder.append(swbPath.id() + " " + swbPath.seedId() + " " + swbPath.depth() + ",");
		for(Path p : allPaths) {
			builder.append(p.id() + " " + p.seedId() + " " + p.depth() + ",");
		}
		props.setProperty("toexecute", builder.toString());
		
		props.setProperty("globalid", String.valueOf(globalId));
	}
	
	static void restoreState(Properties props)
	{
		globalId = Integer.parseInt(props.getProperty("globalid"));

		for(String toExecute : props.getProperty("toexecute").split(",")) {
			String[] s = toExecute.split(" ");
			allPaths.add(new Path(Integer.parseInt(s[0]), Integer.parseInt(s[1]), Integer.parseInt(s[2])));
		}
	}
	*/
}

