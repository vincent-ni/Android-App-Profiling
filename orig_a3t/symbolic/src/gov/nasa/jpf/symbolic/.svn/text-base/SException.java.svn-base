package gov.nasa.jpf.symbolic;


/* Symbolic Excution Exception class */
/* Purpose of this class is to distinguish runtime
   exceptions caused from inside symbolic execution library
   from runtime exception that might raise due to bug in
   the code being checked.
   For example: If a constraint can not be handled by the decision proc.,
   a SException is generated
*/
public class SException extends RuntimeException
{
    public SException(String msg)
    {
	super(msg);
    }
}
