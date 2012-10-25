package edu.gatech.util;

public class ObjectInfo {
	public String type;
	public String value;
	public boolean isPrimType;
	public boolean isSerialized;
	public int serializedSize;
	public boolean isArray;
	public String elementType;
	public int arrayDimension;
	public int arraySize;
	
	public ObjectInfo(String type, String value, boolean isPrimType, int serializedSize){
		this.type = type;
		this.value = value;
		this.isPrimType = isPrimType;
		this.serializedSize = serializedSize;
		this.isSerialized = (serializedSize != -1);
		this.isArray = false;
	}
	
	public void updateArrayInfo(Object obj, String elementType, int dimen){
		this.isArray = true;
		this.elementType = elementType;
		this.arrayDimension = dimen;
		
	}
}
