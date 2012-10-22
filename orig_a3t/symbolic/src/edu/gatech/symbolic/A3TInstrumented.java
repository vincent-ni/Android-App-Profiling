package edu.gatech.symbolic;

import java.lang.annotation.*;

/**
   every class that we instrument and that does not have any native
   methods, is annotated with this annotation. This provides a quick way to
   check during runtime for such methods
*/

@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.TYPE)
public @interface A3TInstrumented { }
