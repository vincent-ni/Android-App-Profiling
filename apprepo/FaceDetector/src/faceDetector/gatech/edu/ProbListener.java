package faceDetector.gatech.edu;


import java.io.InterruptedIOException;
import java.net.ServerSocket;
import java.net.Socket;

public class ProbListener extends Thread {
	static int port = 8000;
	ServerSocket listener;
	
	FaceDetectorExample mhost;
	public ProbListener(FaceDetectorExample host){
		mhost = host;
	}

	public void run(){
		try{
			long time = System.currentTimeMillis();
			listener = new ServerSocket(port);
			listener.setSoTimeout(1000);
			Socket client;
			while( mhost.listen){
				try{
					client= listener.accept();
					TCPReceiver rcv = new TCPReceiver(client, mhost,time);
					rcv.start();
				}
				catch(InterruptedIOException e){
					
				}
			}
		}catch(Exception e){
			e.printStackTrace();
		}
	}
}
