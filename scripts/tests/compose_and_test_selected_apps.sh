#!/bin/bash

OSV_DIR=$(readlink -f $(dirname $0)/../..)
CAPSTAN_REPO=$HOME/.capstan

argv0=${0##*/}
usage() {
	cat <<-EOF
	Compose and test apps out ouf pre-built capstan packages

	Usage: ${argv0} [options] <group_of_tests> | <test_app_name>
	Options:
	  -c              Compose test app image only
	  -r              Run test app only (must have been composed earlier)
	  -R              Compose test app image with RoFS (ZFS is the default)
	EOF
	exit ${1:-0}
}

FS=zfs
COMPOSE_ONLY=false
RUN_ONLY=false

while getopts crRh: OPT ; do
	case ${OPT} in
	c) COMPOSE_ONLY=true;;
	r) RUN_ONLY=true;;
	R) FS=rofs;;
	h) usage;;
	?) usage 1;;
	esac
done

shift $((OPTIND - 1))
[[ -z $1 ]] && usage 1

TEST="$1"

compose_test_app()
{
  local APP_NAME=$1
  local DEPENDANT_PKG1=$2
  local DEPENDANT_PKG2=$3

  local IMAGE_PATH="$CAPSTAN_REPO/repository/test-$APP_NAME/test-$APP_NAME.qemu"

  if [ $RUN_ONLY == false ]; then
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
  else
    echo "Reusing the test image: $IMAGE_PATH that must have been composed before!"
  fi

  if [ -f "$IMAGE_PATH" ]; then
    cp $CAPSTAN_REPO/repository/"test-$APP_NAME"/"test-$APP_NAME".qemu $OSV_DIR/build/release/usr.img
  else
    echo "Could not find test image: $IMAGE_PATH!" 
    exit 1
  fi
}

run_test_app()
{
  local OSV_APP_NAME=$1
  local TEST_PARAMETER=$2

  if [ $COMPOSE_ONLY == false ]; then
    echo "-----------------------------------"
    echo " Testing $OSV_APP_NAME ... "
    echo "-----------------------------------"

    if [ -f $OSV_DIR/apps/$OSV_APP_NAME/test.sh ]; then
      $OSV_DIR/apps/$OSV_APP_NAME/test.sh $TEST_PARAMETER
    elif [ -f $OSV_DIR/modules/$OSV_APP_NAME/test.sh ]; then
      $OSV_DIR/modules/$OSV_APP_NAME/test.sh $TEST_PARAMETER
    fi
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
  compose_and_run_test_app "lua-hello-from-host"
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
  compose_and_run_test_app "iperf3"
  compose_and_run_test_app "graalvm-netty-plot"
  compose_test_app "ffmpeg" "libz" && run_test_app "ffmpeg" "video_subclip" && run_test_app "ffmpeg" "video_transcode"
  compose_and_run_test_app "redis-memonly"
  compose_and_run_test_app "cli"
  compose_and_run_test_app "mysql"
  compose_test_app "apache-derby" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "apache-derby"
  compose_test_app "apache-kafka" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "apache-kafka"
  compose_and_run_test_app "elasticsearch" #TODO: Understand why guest fails in the end
}

run_unit_tests()
{
  compose_test_app "unit-tests" && run_test_app "tests"
  compose_test_app "httpserver-api-tests" && run_test_app "httpserver-api" "http"
  compose_test_app "httpserver-api-https-tests" "httpserver-api-tests" && run_test_app "httpserver-api" "https"
}


case "$TEST" in
  simple)
    echo "Testing simple apps ..."
    echo "-----------------------------------"
    test_simple_apps;;
  http)
    echo "Testing HTTP apps ..."
    echo "-----------------------------------"
    test_http_apps;;
  with_tester)
    echo "Testing apps with custom tester ..."
    echo "-----------------------------------"
    test_apps_with_tester;;
  unit_tests)
    echo "Running unit tests ..."
    echo "-----------------------------------"
    run_unit_tests;;
  all)
    echo "Running all tests ..."
    echo "-----------------------------------"
    run_unit_tests
    test_simple_apps
    test_http_apps
    test_apps_with_tester;;
  *)
    compose_and_run_test_app $TEST
esac

#
#TODO
#osv.netperf
#osv.memcached
#
#-- Think of Ruby
