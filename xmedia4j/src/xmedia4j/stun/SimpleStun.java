package xmedia4j.stun;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.Arrays;

import org.apache.log4j.Logger;

import xmedia4j.util.StreamUtil;

public class SimpleStun {
	static Logger logger = Logger.getLogger(SimpleStun.class);
	
//	private int localPort = 0;
//	public SimpleStun(int localPort){
//		this.localPort = localPort;
//	}
	
	public static class StunPacket{
		public int messgeType;
		public int messageLength;
		public byte[] transactionId;
	}
	
	public void bind(String stunHost, int stunPort, int localPort) throws IOException{
		DatagramSocket udpSocket = new DatagramSocket(localPort);
		byte[] dataBuffer = new byte[1500];
		DatagramPacket udpPacket = new DatagramPacket(dataBuffer, 1);
		
		udpPacket.setAddress(InetAddress.getByName(stunHost) );
		udpPacket.setPort(stunPort);
		
		int offset = 0;
		
		int BIND_REQUEST = 0x0001;
		int BIND_RESPONSE_OK = 0x0101;
		
		int msgLen = 0;
		long magicCookie = 0x2112A442L;
		
		StreamUtil.setLong(dataBuffer, BIND_REQUEST, offset, offset+2);// stun_method
		offset += 2;
		
		StreamUtil.setLong(dataBuffer, msgLen, offset, offset+2); // msg_length
		offset += 2;
		
		StreamUtil.setLong(dataBuffer, magicCookie, offset, offset+4); // magic cookie
		offset += 4;
		
		// transaction ID, 96 bit 
		byte[] transactionId = new byte[]{
				(byte)0x63, (byte)0xc7, (byte)0x11, (byte)0x7e, 
				(byte)0x07, (byte)0x14, (byte)0x27, (byte)0x8f, 
				(byte)0x5d, (byte)0xed, (byte)0x32, (byte)0x21
				};
		System.arraycopy(transactionId, 0, dataBuffer, offset, transactionId.length);
		offset += transactionId.length;
		
		int sendLen = offset;
		logger.info("send bytes " + sendLen);
		logger.debug(bytesToHex(dataBuffer, 0, sendLen));
		udpPacket.setLength(sendLen);
		udpSocket.send(udpPacket);
		
		// receive response
		byte[] buf = new byte[1500];
		DatagramPacket pkt = new DatagramPacket(buf, buf.length);
		udpSocket.receive(pkt);
		int recvLen = pkt.getLength();
		logger.info("recv bytes " + recvLen);
		logger.debug(bytesToHex(buf, 0, recvLen));
		
		offset = 0;
		long respMsgType = StreamUtil.getLong(buf, offset, offset+2);
		offset += 2;
		long respMsgLen = StreamUtil.getLong(buf, offset, offset+2);
		offset += 2;
		long respMagic = StreamUtil.getLong(buf, offset, offset+4);
		offset += 4;
		byte[] respTransactionId = new byte[12];
		System.arraycopy(buf, offset, respTransactionId, 0, 12);
		offset += 12;
		
		if(respMsgType != BIND_RESPONSE_OK){
			logger.error("unknown resp msg type " + respMsgType);
			return;
		}
		
		if(respMagic != magicCookie){
			logger.error("unknown resp msg magic " + respMagic);
			return;
		}
		
		if(!Arrays.equals(respTransactionId, transactionId)){
			logger.error("unexpect resp msg transactionId " + Arrays.toString(respTransactionId));
			return;
		}
		
		int length = (int) respMsgLen;
		int attrOffset = offset;
		while (length > 0) {
			
			long attrType = StreamUtil.getLong(buf, offset, offset+2);
			offset += 2;
			long attrLen = StreamUtil.getLong(buf, offset, offset+2);
			offset += 2;
			byte[] attr = new byte[(int) attrLen];
			System.arraycopy(buf, offset, attr, 0, (int) attrLen);
			offset += attrLen;
			
			logger.info("attrType=" + attrType);
			logger.info("attrLen=" + attrLen);
			logger.info("attr=" + bytesToHex(attr));
			
			int bytes = (offset - attrOffset);
			length -= bytes;
		}
		
		
	}
	
	public InetSocketAddress bind(){
		InetSocketAddress sockAddr = new InetSocketAddress("127.0.0.1", 0);
		logger.info("addr: " + sockAddr.getAddress().toString() + ":" + sockAddr.getPort());
		return sockAddr;
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
	
	/*
	 * 
结论1：只要单侧NAT属于Full Cone NAT，即可实现双向通信。 
结论2：只要两侧NAT都不属于Symmetric NAT，也可双向通信。换种说法，只要两侧NAT都属于Cone NAT，即可双向通信
结论3：一侧NAT属于Symmetric NAT，另一侧NAT属于Restricted Cone，也可双向通信。
结论4，两个都是Symmetric NAT或者一个是Symmetric NAT、另一个是Port Restricted Cone，则不能双向通信。
	 */
	
	public static void main(String[] args){
	    String STUN_SERVER = "203.195.185.236";
	    int    STUN_SERVER_PORT = 3488;
	    SimpleStun stun = new SimpleStun();
	    try {
	    	logger.debug("debug simple stun...");
			stun.bind(STUN_SERVER, STUN_SERVER_PORT, 9898);
			stun.bind();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
}
