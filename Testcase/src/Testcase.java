public class Testcase {
	private int counter = 0;
	
	public void WhileTest() {
		counter = 0;
		while (counter < 6) {
			counter++;
		}
		System.out.println("WhileTest " + counter);
	}
	
	public void DoWhileTest() {
		counter = 0;
		do {
			counter++;
		} while(counter < 6);
		System.out.println("DoWhileTest " + counter);
	}
	
	public void ForTest() {
		counter = 0;
		for (int i = 0; i < 6; i++) {
			counter++;
		}
		System.out.println("ForTest " + counter);
	}
	
	public void NestedLoopTest() {
		counter = 0;
		for (int j = 0; j < 3; j++)
			for (int k = 0; k < 2; k++)
				counter++;
		System.out.println("NestedLoopTest " + counter);
	}
	
	public void Callee() {
		System.out.println("I'm callee.");
	}
	
	public void Caller() {
		System.out.println("I'm caller.");
		Callee();
	}
	
	public int ReturnTest() {
		counter = 6;
		return counter;
	}
	
	public static void main(String[] args) {
		Testcase test = new Testcase();
		test.WhileTest();
		test.DoWhileTest();
		test.ForTest();
		test.NestedLoopTest();
		test.Caller();
		int ret = test.ReturnTest();
		System.out.println("ReturnTest " + ret);
	}
}
