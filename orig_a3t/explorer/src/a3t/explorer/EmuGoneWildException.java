package a3t.explorer;

public class EmuGoneWildException extends RuntimeException
{
	private final int port;
	
	public EmuGoneWildException(int port)
	{
		this.port = port;
	}
	
	public int port()
	{
		return this.port;
	}
}