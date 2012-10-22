package gov.nasa.jpf.symbolic.integer;

public class Types
{
    public static final int BOOLEAN = 0;
    public static final int CHAR = 1;
    public static final int SHORT = 2;
    public static final int BYTE = 3;
    public static final int INT = 4;
    public static final int LONG = 5;
    public static final int FLOAT = 6;
    public static final int DOUBLE = 7;
    public static final int REF = 8;

    public static final String[] types = {"boolean", "char", "short", "byte", "int", "long", "float", "double", "ref"};

    public static String toString(int type)
    {
		return types[type];
    }
    
}