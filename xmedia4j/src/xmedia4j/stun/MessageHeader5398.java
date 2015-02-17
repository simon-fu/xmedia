package xmedia4j.stun;

import de.javawi.jstun.header.MessageHeader;
import de.javawi.jstun.util.UtilityException;

public class MessageHeader5398 extends MessageHeader{
	
	static byte[] MAGiC_COOKIE = new byte[]{(byte) 0x21, (byte) 0x12, (byte) (byte) 0xA4, (byte) 0x42};
	
	public MessageHeader5398(MessageHeaderType type) {
		super(type);
	}
	
	@Override 
	public void generateTransactionID() throws UtilityException{
		super.generateTransactionID();
		System.arraycopy(MAGiC_COOKIE, 0, this.id, 0, MAGiC_COOKIE.length);
	}
}
