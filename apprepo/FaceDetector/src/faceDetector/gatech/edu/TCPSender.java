package faceDetector.gatech.edu;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.net.InetAddress;
import java.net.Socket;

public class TCPSender extends Thread{
	static int port = 8000;
	FaceDetectorExample mhost;
	
	public TCPSender(FaceDetectorExample host){
		mhost = host;
	}

	private Socket findServer() throws Exception{
		BufferedReader reader = new BufferedReader( 
        		new InputStreamReader( new FileInputStream( "/proc/net/arp" ) ));
        //read the titles
        String load = reader.readLine();
        
        load=reader.readLine();
        String[]  token = load.split(" ");
        String ip  = token[0];

        reader.close();  
        InetAddress ipadd =  InetAddress.getByName(ip);
		return new Socket(ipadd,port);
	}
	
	public void run(){
		long total = 0;
		long count = 0;
		long startTime = System.currentTimeMillis();
		while(mhost.send){
		try{
			Socket socket = findServer();
			DataOutputStream out = new DataOutputStream(socket.getOutputStream());
			BufferedReader reader = new BufferedReader( new InputStreamReader( 
					new FileInputStream( "/sys/class/power_supply/battery/capacity" ) ), 1000 );
			String line = reader.readLine();
			int battery = Integer.parseInt(line);
			reader.close();
			record(System.currentTimeMillis()-startTime, 0, battery);
			
			
			byte[] buffer = new byte[1024*1024];
			while(mhost.send){
				//Thread.sleep(100000);
				out.write(buffer);
				out.flush();
				count++;
				total++;
				if(count >= 2){
					count -= 2;
					long time = System.currentTimeMillis() - startTime; 
					reader = new BufferedReader( new InputStreamReader( 
							new FileInputStream( "/sys/class/power_supply/battery/capacity" ) ), 1000 );
					line = reader.readLine();
					battery = Integer.parseInt(line);
					reader.close();
					record(time,total, battery);
					final String info = time+" "+total+" "+battery;
					mhost.mInfo.post(new Runnable(){public void run(){mhost.mInfo.setText(info);}});
				
				}
			}
			out.close();
			socket.close();
			
		}catch(Exception e){
			e.printStackTrace();
		}
		}
	}
	public static synchronized void record(long time, long count, int battery){
		File f = new File("/sdcard/dcim/record_sys.txt");
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
