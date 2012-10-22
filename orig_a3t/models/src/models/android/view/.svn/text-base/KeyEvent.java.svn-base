package models.android.view;

import java.lang.reflect.Field;
import gov.nasa.jpf.symbolic.integer.SymbolicLong;
import gov.nasa.jpf.symbolic.integer.SymbolicInteger;
import gov.nasa.jpf.symbolic.integer.Types;
import gov.nasa.jpf.symbolic.integer.Expression;

public class KeyEvent
{
	private static int count = 0;

	private static Class[] fldTypes = new Class[] {
		long.class, long.class, int.class, int.class, int.class, int.class, int.class
	};

	private static String[] fldNames = new String[] {
		"mDownTime", "mEventTime", "mAction", "mKeyCode", "mRepeatCount", "mMetaState", "mScanCode"
	};

	public static Expression Landroid_view_KeyEvent_2(java.lang.Object seed, java.lang.String name)
    {
		System.out.println("symbolic key event injected");
		android.view.KeyEvent event = (android.view.KeyEvent) seed;
		Class cls = event.getClass();

		try {
			for (int i = 0; i < fldTypes.length; i++) {
				String fldName = fldNames[i];
				Field symFld = cls.getDeclaredField(fldName+"$sym");
				Field conFld = cls.getDeclaredField(fldName);
			
				symFld.setAccessible(true);
				conFld.setAccessible(true);
			
				Object conVal = conFld.get(event);
				if (conVal == null)
					continue;
			
				Expression symVal;
				String symName = name + "$" + count++;
				Class fldType = fldTypes[i];
				if (fldType == int.class)
					symVal = new SymbolicInteger(Types.INT, symName, 0);
				else if (fldType == long.class)
					symVal = new SymbolicLong(symName, 0);
				else
					throw new Error();
				symFld.set(event, symVal);
			}
			return null;
		} catch (NoSuchFieldException e) {
			throw new Error(e);
		} catch (IllegalAccessException e) {
			throw new Error(e);
		}
    }
}

