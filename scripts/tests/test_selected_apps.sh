# For java apps it wold be nice to test them with java.so and /usr/bin/java as well as verious versions of Java - 8, 11, 12, 13
# Possibly build and test with rofs and zfs
#----
#* Run and chec output for some text
osv.golang-example
osv.golang-pie-example
osv.graalvm-example
osv.lua-hello-from-host
osv.rust-example
osv.stream

osv.python2-from-host
osv.python3-from-host

# http with curl -------
osv.golang-httpserver
osv.golang-pie-httpserver
osv.graalvm-httpserver
osv.jetty
osv.lighttpd
osv.nginx
osv.rust-httpserver
tomcat
osv.vertx
osv.spring-boot-example

# Multiple versions of node
osv.node-express-example
osv.node-from-host
node-socketio-example

# With tester
osv.iperf3
osv.graalvm-netty-plot
osv.ffmpeg
osv.elasticsearch
osv.apache-derby #ADD tester
osv.apache-kafka #Cannot connect tp each other
osv.mysql
osv.redis-memonly

---- HERE
osv.netperf
osv.memcached

# Requires input
osv.cli

osv.httpserver-api-tests
osv.httpserver-api
osv.unit-tests

# --- runtimes, utility
osv.openjdk11-zulu
osv.openjdk8-zulu-compact3-with-java-beans
osv.run-go
osv.run-java

-- Think of Ruby
