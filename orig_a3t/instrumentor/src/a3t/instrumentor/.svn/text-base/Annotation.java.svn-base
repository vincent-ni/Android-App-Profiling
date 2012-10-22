package a3t.instrumentor;

import soot.SootMethod;
import soot.tagkit.AnnotationTag;
import soot.tagkit.VisibilityAnnotationTag;
import soot.tagkit.Tag;

public class Annotation
{
	private static final String symbolicClassName = 
		"L" + edu.gatech.symbolic.Symbolic.class.getName().replace(".", "/") + ";";

	static boolean isSymbolicMethod(SootMethod method)
	{
		for (Tag tag : method.getTags()) {
			//System.out.println("name: " + tag.getName());
			//System.out.println("val: " + tag.getValue
			if (tag instanceof VisibilityAnnotationTag) {
				for (AnnotationTag atag : ((VisibilityAnnotationTag) tag).getAnnotations()) {
					String type = atag.getType();
					System.out.println("type " + type);
					if (type.equals(symbolicClassName)) {
						return true;
					}
				}
			}
		}
		return false;
	}
}
