package faceDetector.gatech.edu;


import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.net.Socket;

public class TCPReceiver extends Thread {
	int DATASIZE = 1024;
	Socket mSocket;
	FaceDetectorExample mhost;
	long startTime;
	
	public TCPReceiver(Socket socket, FaceDetectorExample host, long time){
		mSocket = socket;
		mhost = host;
		startTime = time;
	}
	
	public void run(){
		
		char[] buffer = new char[1460];
		//long startTime = System.currentTimeMillis();
		BufferedReader reader= null;
		
		try{
			//startTime;
			
			reader = new BufferedReader( new InputStreamReader( 
					new FileInputStream( "/sys/class/power_supply/battery/capacity" ) ), 1000 );
			String line = reader.readLine();
			int battery = Integer.parseInt(line);
			reader.close();
			record(System.currentTimeMillis()-startTime, 0, battery);
		
			
			
			BufferedReader in = new BufferedReader(new InputStreamReader(mSocket.getInputStream()));
			
			while(mhost.listen){
				
				int len = in.read(buffer,0,DATASIZE);
				mhost.total += len;
				mhost.count += len;
				if(mhost.count >= 100*1024*1024){
					mhost.count -= 100*1024*1024;
					long time = System.currentTimeMillis() - startTime; 
					reader = new BufferedReader( new InputStreamReader( 
							new FileInputStream( "/sys/class/power_supply/battery/capacity" ) ), 1000 );
					line = reader.readLine();
					battery = Integer.parseInt(line);
					reader.close();
					long size= mhost.total/1024/1024;
					record(time,size, battery);
					final String info = time+" "+size+" "+battery;
					mhost.mInfo.post(new Runnable(){public void run(){mhost.mInfo.setText(info);}});
				
				}
				
			}
			in.close();
			mSocket.close();
		}
		catch(Exception e){}
		
			
	}
	public static synchronized void record(long time, long count, int battery){
		File f = new File("/sdcard/DCIM/record_wifi1.txt");
		try{
			
			if(!f.exists())
				f.createNewFile();
			BufferedWriter wr = new BufferedWriter(new FileWriter(f,true));
			wr.append(time+"\t"+count+"\t"+battery+"\n");
			wr.flush();
			wr.close();
		}catch(Exception e){}
	}

}
