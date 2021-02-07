## Binding a language to Gravity

This directory contains a sample Objective-C bridge that shows you how to bind a new language to Gravity.

main.m file contains a standard main function that loads a Gravity source `main.gravity` and executes it.

The core function is a C function:
`void objc_register (gravity_vm *VM);`
used to register the ObjC class to the current Gravity VM instance.

ObjC is a dynamic language that offers a rich set of runtime reflection features.
An ObjC class can be scanned using the `objc/runtime.h` API.
Using these APIs, we can construct a Gravity class that acts as a bridge between the ObjC runtime and the Gravity runtime with access to all its methods and properties.

A more manual approach should be followed for other languages that do not offer runtime introspection features (if it is not possible to extract methods and properties at runtime). A manual approach could be header extraction or a text file that contains all the necessary information to reconstruct the class (and that it must be manually parsed).


#### Java
Some users asked for a way to bind Java to Gravity. I am not a Java expert, but I am sure that it can be done and the process can be fully automated.
1. To build a Gravity class you can use the Java reflection API
2. To execute Java code from C, the JNI can be used 

Some useful Java related links:

* https://www.baeldung.com/java-reflection
* https://nachtimwald.com/2017/06/17/calling-java-from-c/
* https://stackoverflow.com/questions/819536/how-to-call-java-functions-from-c
* https://www.codeproject.com/Articles/22881/How-to-Call-Java-Functions-from-C-Using-JNI
