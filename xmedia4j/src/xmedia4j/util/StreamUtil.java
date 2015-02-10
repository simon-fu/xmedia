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
	
}
