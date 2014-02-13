import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;


public class Main {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		int toSend = 10;
		try (   Socket kkSocket = new Socket("192.168.2.3", 1234);) 
	        {
			  kkSocket.getOutputStream().write(toSend);
			  kkSocket.close();
			}
         catch (UnknownHostException e) {
        	 System.err.println("FAILED");
        } catch (IOException e) {
        	System.err.println("FAILED");
        }
	}

}
