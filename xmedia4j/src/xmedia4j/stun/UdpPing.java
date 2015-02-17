package xmedia4j.stun;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;

import xmedia4j.util.StreamUtil;

public class UdpPing {
	
	private static void info(String msg){
		System.out.println(msg);
	}
	
	private static void printUsage(){
		info("Usage:");
		info("    ping targIp targPort [localPort]");
		info("    echo localPort");
	}
	
	
	private static void recvLoop(DatagramSocket udpSocket) throws IOException{
		DatagramPacket pkt = new DatagramPacket(new byte[1600], 1600);
		while(true){
			udpSocket.receive(pkt);
			info("got from " + pkt.getSocketAddress()
					+ ", bytes " + pkt.getLength()
					+ ", [" + StreamUtil.bytesToHex(pkt.getData(), 0, pkt.getLength(), 16) + "]");
		}
	}
	
	public static void ping(String targetIp, int targetPort, int localPort) throws IOException, InterruptedException{

		final DatagramSocket udpSocket = new DatagramSocket(localPort);
		info("ping " + targetIp + ":" + targetPort + " at port " + localPort); 
		
		Thread recvThread = new Thread(new Runnable() {
			
			@Override
			public void run() {
				try {
					recvLoop(udpSocket);
				} catch (IOException e) {
					e.printStackTrace();
				}
				
			}
		});
		recvThread.start();
		
		try{
			DatagramPacket pkt = new DatagramPacket(new byte[1600], 1600);
			
			int n = 0;
			while(true){
				String msg = "hello " + (++n);
				info("send msg: " + msg);
				byte[] data = msg.getBytes();
				pkt.setData(data);
				pkt.setSocketAddress(new InetSocketAddress(targetIp, targetPort));
				udpSocket.send(pkt);
				Thread.sleep(1000);
			}
		}finally{
			recvThread.join();
			udpSocket.close();
		}
	}
	
	public static void ping(String args[]) throws IOException, InterruptedException {
		if(args.length < 3){
			printUsage();
			return;
		}
		
		String targetIp = args[1];
		int targetPort = Integer.valueOf(args[2]);
		int localPort = 0;
		if(args.length >= 4){
			localPort = Integer.valueOf(args[3]);
		}
		ping(targetIp, targetPort, localPort);
	}
	
	public static void echo(String args[]) throws IOException {
		if(args.length < 2){
			printUsage();
			return;
		}
		
		int localPort = Integer.valueOf(args[1]);
		info("echo at local port " + localPort);
		DatagramSocket udpSocket = new DatagramSocket(localPort);
		try{
			DatagramPacket pkt = new DatagramPacket(new byte[1600], 1600);
			while(true){
				udpSocket.receive(pkt);
				info("got from " + pkt.getSocketAddress()
						+ ", bytes " + pkt.getLength()
						+ ", [" + StreamUtil.bytesToHex(pkt.getData(), 0, pkt.getLength(), 16) + "]");
				udpSocket.send(pkt); // echo it back
			}
		}finally{
			udpSocket.close();
		}
	}
	
	public static void main(String args[]) throws IOException, InterruptedException {
		if(args.length < 1){
//			args = new String[]{"ping", "127.0.0.1", "9222", "9222" };
			printUsage();
			return;
		}
		
		String cmd = args[0];
		if(cmd.equalsIgnoreCase("ping")){
			ping(args);
		}else if(cmd.equalsIgnoreCase("echo")){
			echo(args);
		}else{
			printUsage();
		}
	}
}
