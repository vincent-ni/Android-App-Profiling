package a3t.explorer;

public class PanelDetector
{
	private boolean lastWasNot;

	void process(String pc)
	{
		lastWasNot = pc.startsWith("(not ");
	}
	
	boolean lastTapOnPanel()
	{
		return lastWasNot;
	}
	
	void finish()
	{
	}
}