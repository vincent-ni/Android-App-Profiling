package faceDetector.gatech.edu;

import java.io.ByteArrayOutputStream;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.lang.reflect.Method;

public class ExecutionController {
	public Object execute(String methodName, Class<?>[] paramTypes, 
			Object[] paramValues, Object instance) throws Exception{
		
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		byte[] buf = null; 
		ObjectOutput out = new ObjectOutputStream(bos); 
		out.writeObject(instance);
		out.writeObject(methodName); 
		out.writeObject(paramTypes);
		out.writeObject(paramValues);
		out.close(); 
		      
		// Get the bytes of the serialized object 
		buf = bos.toByteArray(); 

		if(toOffloadExecution(buf.length)){
			return executeRemotely(buf);
		}
		else{
			return executeLocally(methodName, paramTypes, paramValues, instance);
		}
	}
	
	public boolean toOffloadExecution(int size) throws Exception{
		return false;
	}
	
	private Object executeLocally(String methodName, Class<?>[] paramTypes, 
		Object[] paramValues, Object instance) throws Exception{

		Method method  = instance.getClass().getDeclaredMethod(methodName, paramTypes);
		return method.invoke(instance, paramValues);
		
	}
	private Object executeRemotely(byte[] b) throws Exception{
		return null;
	}
}
