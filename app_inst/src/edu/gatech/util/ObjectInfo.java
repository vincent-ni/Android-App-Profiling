package edu.gatech.util;

public class ObjectInfo {
	public String type;
	public String value;
	public boolean isPrimType;
	public boolean isSerialized;
	int serializedSize;
	
	public ObjectInfo(String type, String value, boolean isPrimType, int serializedSize){
		this.type = type;
		this.value = value;
		this.isPrimType = isPrimType;
		this.serializedSize = serializedSize;
		this.isSerialized = (serializedSize != -1);
	}
}
