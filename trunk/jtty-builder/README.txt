README for jtty-builder
~~~~~~~~~~~~~~~~~~~~~~~
jtty-builder is an easy-to-install, lightweight webserver with a JSP
container for Unix. More specifically, jtty-builder is just a set of shell
scripts for Unix which automate downloading and building a lightweight Java
web and application server for serving JSP (and Java servlets). The
container used is Jtty, which uses Jetty, which uses parts of Apache Tomcat
(mostly Jasper). Most steps are automated. There is no need to write
configuration files.

To start serving a hello-world JSP page on Unix, do this:

0. As a shortcut, you can use a prebuilt jttyp.jar instead of building one
   using jtty-builder. To do that, run the following command, and skip
   steps 1 to 5 (inclusive):

  $ wget -O jttyp.jar \
    http://pts-mini-gpl.googlecode.com/svn/trunk/jtty-prebuilt/jttyp.jar

1. Download jtty-builder:

  $ svn co http://pts-mini-gpl.googlecode.com/svn/trunk/jtty-builder

2. Install java and javac. Example command on Ubuntu Lucid:

  $ sudo apt-get install openjdk-6-jdk

3. Use jtty-builder to download the sources of Jtty, Jetty and Tomcat:

  $ (cd jtty-builder && ./download_jttyp.jar)

4. Use jtty-builder to build jttyp.jar:

  $ (cd jtty-builder && ./build_jttyp.jar)

5. Copy jttyp.jar to the application directory:

  # cp jtty-builder/jttyp.jar .

6. Create your .war file or application directory. A simple example:

  $ mkdir Hello-World
  $ (echo '<html><head><title>JSP Test</title>'
     echo '<%! String message = "Hello, World."; %>'
     echo '</head><body><h2><%= message %></h2>'
     echo '<%= new java.util.Date() %></body></html>') >Hello-World/hi.jsp
  $ mkdir Hello-World/WEB-INF
  $ (echo '<web-app>'
     echo '<display-name>Hello World</display-name>'
     echo '</web-app>') >Hello-World/WEB-INF/web.xml

7. Start the Java application server running your Hello-World application:

  $ java -jar jttyp.jar 8765 Hello-World

   (Keep the java application running.)

8. Try the web page in your web browser by visiting http://127.0.0.1:8765/
   Reload it to get the time updated.

   If you modify hi.jsp and reload the page, it gets autmatically recompiled
   and the new version will run.

See https://github.com/stephenh/jtty for more information about using
jtty.jar and jttyp.jar for staring Java web applications from the command
line.

__EOF__
