#!/bin/bash --
set -ve
javac PTetriS.java
rm -f PTetriS.jar
java_syn.pl PTetriS.java
jar cvfm PTetriS.jar MANIFEST.MF *.class
tar czvf PTetriS.tgz PTetriS.jar PTetriS.java PTetriS.html PTetriS.java.html
