#!/bin/sh

# we need to limit the number of threads that the Java runtime creates by
# default to fit into the default thread limit (4 threads). the minimum hit
# with the current options is 10 threads, but during loading of the jar
# file we can hit up to 16 threads
JVM_OPTS="
	-XX:CICompilerCount=2
	-XX:+UnlockExperimentalVMOptions
	-XX:+UseSerialGC
	-XX:+ReduceSignalUsage
	-XX:+DisableAttachMechanism
"

exec java ${JVM_OPTS} -jar $(dirname $0)/agent.jar $@
