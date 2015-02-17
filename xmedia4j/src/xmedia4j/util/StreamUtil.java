package xmedia4j.util;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

public class StreamUtil {
	public static long getLong(byte[] buffer, int begin, int end) {
		long n = 0;
		for (int i = begin; i < end; i++) {
			n <<= 8;
			n |= (long)buffer[i] & 0xFF;
		}
		return n;
	}
	
	public static void setLong(byte[] buffer, long n, int begin, int end) {
		for (end--; end >= begin; end--) {
			buffer[end] = (byte) (n % 256);
			n >>= 8;
		}
	}
	
	public static String loadSDPFile(String fileName) throws IOException{
        InputStream is = new FileInputStream(fileName);
        String line; 
        StringBuffer sb = new StringBuffer();
        BufferedReader reader = new BufferedReader(new InputStreamReader(is));
        line = reader.readLine();
        while (line != null) {
        	sb.append(line);
        	sb.append("\r\n"); 
            line = reader.readLine(); 
        }
        reader.close();
        is.close();
		return sb.toString();
	}
	
	final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();
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
	public static String bytesToHex(byte[] bytes) {
		return bytesToHex(bytes, 0, bytes.length);
	}
	public static String bytesToHex(byte[] bytes, int offset, int len, int maxLen) {
		if(len <= maxLen) {
			return bytesToHex(bytes, offset, len);
		}else{
			return bytesToHex(bytes, offset, maxLen) + " ...";
		}
	}
	
}
