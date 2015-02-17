package xmedia4j.stun;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.net.MulticastSocket;
import java.net.SocketTimeoutException;
import java.util.Map;
import java.util.Map.Entry;

import org.apache.log4j.Logger;

import xmedia4j.stun.SimpleSignalServer.BeanPeer;
import xmedia4j.stun.SimpleSignalServer.BeanRequest;
import xmedia4j.stun.SimpleSignalServer.BeanResponse;
import xmedia4j.stun.SimpleSignalServer.BeanSig;

import com.fasterxml.jackson.annotation.JsonInclude.Include;
import com.fasterxml.jackson.core.JsonParseException;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.JsonMappingException;
import com.fasterxml.jackson.databind.ObjectMapper;

public class SimpleSignalClient {

	private static final Logger logger = Logger.getLogger(SimpleSignalClient.class.getSimpleName());

	
	private ObjectMapper objectMapper = new ObjectMapper();	
	private String servHost;
	private int servPort;
	private int localPort;
	private DatagramSocket udpSocket = null;
	private DatagramPacket rspPkt;
	private Map<String, BeanPeer> peers = null;
	private BeanPeer selfP = null;
	private BeanSig otherSig = null;
	
	public SimpleSignalClient(String servHost, int servPort, int localPort){
		this.servHost = servHost;
		this.servPort = servPort;
		this.localPort = localPort;
		
		rspPkt = new DatagramPacket(new byte[2048], 2048);
		objectMapper.configure(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES, false);
    	objectMapper.setSerializationInclusion(Include.NON_NULL);
	}
	
	public void start() throws IOException, InterruptedException{
		if(this.udpSocket == null){
			MulticastSocket msock = new MulticastSocket(localPort);
			logger.info("old TTL = " + msock.getTimeToLive());
			//msock.setTimeToLive(0);
			logger.info("new TTL = " + msock.getTimeToLive());
			this.udpSocket = msock; 
//			this.udpSocket = new DatagramSocket(localPort);
			this.udpSocket.setReuseAddress(true);
			logger.info("create udp socket at port " + this.udpSocket.getLocalPort());
		}
		
		new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					workLoop();
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		}).start();
		
		while(true){
			synchronized (this) {
				this.wait();
				if(this.selfP != null) break;
			}
		}
		
	}
	
	private String getName(){
		synchronized (this) {
			return selfP.name;
		}
	}
	
	
	private void doSig(String peerName) throws IOException{
		logger.info("send sig to " + peerName);
		BeanRequest req = buildRequestSig(peerName);
		BeanResponse rsp = doRequest(req);
		logger.info("got sig ack");
		
		this.otherSig = objectMapper.readValue(rsp.param, BeanSig.class);
		
	}
	
	private void doPunch() throws IOException{
		BeanRequest req = buildRequestPun(this.otherSig);
		BeanResponse rsp = doRequest(req);
		logger.info("got pun ack");
	}
	
	private BeanResponse doRequest(BeanRequest req) throws IOException{
		while(true){
			logger.info("send req: " + req.cmd);
			sendRequest(req);
			
			try{
				BeanResponse rsp = recvResponse();
				while(rsp.seq != req.seq){
					rsp = recvResponse();
				}
				
				if(rsp.errCode < 0){
					logger.error(req.cmd + " response err " + rsp.errCode);
					throw new IOException(req.cmd + " response err " + rsp.errCode);
				}
				
				logger.info("got rsp: " + rsp.cmd);
				return rsp;
				
			}catch(SocketTimeoutException e){
				logger.info("recv timeout: " + req.cmd);
				continue;
			}
			
		}
	}
	
	
	private long lastSentPunTime = 0;
	private void constSendPun(DatagramPacket punPkt) throws IOException{
		if(punPkt != null){
			if((System.currentTimeMillis() - lastSentPunTime) >= 1000){
				//logger.info("send pun to " + punPkt.getAddress().getHostAddress() + ":" + punPkt.getPort());
				this.udpSocket.send(punPkt);
				lastSentPunTime = System.currentTimeMillis();
			}
		}
	}
	
	private void workLoop() throws IOException, InterruptedException{
		logger.info("workLoop begin");
		doReg();
		
		BeanRequest punReq = null;
		DatagramPacket punPkt = null;
		BeanPeer peer = null;
		for(Entry<String, BeanPeer> entry : peers.entrySet()){
			peer = entry.getValue();
			doSig(peer.name);
//			doPunch();
			punReq = buildRequestPun(this.otherSig);
			punPkt = buildReqPacket(punReq);
			punPkt.setSocketAddress(new InetSocketAddress(peer.publicIp, peer.publicPort));
//			Thread.sleep(3000);
			break;
		}
		
		
		this.udpSocket.setSoTimeout(1000);
		while(true){
			BeanRequest heartReq = new BeanRequest();
			heartReq.cmd = SimpleSignalServer.CMD_HEART;
			heartReq.param = getName();
			sendRequest(heartReq);
			
			try{
				while(true){
					constSendPun(punPkt);
					BeanResponse rsp = recvResponse();
					if(rsp.seq == heartReq.seq){
						logger.debug("got heart ack");
					}
					else if(rsp.cmd.equals(SimpleSignalServer.CMD_SIG)){
						logger.info("got sig");
						BeanSig beanSig = objectMapper.readValue(rsp.param, BeanSig.class);
						if(peer != null){
							logger.info("it is sig ack");
//							logger.warn("sig already in progress");
						}else{
							logger.info("it sig req from " + beanSig.thisPeer.name);
							this.otherSig = beanSig;
							
							peer = beanSig.thisPeer;
							BeanRequest req = buildRequestSig(beanSig.thisPeer.name);
							req.seq = rsp.seq;
							sendRequest(req);
							
							punReq = buildRequestPun(this.otherSig);
							punPkt = buildReqPacket(punReq);
							punPkt.setSocketAddress(new InetSocketAddress(peer.publicIp, peer.publicPort));
							
						}
						
					}
					else if(punReq != null && rsp.seq == punReq.seq){
						logger.info("got pun ack");
					}
					else if(rsp.cmd.equals(SimpleSignalServer.CMD_PUN)){
						logger.info("got pun from " + rspPkt.getAddress().getHostAddress() + ":" + rspPkt.getPort());
//						BeanRequest req = buildRequestPun(this.otherSig);
//						punPkt = buildReqPacket(req);
//						punPkt.setSocketAddress(new InetSocketAddress(peer.publicIp, peer.publicPort));
//						
//						this.udpSocket.send(punPkt);
					}else{
						logger.warn("got unknown cmd " + rsp.cmd);
					}
				}
			}catch(SocketTimeoutException e){
//				logger.debug("recv timeout: heart");
				continue;
			}
			
		}
//		logger.info("workLoop end");
	}
	
	private void doReg() throws JsonParseException, JsonMappingException, IOException, InterruptedException{
		BeanPeer peer = new BeanPeer();
		BeanRequest req = buildRequestReg(peer);
		this.udpSocket.setSoTimeout(2000);
		while(true){
			sendRequest(req);
			try{
				BeanResponse rsp = recvResponse();
				while(rsp.seq != req.seq){
					rsp = recvResponse();
				}
				
				Map<String, BeanPeer> respPeers = objectMapper.readValue(rsp.param, new TypeReference<Map<String, BeanPeer>>() {});
				for(Entry<String, BeanPeer> entry : respPeers.entrySet()){
					BeanPeer u = (BeanPeer)entry.getValue();
					if(u.regId.equals(peer.regId)){
						synchronized (this) {
							peers = respPeers;
							selfP = u;
							peers.remove(selfP.name);
							this.notifyAll();
							break;
						}
					}
				}
				
				return;
				
			}catch(SocketTimeoutException e){
				logger.info("recv timeout: reg");
				continue;
			}
		}
		
	}
	
	private BeanResponse recvResponse() throws JsonParseException, JsonMappingException, IOException{
		this.udpSocket.receive(rspPkt);
		String rspJson = new String(rspPkt.getData(), 0, rspPkt.getLength());
		logger.debug("rsp json: " + rspJson);
		BeanResponse rsp = objectMapper.readValue(rspJson, BeanResponse.class);
		return rsp;
		
	}
	
	private DatagramPacket buildReqPacket(BeanRequest req) throws JsonProcessingException{
		String reqJson = objectMapper.writeValueAsString(req);
		byte[] reqData = reqJson.getBytes();
		DatagramPacket pkt = new DatagramPacket(reqData, reqData.length);
		pkt.setLength(reqData.length);
		return pkt;
	}
	
	private void sendRequest(BeanRequest req) throws IOException{
		DatagramPacket pkt = buildReqPacket(req);
		pkt.setSocketAddress(new InetSocketAddress(this.servHost, this.servPort));
		this.udpSocket.send(pkt);
	}
	
	private BeanRequest buildRequestReg(BeanPeer beanPeer) throws JsonProcessingException{
		BeanRequest beanReq = new BeanRequest();
		beanReq.cmd = SimpleSignalServer.CMD_REG;
		beanReq.param = objectMapper.writeValueAsString(beanPeer);
		return beanReq;
	}
	
	private BeanRequest buildRequestSig(String peerName) throws JsonProcessingException{
		BeanSig sig = new BeanSig();
		sig.otherPeerName = peerName;
		sig.thisPeer = selfP;
//		BeanRequest req = buildRequestSig(sig);
//		return req;
		BeanRequest beanReq = new BeanRequest();
		beanReq.cmd = SimpleSignalServer.CMD_SIG;
		beanReq.param = objectMapper.writeValueAsString(sig);
		return beanReq;
	}
	
//	private BeanRequest buildRequestSig(BeanSig sig) throws JsonProcessingException{
//		BeanRequest beanReq = new BeanRequest();
//		beanReq.cmd = SimpleSignalServer.CMD_SIG;
//		beanReq.param = objectMapper.writeValueAsString(sig);
//		return beanReq;
//	}
	
	private BeanRequest buildRequestPun(BeanSig sig) throws JsonProcessingException{
		BeanRequest beanReq = new BeanRequest();
		beanReq.cmd = SimpleSignalServer.CMD_PUN;
		beanReq.param = sig.punId;
		return beanReq;
	}
	
	
	public static void main(String args[]) throws IOException, InterruptedException {
		SuperPuncher pun = new SuperPuncher();
		pun.detectNAT();
		
		//logger.info("local host " + InetAddress.getLocalHost());
//		SimpleSignalClient clt = new SimpleSignalClient("127.0.0.1", 7711, 0);
		SimpleSignalClient clt = new SimpleSignalClient("121.41.75.10", 7711, 0);
		clt.start();
		System.in.read();
	}


}
