package edu.gatech.symbolic;

import java.lang.annotation.*;

/**
   every class that we instrument and that has at least one native
   method, is annotated with this annotation. This provides a quick way to
   check during runtime for such methods
*/


@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.TYPE)
public @interface A3TNative { }
