package xmedia.p2p.android;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.SocketAddress;
import java.net.SocketTimeoutException;
import java.util.Enumeration;
import java.util.logging.Level;
import java.util.logging.Logger;

import me.myandroid.LogTextView;
import android.content.Context;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.support.v7.app.ActionBarActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import de.javawi.jstun.attribute.ChangeRequest;
import de.javawi.jstun.attribute.ChangedAddress;
import de.javawi.jstun.attribute.ErrorCode;
import de.javawi.jstun.attribute.MappedAddress;
import de.javawi.jstun.attribute.MessageAttribute;
import de.javawi.jstun.attribute.MessageAttributeParsingException;
import de.javawi.jstun.header.MessageHeader;
import de.javawi.jstun.header.MessageHeaderParsingException;
import de.javawi.jstun.test.DiscoveryInfo;
import de.javawi.jstun.test.DiscoveryTest;
import de.javawi.jstun.test.demo.DiscoveryTestDemo;
import de.javawi.jstun.util.UtilityException;

public class MainActivity extends ActionBarActivity {

	private static final String BTNTEXT_START = "开始";
	private static final String BTNTEXT_STOP = "停止";
	
	LogTextView logtv = null;
	boolean stopRequest = false;
	
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		Button btnControl = (Button)findViewById(R.id.ButtonControl);
        btnControl.setOnClickListener(new OnClickListener() {
			
			@Override
			public void onClick(View v) {
				final Button btnCtrl = (Button)findViewById(R.id.ButtonControl);
				
				if(btnCtrl.getText().equals(BTNTEXT_START))
				{
					startTest();
				}
				else
				{
					stopTest();
				}
				
			}
		});
        btnControl.setText(BTNTEXT_START);
        btnControl.setEnabled(true);
        
		logtv = new LogTextView((TextView)findViewById(R.id.TextInfo),   (ScrollView) findViewById(R.id.scrollview) );
		
	}
	
	
	public void startTest()
	{
		final Button btnCtrl = (Button)findViewById(R.id.ButtonControl);
		synchronized(MainActivity.this) {stopRequest = false;}
		
        btnCtrl.setText(BTNTEXT_STOP);
		//btnControl.setEnabled(false);
		
        Thread thread=new Thread(new Runnable()
        {
        	@Override
			public void run()
        	{
        		PowerManager pm = (PowerManager)getSystemService(Context.POWER_SERVICE);  
	        	//WakeLock wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "CryptorTesterWakeLock");
        		WakeLock wakeLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK, "CryptorTesterWakeLock");
	        	wakeLock.acquire();
	        	try
	        	{
	        		runtest();
	        	} catch (Exception e) {
					e.printStackTrace();
					logtv.appendln(e.getMessage());
				} 
	        	finally
	        	{
	        		btnCtrl.post(new Runnable() {
						@Override
						public void run() {
							btnCtrl.setText(BTNTEXT_START);
							btnCtrl.setEnabled(true);
						}
						
					});
	        		
	        		wakeLock.release();
	        	}
        	}
			
        	void runtest() throws Exception{
        		logtv.appendln("testing...");
        		
        		try {
        			testNAT();
        		} catch (Exception e) {
        			e.printStackTrace();
        			logtv.appendln(e.getMessage());
        		}
        		
        		logtv.appendln("test done");
        	}
        	
        });
        thread.start();
	}
	
	
	public void stopTest()
	{
		//btnControl.setText(BTNTEXT_START);
		synchronized(MainActivity.this) {stopRequest = true;}
	}
	
	public void testNAT()throws Exception{
//		Handler fh = new FileHandler("logging.txt");
//		fh.setFormatter(new SimpleFormatter());
//		Logger.getLogger("de.javawi.jstun").addHandler(fh);
		Logger.getLogger("de.javawi.jstun").setLevel(Level.ALL);
		
		Enumeration<NetworkInterface> ifaces = NetworkInterface.getNetworkInterfaces();
		while (ifaces.hasMoreElements()) {
			NetworkInterface iface = ifaces.nextElement();
			Enumeration<InetAddress> iaddresses = iface.getInetAddresses();
			while (iaddresses.hasMoreElements()) {
				InetAddress iaddress = iaddresses.nextElement();
				if (Class.forName("java.net.Inet4Address").isInstance(iaddress)) {
					if ((!iaddress.isLoopbackAddress()) && (!iaddress.isLinkLocalAddress())) {
						logtv.appendln("try net: " + iaddress.getHostAddress());
						//testNATType(iaddress);
						testPublicAddress(iaddress);
					}
				}
			}
		}
	}
	
	protected void info(String msg){
		logtv.appendln(msg);
	}
	
	final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();
	public static String bytesToHex(byte[] bytes) {
		return bytesToHex(bytes, 0, bytes.length);
	}
	public static String bytesToHex(byte[] bytes, int offset, int len) {
	    char[] hexChars = new char[len * 2];
	    for ( int j = 0; j < len; j++ ) {
	    	int off = offset + j;
	        int v = bytes[off] & 0xFF;
	        hexChars[j * 2] = hexArray[v >>> 4];
	        hexChars[j * 2 + 1] = hexArray[v & 0x0F];
	    }
	    return new String(hexChars);
	}
	
	protected InetSocketAddress[] findPublicAddress(InetSocketAddress localAddr, InetSocketAddress[] stunAddrs, long timeoutMilli) throws UtilityException, IOException, MessageHeaderParsingException, MessageAttributeParsingException {
		
		DatagramSocket udpSocket = new DatagramSocket(localAddr);
		int recvTimeout = 300;
		try{
			// configure UDP socket
			udpSocket.setReuseAddress(true);
			//udpSocket.connect(InetAddress.getByName(stunServer), port);
			udpSocket.setSoTimeout(recvTimeout);
			
			
			
			// init 
			InetSocketAddress[] publicSockAddrs = new InetSocketAddress[stunAddrs.length];
			DatagramPacket[] udpPkts = new DatagramPacket[stunAddrs.length];
			MessageHeader[] sendMHs = new MessageHeader[stunAddrs.length];
			for(int i = 0; i < stunAddrs.length; i++){
				
				// build data
				sendMHs[i] = new MessageHeader(MessageHeader.MessageHeaderType.BindingRequest);
				sendMHs[i].generateTransactionID();
				ChangeRequest changeRequest = new ChangeRequest();
				sendMHs[i].addMessageAttribute(changeRequest);
				byte[] data = sendMHs[i].getBytes();
				info("send data: " + bytesToHex(data));
				
				publicSockAddrs[i] = null;
				udpPkts[i] = new DatagramPacket(data, data.length);
				udpPkts[i].setSocketAddress(stunAddrs[i]);
			}
			
			MessageHeader receiveMH = new MessageHeader();
			DatagramPacket recvPkt = new DatagramPacket(new byte[1200], 1200);
			long startMilli = System.currentTimeMillis();
			while(true){
				
				// send packet if NOT success
				boolean done = true;
				for(int i = 0; i < stunAddrs.length; i++){
					if(publicSockAddrs[i]== null){
						info("send to [" + udpPkts[i].getSocketAddress().toString()
								+ "], data[" + bytesToHex(udpPkts[i].getData()) + "]" );
						udpSocket.send(udpPkts[i]);
						done = false;
					}
				}
				if(done) break;
				
				
				try{
					while ((System.currentTimeMillis() - startMilli) < timeoutMilli) {
						done = false;
						udpSocket.receive(recvPkt);
						info("got pkt bytes " + recvPkt.getLength());
						
						receiveMH = MessageHeader.parseHeader(recvPkt.getData());
						receiveMH.parseAttributes(recvPkt.getData());
						done = true;
						for(int i = 0; i < stunAddrs.length; i++){
							if(receiveMH.equalTransactionID(sendMHs[i])){
								info("recv from [" + udpPkts[i].getSocketAddress().toString() + "]"
										+ ", bytes[" + recvPkt.getLength() + "]"
										+ ", data[" + bytesToHex(recvPkt.getData(), 0, recvPkt.getLength()) + "]" );
								
								
								MappedAddress ma = (MappedAddress) receiveMH.getMessageAttribute(MessageAttribute.MessageAttributeType.MappedAddress);
								if(ma != null){
									info("got public addr: " + ma.getAddress().getInetAddress().getHostAddress() + ":" + ma.getPort());
									publicSockAddrs[i] = new InetSocketAddress(ma.getAddress().getInetAddress(), ma.getPort());
								}
							}
							if(publicSockAddrs[i] == null) done = false;
						}
						if(done) break;
						
					}
				}catch(SocketTimeoutException e){
					info("socket timeout");
				}
				
				if((System.currentTimeMillis() - startMilli) >= timeoutMilli){
					info("reach timeout " + timeoutMilli);
					break;
				}
			}
			
			return publicSockAddrs;
		}finally{
			udpSocket.close();
		}
		
		
		
	}
	
	protected boolean testPublicAddress(InetAddress localIp) throws Exception{
		InetSocketAddress[] stunAddrs = new InetSocketAddress[]{
				new InetSocketAddress("203.195.185.236", 3488), // turn1
				new InetSocketAddress("121.41.75.10", 3488), // turn3
				//new InetSocketAddress("jstun.javawi.de", 3478),
				};
		
		DatagramSocket tempSocket = new DatagramSocket(new InetSocketAddress(localIp, 0));
		InetSocketAddress localAddr = new InetSocketAddress(tempSocket.getLocalAddress(), tempSocket.getLocalPort());
		tempSocket.close();
		InetSocketAddress[] publicAddrs = findPublicAddress(localAddr, stunAddrs, 3000);
		if(publicAddrs == null){
			return false;
		}
		
		
		info("=========>");
		info("local address :" + localAddr);
		for(InetSocketAddress addr : publicAddrs){
			info("public address: " + addr);
		}
		info("<=========");
		return true;
	}
	protected boolean testPublicAddress0(InetAddress iaddress) throws Exception{
		String stunServer = "203.195.185.236";
	    int    port = 3488;
		
		
		DiscoveryInfo di = new DiscoveryInfo(iaddress);;
		MappedAddress ma = null;
		ChangedAddress ca = null;
		
		info("test...");
		DatagramSocket udpSocket = new DatagramSocket(new InetSocketAddress(iaddress, 0));
		
		int tryNum = 0;
		while(true){
			try{
				int timeout = 300; 
				udpSocket.setReuseAddress(true);
				//udpSocket.connect(InetAddress.getByName(stunServer), port);
				udpSocket.setSoTimeout(timeout);
				
				MessageHeader sendMH = new MessageHeader(MessageHeader.MessageHeaderType.BindingRequest);
				sendMH.generateTransactionID();
				
//				ChangeRequest changeRequest = new ChangeRequest();
//				sendMH.addMessageAttribute(changeRequest);
				
				byte[] data = sendMH.getBytes();
				DatagramPacket send = new DatagramPacket(data, data.length);
				send.setSocketAddress(new InetSocketAddress(stunServer, port));
				udpSocket.send(send);
				info("Test 1: Binding Request sent.");
				info(bytesToHex(data));
			
				MessageHeader receiveMH = new MessageHeader();
				while (!(receiveMH.equalTransactionID(sendMH))) {
					DatagramPacket receive = new DatagramPacket(new byte[1200], 1200);
					
					udpSocket.receive(receive);
//					try{
//						socketTest1.receive(receive);
//					}catch(SocketTimeoutException e){
//						info("socket timeout");
//						continue; 
//					}
					
					
					info("Test 1: recv bytes " + receive.getLength());
					info(bytesToHex(receive.getData(), 0, receive.getLength()));
					receiveMH = MessageHeader.parseHeader(receive.getData());
					receiveMH.parseAttributes(receive.getData());
				}
				
				ma = (MappedAddress) receiveMH.getMessageAttribute(MessageAttribute.MessageAttributeType.MappedAddress);
				ca = (ChangedAddress) receiveMH.getMessageAttribute(MessageAttribute.MessageAttributeType.ChangedAddress);
				ErrorCode ec = (ErrorCode) receiveMH.getMessageAttribute(MessageAttribute.MessageAttributeType.ErrorCode);
				if (ec != null) {
					di.setError(ec.getResponseCode(), ec.getReason());
					info("Message header contains an Errorcode message attribute.");
					return false;
				}
				if ((ma == null) || (ca == null)) {
					di.setError(700, "The server is sending an incomplete response (Mapped Address and Changed Address message attributes are missing). The client should not retry.");
					info("Response does not contain a Mapped Address or Changed Address message attribute.");
					info(di.toString());
					return false;
				} else {
					di.setPublicIP(ma.getAddress().getInetAddress());
					if ((ma.getPort() == udpSocket.getLocalPort()) && (ma.getAddress().getInetAddress().equals(udpSocket.getLocalAddress()))) {
						info("Node is not natted.");
						//nodeNatted = false;
					} else {
						info("Node is natted.");
					}
					info(di.toString());
					return true;
				}
			}catch (SocketTimeoutException ste){
				tryNum++;
				if(tryNum > 10) return false;
			}
			
		}
		
		
	}
	
	protected void testNATType(InetAddress iaddress)throws Exception{
		new DiscoveryTestDemo(iaddress).run();
		DiscoveryTest test = new DiscoveryTest(iaddress, "jstun.javawi.de", 3478);
		logtv.appendln(test.test().toString());
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		if (id == R.id.action_settings) {
			return true;
		}
		return super.onOptionsItemSelected(item);
	}
}
