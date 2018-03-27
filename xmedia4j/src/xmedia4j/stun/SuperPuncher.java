package xmedia4j.stun;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;

import org.apache.log4j.Logger;

import xmedia4j.util.StreamUtil;
import de.javawi.jstun.attribute.ChangeRequest;
import de.javawi.jstun.attribute.MappedAddress;
import de.javawi.jstun.attribute.MessageAttribute;
import de.javawi.jstun.attribute.MessageAttributeParsingException;
import de.javawi.jstun.header.MessageHeader;
import de.javawi.jstun.header.MessageHeaderParsingException;
import de.javawi.jstun.util.UtilityException;

public class SuperPuncher {
	private static final Logger logger = Logger.getLogger(SuperPuncher.class.getSimpleName());
	
	public static class P2PNATAddress{
		private InetSocketAddress localAddress = null;
		private InetSocketAddress[] publicAddresses = null;
		private boolean isCone = false;
		
		public P2PNATAddress(InetSocketAddress localAddr,
				InetSocketAddress[] publicAddrs) {
			super();
			this.localAddress = localAddr;
			this.publicAddresses = publicAddrs;
			
			if(this.publicAddresses != null){
				InetSocketAddress sa = null;
				boolean cone = true;
				for(InetSocketAddress pub : this.publicAddresses){
					if(sa == null){
						sa = pub;
					}else{
						if(!sa.equals(pub)){
							cone = false;
						}
					}
				}
				this.isCone = cone;
			}
		}

		public InetSocketAddress getLocal() {
			return localAddress;
		}

		public InetSocketAddress[] getPublic() {
			return publicAddresses;
		}

		public boolean isCone() {
			return isCone;
		}
	}
	
	public static class P2PAddress{
		public enum PROTO{UDP, TCP};
		
		public PROTO proto = PROTO.UDP; 
	}
	
	protected static void info(String msg){
		logger.info(msg);
	}
	
	
	
	public InetSocketAddress[] findPublicAddress(InetSocketAddress localAddr, InetSocketAddress[] stunAddrs, long timeoutMilli) throws UtilityException, IOException, MessageHeaderParsingException, MessageAttributeParsingException {
		
//		DatagramSocket udpSocket = new DatagramSocket(localAddr);
		final DatagramSocket udpSocket = new DatagramSocket(null);
		udpSocket.setReuseAddress(true);
		udpSocket.bind(localAddr);
		
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
				info("send data: " + StreamUtil.bytesToHex(data));
				
				publicSockAddrs[i] = null;
				udpPkts[i] = new DatagramPacket(data, data.length);
				udpPkts[i].setSocketAddress(stunAddrs[i]);
			}
			
			
			DatagramPacket recvPkt = new DatagramPacket(new byte[1200], 1200);
			long startMilli = System.currentTimeMillis();
			while(true){
				
				// send packet if NOT success
				boolean done = true;
				for(int i = 0; i < stunAddrs.length; i++){
					if(publicSockAddrs[i]== null){
						info("send to [" + udpPkts[i].getSocketAddress().toString()
								+ "], data[" + StreamUtil.bytesToHex(udpPkts[i].getData()) + "]" );
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
						
						MessageHeader receiveMH = new MessageHeader();
						receiveMH = MessageHeader.parseHeader(recvPkt.getData());
						receiveMH.parseAttributes(recvPkt.getData());
						done = true;
						for(int i = 0; i < stunAddrs.length; i++){
							if(receiveMH.equalTransactionID(sendMHs[i])){
								info("recv from [" + udpPkts[i].getSocketAddress().toString() + "]"
										+ ", bytes[" + recvPkt.getLength() + "]"
										+ ", data[" + StreamUtil.bytesToHex(recvPkt.getData(), 0, recvPkt.getLength()) + "]" );
								
								
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
	
	public List<InetAddress> getLocalNetAddrList() throws ClassNotFoundException, SocketException{
		List<InetAddress> addrList = new ArrayList<InetAddress>();
		Enumeration<NetworkInterface> ifaces = NetworkInterface.getNetworkInterfaces();
		while (ifaces.hasMoreElements()) {
			NetworkInterface iface = ifaces.nextElement();
			Enumeration<InetAddress> iaddresses = iface.getInetAddresses();
			while (iaddresses.hasMoreElements()) {
				InetAddress iaddress = iaddresses.nextElement();
				if (Class.forName("java.net.Inet4Address").isInstance(iaddress)) {
					if ((!iaddress.isLoopbackAddress()) && (!iaddress.isLinkLocalAddress())) {
						addrList.add(iaddress);
					}
				}
			}
		}
		return addrList;
	}
	
	public List<P2PNATAddress> findNATAddress(int localPort, List<InetAddress> localAddrList, InetSocketAddress[] stunAddrs){
		info("find NAT address...");
		List<P2PNATAddress> infoList = new ArrayList<P2PNATAddress>();
		for(InetAddress addr : localAddrList){
			InetSocketAddress localAddr = new InetSocketAddress(addr, localPort);
			info("===== local " + localAddr.getAddress().getHostAddress() + ":" + localAddr.getPort() + " =====");
			try{
				InetSocketAddress[] publicAddrs = this.findPublicAddress(localAddr, stunAddrs, 3000);
				if(publicAddrs == null){
					info("get public address fail!!!");
					continue ;
				}
				P2PNATAddress natAddr = new P2PNATAddress(localAddr, publicAddrs);
				infoList.add(natAddr);
			}catch(Exception e){
				e.printStackTrace();
			}
	    }
		info("find NAT address done");
		return infoList;
	}
	
	private static boolean matchLocal(String addr, String[] localAddrPrefix){
		if(localAddrPrefix == null || localAddrPrefix.length == 0) return true;
		for(String f : localAddrPrefix){
    		if(addr.startsWith(f)) return true;
    	}
		return false;
	}
	
	int selectLocalPort = 0;
	public int getLocalPort(){
		return this.selectLocalPort;
	}
	
	public void detectNAT(){
		try {
		    InetSocketAddress[] stunAddrs = new InetSocketAddress[]{
		    		new InetSocketAddress("121.41.105.183", 3478), // turn1
//					new InetSocketAddress("203.195.185.236", 3488), // turn1
//					new InetSocketAddress("121.41.75.10", 3488), // turn3
//					new InetSocketAddress("127.0.0.1", 3478), // 
					//new InetSocketAddress("jstun.javawi.de", 3478),
//		    		new InetSocketAddress("52.8.75.23", 3478), // els
					};
		    String[] localAddrPrefix = new String[]{
//		    		"192.168.0.",
//		    		"192.168.1.",
//		    		"10.0.1.",
//		    		"172.16"
		    };
		    
		    List<InetAddress> localAddrList = new ArrayList<InetAddress>();
		    
		    for(InetAddress addr : this.getLocalNetAddrList()){
		    	if(!matchLocal(addr.getHostAddress(), localAddrPrefix)){
		    		continue;
		    	}
		    	localAddrList.add(addr);
		    }
		    
		    int localPort = 9111;
		    List<P2PNATAddress> infoList = findNATAddress(localPort, localAddrList, stunAddrs);
		    for(P2PNATAddress natAddr : infoList){
		    	info("--->");
				info("local address :" + natAddr.getLocal());
				for(InetSocketAddress pub : natAddr.getPublic()){
					info("public address: " + pub);
				}
				info("NAT type: " + (natAddr.isCone ? "cone" : "symmetric"));
				info("<---");
		    }
		    this.selectLocalPort = localPort;
		    
		    //UdpPing.ping("123.125.162.110", 9111, localPort);
		    //UdpPing.ping("127.0.0.1", 9111, localPort);
			
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	public static void main(String args[]) {
		SuperPuncher pun = new SuperPuncher();
		pun.detectNAT();
	}
}
