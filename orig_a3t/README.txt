Directory Structure
===================
a3test
   |----------- src (the code for instrumentation and everything else)
   |----------- build (generated dir)
   |----------- run.xml (the ant file to run the instrumentation)
   |----------- test (tests)
   |               |--------- src
   |               |--------- build (generated dir)
   |------------ symlib (library for maipulating symbolic expressions and path constraints)
   |                   |------- src
   |                   |------- classes
   |------------ models (models for library classes)
                        |------ src
                        |------ build (generated dir)

The model classes are named with "models" prefix to their original
names. For example, the model class for java.lang.String must be named
models.java.lang.String, and placed in the directory
models/src/models/java/lang.

Configuring the setup
=====================
Create a file named "run.settings" and put it in the top-level
directory. run.xml file uses this file. The content of my run.settings
is as follows:

rt.jar=/home/saswat/software/jdk1.6.0_16/jre/lib/rt.jar
jce.jar=/home/saswat/software/jdk1.6.0_16/jre/lib/jce.jar
jsse.jar=/home/saswat/software/jdk1.6.0_16/jre/lib/jsse.jar

You have to change the above paths to point to your jdk files.

Building sources
================

There is a build.xml in each of the following directories: top-level
directory, tests, symlib, and models. Run ant in each of those
directories.

Annotating the application code
===============================

Methods can be annotated with @Symbolic annotations. The tool will
introduce symbolic values corresponding to each parameter of such
annotated methods. If the parameter is of primitive type, introduction
of corresponding symbolic values can be done automatically. For
non-primitive types, the tool will expect the model of the corresponding
class to implement a method with a specific signature. This method
is called to perform introduction of symbolic values. However, if
the user does not implement such a method, the tool will not introduce
any symbolic values.

Running the tool (this section is OUTDATED -- Mayur)
================
ant -f run.xml -Dmain=T2 -Dcp=tests/build

The run.xml expects two parameters -Dmain and -Dcp. -Dmain specifies
the entry class of the application. In the above, T2 is the
application class. -Dcp specifies the classpath containing application
classes. In the above, tests/build contains application classes.

Output of the tool (this section is OUTDATED -- Mayur)
==================
The instrumented files are stored in a jar file in the user's working
directory. The name of the jar file is <mainclassname>-a3t.jar. For
example, running the tool as above, produces T2-a3t.jar. Running the
tool also produces a log file.The log file is named as
<mainclassname>-a3t.log. The most useful information that the log
contains right now is information about missing model methods. This
information is to be used to decide which methods should be modeled.

[saswat@solve a3t]$ grep Missing T2-a3t.log -A 4
Missing model
Method: void _4init_4()V(java.lang.Object,gov.nasa.jpf.symbolic.integer.Expression)
Model method sig:  void _4init_4____V(java.lang.Object,gov.nasa.jpf.symbolic.integer.Expression)
Model class: models.java.lang.Object

--
Missing model
Method: void println(Ljava/lang/String;)V(java.lang.Object,gov.nasa.jpf.symbolic.integer.Expression,java.lang.String,gov.nasa.jpf.symbolic.integer.Expression)
Model method sig:  void println__Ljava_lang_String_2__V(java.lang.Object,gov.nasa.jpf.symbolic.integer.Expression,java.lang.String,gov.nasa.jpf.symbolic.integer.Expression)
Model class: models.java.io.PrintStream

Later I will describe how to read the above (rather cryptic) information.

Running the instrumented files (this section is OUTDATED -- Mayur)
=============================
Instrumented files can be executed by providing all necessary jar
files in the classpath. For example,

[saswat@solve a3t]$ java -classpath T2-a3t.jar:symlib/lib/symbolic.jar:models/build T2

Above execution produces a file called .0.pc. in the user.s working
directory. This file contains the path constraint for the path that
was executed. The content of the 0.pc produced from the above example
execution is:

(assert (= (java.lang.String.equals foo$java$lang$String$java$lang$String$0 foo$java$lang$String$java$lang$String$1) bv0[32]))

I will later describe the format in detail later. Basically the above
specifies the two symbolic string values are *not* equal. The two
symbolic string values are
   foo$java$lang$String$java$lang$String$0 and
   foo$java$lang$String$java$lang$String$1
bv0[32] represents constant 0 as a bit-vector of length
32. .java.lang.String.equals. is a uninterpreted function symbol that
is introduced by the model of the .equals. method of the
java.lang.String class.

Every symbol encodes the signature of the method in which it was
introduced into the system. For example,
foo$java$lang$String$java$lang$String$0 represents the 0th argument of
the method .foo. and is of java.lang.String type. Similarly,
foo$java$lang$String$java$lang$String$1 represents the 1th parameter
of the foo method.

=====
1. Build SDK
2. Copy ramdisk.img
3. Copy out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/classes.jar to a3t/libs/framework.jar
4. Clean and build using a3t-framework.xml

=====
hings to do once and for all:
==============================

Define environment variables SDK_DIR and A3T_DIR (e.g. in your .bashrc file) to point to
the absolute location of your Android SDK installation and a3t/ directory, respectively.

Example:

SDK_DIR=/Users/mhn/android-sdk-mac_x86
A3T_DIR=/Users/mhn/repositories/pag/code/a3t/

Things to do to set up an app:
==============================

Replace or adapt the build.xml file in the app's base directory using the sample.build.xml
file in this directory.

Things to do to instrument and run the instrumented app:
========================================================

Run "ant -Da3t= debug" in the app's base directory.

Android App Documentation:
==========================

Creating an Android app from the command-line:
http://developer.android.com/guide/developing/projects/projects-cmdline.html

Building and Running an Android app from the command-line:
http://developer.android.com/guide/developing/building/index.html

