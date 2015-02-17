package xmedia4j.stun;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Random;
import java.util.UUID;

import org.apache.log4j.Logger;

import xmedia4j.stun.SuperPuncher.P2PNATAddress;
import xmedia4j.util.StreamUtil;

import com.fasterxml.jackson.annotation.JsonInclude.Include;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.ObjectMapper;

public class SimpleSignalServer {
	private static final Logger logger = Logger.getLogger(SimpleSignalServer.class.getSimpleName());

	protected static final String CMD_REG = "reg";
	protected static final String CMD_HEART = "heart";
	protected static final String CMD_SIG = "sig";
	protected static final String CMD_PUN = "pun";
	protected static final String CMD_MSG = "msg";
	
	public static class BeanRequest{
		public String cmd;
		public String param;
		public long seq = new Random().nextLong();
		public int errCode = 0;
		
	}
	
	public static class BeanResponse extends BeanRequest{
		public BeanResponse(){
			
		}
		public BeanResponse(BeanRequest req){
			this.cmd = req.cmd;
			this.param = req.param;
			this.seq = req.seq;
		}
	}
	
	public static class BeanPeer{
		public String name;
		public String regId = UUID.randomUUID().toString();
		public String publicIp;
		public int publicPort;
		public long activeTime;
		public List<P2PNATAddress> natAddrList;
	}
	
	public static class BeanSig{
		public String otherPeerName;
		public BeanPeer thisPeer;
		public String punId = UUID.randomUUID().toString();
	}
	
	private ObjectMapper objectMapper = new ObjectMapper();	
	private Map<String, BeanPeer> regUsers = new HashMap<String, BeanPeer>();
	public SimpleSignalServer(){
		objectMapper.configure(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES, false);
    	objectMapper.setSerializationInclusion(Include.NON_NULL);
	}
	
	private BeanRequest parseRequest(byte[] data, int offset, int len){
		try{
			String reqJson = new String(data, offset, len);
			logger.debug("reqJson=" + reqJson);
			BeanRequest req = objectMapper.readValue(reqJson, BeanRequest.class);
			return req;
		}catch(Exception e){
			e.printStackTrace();
			logger.error("parse request fail!");
			return null;
		}
	}
	
	private DatagramPacket buildResponsePacket(DatagramPacket pkt, BeanResponse rsp) throws JsonProcessingException{
		String respJson = objectMapper.writeValueAsString(rsp);
		byte[] data = respJson.getBytes();
		DatagramPacket rspPkt = new DatagramPacket(data, data.length);
		rspPkt.setLength(data.length);
		rspPkt.setSocketAddress(pkt.getSocketAddress());
		return rspPkt;
	}
	
	private DatagramPacket handleReg(DatagramPacket pkt, BeanRequest req) throws IOException{
		logger.info("reg from " + pkt.getAddress().getHostAddress() + ":" + pkt.getPort());
		BeanPeer peer = objectMapper.readValue(req.param, BeanPeer.class);
		peer.activeTime = System.currentTimeMillis();
		if(peer.publicIp == null){
			peer.publicIp = pkt.getAddress().getHostAddress();
			peer.publicPort = pkt.getPort();
		}
		if(peer.name == null){
			peer.name = peer.publicIp + ":" + peer.publicPort;
		}
		
		synchronized (regUsers) {
			regUsers.put(peer.name, peer);
			
			Map<String, BeanPeer> respPeers = new HashMap<String, BeanPeer>();
			Iterator<Entry<String, BeanPeer>> iterator = regUsers.entrySet().iterator();
			while (iterator.hasNext()) {
				BeanPeer u = iterator.next().getValue();
				if((System.currentTimeMillis() - u.activeTime) > 3*1000){
					logger.info("user [" + u.name + "] expired");
					iterator.remove();
					regUsers.remove(u.name);
				}else{
					respPeers.put(u.name, u);
				}
			}
			
//			for(Entry<String, BeanPeer> entry : regUsers.entrySet()){
//				BeanPeer u = entry.getValue();
//				if((System.currentTimeMillis() - u.activeTime) > 60*1000){
//					logger.info("user [" + u.name + "] expired");
//					regUsers.remove(u.name);
//				}else{
//					respPeers.put(u.name, u);
//				}
//			}
			
			BeanResponse rsp = new BeanResponse(req);
			rsp.param = objectMapper.writeValueAsString(respPeers);
			return buildResponsePacket(pkt, rsp);
		}
	}
	
	private DatagramPacket handleHeart(DatagramPacket pkt, BeanRequest req) throws IOException{
		logger.debug("heart from " + pkt.getAddress().getHostAddress() + ":" + pkt.getPort());
		String name = req.param;
		
		synchronized (regUsers) {
			BeanPeer peer = regUsers.get(name);
			BeanResponse rsp = new BeanResponse(req);
			if(peer == null){
				rsp.errCode = -1;
			}else{
				peer.activeTime = System.currentTimeMillis();
			}
			rsp.param = req.param;
			
			return buildResponsePacket(pkt, rsp);
		}
	}
	
	
	
	private DatagramPacket handleSig(DatagramPacket pkt, BeanRequest req) throws IOException{
		logger.info("sig from " + pkt.getAddress().getHostAddress() + ":" + pkt.getPort());
		BeanSig bean = objectMapper.readValue(req.param, BeanSig.class);
		synchronized (regUsers) {
			BeanPeer peer = regUsers.get(bean.otherPeerName);
			if(peer == null){
				logger.error("NOT found sig name " + bean.otherPeerName);
				BeanResponse rsp = new BeanResponse(req);
				rsp.errCode = -1;
				return buildResponsePacket(pkt, rsp);
			}
			
			pkt.setSocketAddress(new InetSocketAddress(peer.publicIp, peer.publicPort));
			logger.info("sig to   " + pkt.getAddress().getHostAddress() + ":" + pkt.getPort());
			return pkt;
		}
	}
	
	private DatagramPacket handleRequest(DatagramPacket pkt) throws IOException{
		BeanRequest req = parseRequest(pkt.getData(), 0, pkt.getLength());
		if(req == null) return null;
		
		if(req.cmd.equals(CMD_REG)){
			return handleReg(pkt, req);
		}else if(req.cmd.equals(CMD_HEART)){
			return handleHeart(pkt, req);
		}else if (req.cmd.equals(CMD_SIG)){
			return handleSig(pkt, req);
		}else{
			logger.error("unknown req " + req.cmd);
			return null;
		}
	}
	
	
	public void serve(int localPort) throws IOException{

		final DatagramSocket udpSocket = new DatagramSocket(localPort);
		logger.info("serve at " + udpSocket.getLocalSocketAddress()); 
		
		try{
			udpSocket.setSoTimeout(1000);
			DatagramPacket pkt = new DatagramPacket(new byte[1600], 1600);
			while(true){
				try{
					udpSocket.receive(pkt);
				}catch(SocketTimeoutException e){
					continue;
				}
				
				logger.debug("got from " + pkt.getSocketAddress()
						+ ", bytes " + pkt.getLength()
						+ ", [" + StreamUtil.bytesToHex(pkt.getData(), 0, pkt.getLength(), 16) + "]");
				DatagramPacket rspPkt = handleRequest(pkt);
				if(rspPkt != null){
					udpSocket.send(rspPkt);
				}
			}
		}finally{
			udpSocket.close();
		}
	}
	
	
	public static void main(String args[]) throws IOException {
		SimpleSignalServer serv = new SimpleSignalServer();
		serv.serve(7711);
	}

}
