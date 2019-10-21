#!/bin/bash

OSV_DIR=$(readlink -f $(dirname $0)/../..)
CAPSTAN_REPO=$HOME/.capstan
FS=zfs

compose_test_app()
{
  local APP_NAME=$1
  local DEPENDANT_PKG1=$2
  local DEPENDANT_PKG2=$3

  local DEPENDENCIES="--require osv.$APP_NAME"
  if [ "$DEPENDANT_PKG1" != "" ]; then
     DEPENDENCIES="--require osv.$DEPENDANT_PKG1 $DEPENDENCIES"
  fi
  if [ "$DEPENDANT_PKG2" != "" ]; then
     DEPENDENCIES="--require osv.$DEPENDANT_PKG2 $DEPENDENCIES"
  fi

  echo "-----------------------------------"
  echo " Composing $APP_NAME ... "
  echo "-----------------------------------"

  TEMPDIR=$(mktemp -d) && pushd $TEMPDIR > /dev/null && \
    capstan package compose $DEPENDENCIES --fs $FS "test-$APP_NAME" && \
    rmdir $TEMPDIR && popd > /dev/null
  cp $CAPSTAN_REPO/repository/"test-$APP_NAME"/"test-$APP_NAME".qemu $OSV_DIR/build/release/usr.img
}

run_test_app()
{
  local OSV_APP_NAME=$1
  local TEST_PARAMETER=$2

  echo "-----------------------------------"
  echo " Testing $OSV_APP_NAME ... "
  echo "-----------------------------------"

  if [ -f $OSV_DIR/apps/$OSV_APP_NAME/test.sh ]; then
    $OSV_DIR/apps/$OSV_APP_NAME/test.sh $TEST_PARAMETER
  elif [ -f $OSV_DIR/modules/$OSV_APP_NAME/test.sh ]; then
    $OSV_DIR/modules/$OSV_APP_NAME/test.sh $TEST_PARAMETER
  fi
  echo ''
}

compose_and_run_test_app()
{
  local APP_NAME=$1
  compose_test_app $APP_NAME
  run_test_app $APP_NAME
}

test_simple_apps()
{
  compose_test_app "golang-example" "run-go" && run_test_app "golang-example"
  compose_and_run_test_app "golang-pie-example"
  compose_and_run_test_app "graalvm-example"
  compose_and_run_test_app "graalvm-example"
  compose_and_run_test_app "lua-hello-from-host
  compose_and_run_test_app "rust-example"
  compose_and_run_test_app "stream"
  compose_test_app "python2-from-host" && run_test_app "python-from-host"
  compose_test_app "python3-from-host" && run_test_app "python-from-host"
}

test_http_apps()
{
  compose_test_app "golang-httpserver" "run-go" && run_test_app "golang-httpserver"
  compose_and_run_test_app "golang-pie-httpserver"
  compose_and_run_test_app "graalvm-httpserver"
  compose_and_run_test_app "lighttpd"
  compose_and_run_test_app "nginx"
  compose_and_run_test_app "rust-httpserver"
  #TODO: Test with multiple versions of java
  compose_test_app "jetty" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "jetty"
  compose_test_app "tomcat" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "tomcat"
  compose_test_app "vertx" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "vertx"
  compose_test_app "spring-boot-example" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "spring-boot-example" #Really slow
  #TODO: Test with multiple versions of node
  compose_test_app "node-express-example" "node-from-host" && run_test_app "node-express-example"
  compose_test_app "node-socketio-example" "node-from-host" && run_test_app "node-socketio-example"
}

test_apps_with_tester()
{
  compose_and_run_test_app "iperf3" && run_test_app "
  compose_and_run_test_app "graalvm-netty-plot" && run_test_app "
  compose_test_app "ffmpeg" "" "libz" # && run_test_app "TODO handle parameter passed to test.sh
  compose_and_run_test_app "redis-memonly" && run_test_app "
  compose_and_run_test_app "cli" && run_test_app "
  #-----------------------
  ## compose_test_app "elasticsearch" && run_test_app "
  ## compose_test_app "apache-derby"  && run_test_app "#ADD tester
  ## compose_test_app "apache-kafka"  && run_test_app "#Cannot connect tp each other
  #compose_test_app "mysql"  && run_test_app "FAILS
}

test_unit_tests()
{
  compose_test_app "unit-tests" && run_test_app "tests"
  #compose_test_app "httpserver-api-tests" && run_test_app "httpserver-api" http
  #compose_test_app "httpserver-api-https-tests" && run_test_app "httpserver-api" https
}

#test_simple_apps
#test_http_apps
#test_apps_with_tester
test_unit_tests

#
#-- LATER
#osv.netperf
#osv.memcached
#
#-- Think of Ruby
