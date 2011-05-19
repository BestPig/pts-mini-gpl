README for jtty-builder
~~~~~~~~~~~~~~~~~~~~~~~
jtty-builder is an easy-to-install, lightweight webserver with a JSP
container for Unix. More specifically, jtty-builder is just a set of shell
scripts for Unix which automate downloading and building a lightweight Java
web and application server for serving JSP (and Java servlets). The
container used is Jtty, which uses Jetty, which uses parts of Apache Tomcat
(mostly Jasper).

To start serving a hello-world JSP page on Unix, do this:

1. Download jtty-builder:

  $ svn co
  $ cd jtty-builder

2. Install java and javac. Example command on Ubuntu Lucid:

  $ sudo apt-get install openjdk-6-jdk

3. Use jtty-builder to download the sources of Jtty, Jetty and Tomcat:

  $ (cd jtty-builder && ./download_jttyp.jar)

4. Use jtty-builder to build jttyp.jar:

  $ (cd jtty-builder && ./build_jttyp.jar)

5. Create your .war file or application directory. A simple example:

  $ mkdir Hello-World
  $ (echo '<html><head><title>JSP Test</title>'
     echo '<%! String message = "Hello, World."; %>'
     echo '</head><body><h2><%= message %></h2>'
     echo '<%= new java.util.Date() %></body></html>') >Hello-World/hi.jsp
  $ mkdir Hello-World/WEB-INF
  $ (echo '<web-app>'
     echo '<display-name>Hello World</display-name>'
     echo '</web-app>') >Hello-World/WEB-INF/web.xml

6. Copy jttyp.jar to the application directory:

  # cp jtty-builder/jttyp.jar .

7. Start the Java application server running your Hello-World application:

  $ java -jar jttyp.jar 8765 Hello-World

8. Try the web page in your web browser by visiting http://127.0.0.1:8765/
   Reload it to get the time updated.

__EOF__
