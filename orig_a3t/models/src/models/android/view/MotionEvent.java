package models.android.view;

import java.lang.reflect.Field;
import gov.nasa.jpf.symbolic.array.SymbolicFloatArray;
import gov.nasa.jpf.symbolic.integer.Expression;
import gov.nasa.jpf.symbolic.integer.SymbolicFloat;
import android.util.Slog;
import edu.gatech.symbolic.Util;

public class MotionEvent
{
	private static int count = 0;
	private static final String[] fldNames = {"mDownTimeNano", "mAction", "mXOffset", 
											  "mYOffset", "mXPrecision", "mYPrecision", 
											  "mEdgeFlags", "mMetaState", "gRecyclerUsed",
											  "mFlags", "mNumPointers", "mNumSamples",
											  "mLastDataSampleIndex", "mLastEventTimeNanoSampleIndex",
											  "mPointerIdentifiers", "mNumSamples"};

	public static Expression Landroid_view_MotionEvent_2(java.lang.Object seed, java.lang.String name)
    {
        int id = count++;
        if (id % 2 == 0) {
            //XXX: tap specific trick. each tap calls this method twice (UP and DOWN)
			Util.newEvent();
           	Util.e("A3T_ITER", String.valueOf(id));
        }
		Slog.e("A3T_DEBUG", "symbolic motion event injected " + id);
		android.view.MotionEvent event = (android.view.MotionEvent) seed;
		Class cls = event.getClass();

		try {
			String fldName = "mDataSamples";
			Field conFld = cls.getDeclaredField(fldName);			
			conFld.setAccessible(true);
			float[] conVal = (float[]) conFld.get(event);
			Expression symVal = (conVal == null) ? null : new SymbolicFloatArray(name+"$"+id, new int[]{0, 1});
			setSymField(event, cls, fldName, symVal);

			for(String fName : fldNames)
				setSymField(event, cls, fName, null);
			
			return null;
		 } catch (NoSuchFieldException e) {
			 throw new Error(e);
		 } catch (IllegalAccessException e) {
			 throw new Error(e);
		 }
	 }

	
	private static void setSymField(android.view.MotionEvent event, Class cls, String fldName, Expression symVal) 
		throws NoSuchFieldException, IllegalAccessException
	{
		Field symFld = cls.getDeclaredField(fldName+"$sym");
		symFld.setAccessible(true);
		symFld.set(event, symVal);
	}
}
