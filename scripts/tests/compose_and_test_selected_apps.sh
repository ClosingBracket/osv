#!/bin/bash

OSV_DIR=$(readlink -f $(dirname $0)/../..)
CAPSTAN_REPO=$HOME/.capstan

OSV_VERSION=$($OSV_DIR/scripts/osv-version.sh | cut -d - -f 1 | grep -Po "[^v]*")
OSV_COMMIT=$($OSV_DIR/scripts/osv-version.sh | grep -Po "\-g.*" | grep -oP "[^-g]*")

if [ "$OSV_COMMIT" != "" ]; then
  OSV_VERSION="$OSV_VERSION-$OSV_COMMIT"
fi

argv0=${0##*/}
usage() {
	cat <<-EOF
	Compose and test apps out ouf pre-built capstan packages

	Usage: ${argv0} [options] <group_of_tests> | <test_app_name>
	Options:
	  -c              Compose test app image only
	  -r              Run test app only (must have been composed earlier)
	  -R              Compose test app image with RoFS (ZFS is the default)
	  -l              Use latest OSv kernel from build/last to build test image
	  -f              Run OSv on firecracker
	EOF
	exit ${1:-0}
}

FS=zfs
COMPOSE_ONLY=false
RUN_ONLY=false
LOADER="osv-loader"
OSV_HYPERVISOR="qemu"

while getopts crRlfh: OPT ; do
	case ${OPT} in
	c) COMPOSE_ONLY=true;;
	r) RUN_ONLY=true;;
	R) FS=rofs;;
        l) LOADER="osv-latest-loader";;
        f) OSV_HYPERVISOR="firecracker";;
	h) usage;;
	?) usage 1;;
	esac
done

export OSV_HYPERVISOR

shift $((OPTIND - 1))
[[ -z $1 ]] && usage 1

TEST_APP_PACKAGE_NAME="$1"
TEST_OSV_APP_NAME="$2"

determine_platform() {
  if [ -f /etc/os-release ]; then
    PLATFORM=$(grep PRETTY_NAME /etc/os-release | cut -d = -f 2 | grep -o -P "[^\"]+")
  elif [ -f /etc/lsb-release ]; then
    PLATFORM=$(grep DISTRIB_DESCRIPTION /etc/lsb-release | cut -d = -f 2 | grep -o -P "[^\"]+")
  else
    PLATFORM="Unknown Linux"
  fi
}

compose_test_app()
{
  local APP_NAME=$1
  local DEPENDANT_PKG1=$2
  local DEPENDANT_PKG2=$3

  local IMAGE_PATH="$CAPSTAN_REPO/repository/test-$APP_NAME-$FS/test-$APP_NAME-$FS.qemu"

  if [ $RUN_ONLY == false ]; then
    local DEPENDENCIES="--require osv.$APP_NAME"
    if [ "$DEPENDANT_PKG1" != "" ]; then
      DEPENDENCIES="--require osv.$DEPENDANT_PKG1 $DEPENDENCIES"
    fi
    if [ "$DEPENDANT_PKG2" != "" ]; then
      DEPENDENCIES="--require osv.$DEPENDANT_PKG2 $DEPENDENCIES"
    fi

    echo "-------------------------------------------------------"
    echo " Composing $APP_NAME into $FS image at $IMAGE_PATH ... "
    echo "-------------------------------------------------------"

    #Copy latest OSv kernel if requested by user
    local LOADER_OPTION=""
    if [ "$LOADER" != "osv-loader" ]; then
      mkdir -p "$CAPSTAN_REPO/repository/$LOADER"
      cp -a $OSV_DIR/build/last/loader.img "$CAPSTAN_REPO/repository/$LOADER/$LOADER.qemu"
      determine_platform
      cat << EOF > $CAPSTAN_REPO/repository/$LOADER/index.yaml
format_version: "1"
version: "$OSV_VERSION"
created: $(date +'%Y-%m-%d %H:%M')
description: "OSv kernel"
platform: "$PLATFORM"
EOF
      LOADER_OPTION="--loader_image $LOADER"
      echo "Using latest OSv kernel from $OSV_DIR/build/last/loader.img !"
    fi

    if [ "$FS" == "rofs" ]; then
      FSTAB=static/etc/fstab_rofs
    else
      FSTAB=static/etc/fstab
    fi

    TEMPDIR=$(mktemp -d) && pushd $TEMPDIR > /dev/null && \
      mkdir -p etc && cp $OSV_DIR/$FSTAB etc/fstab && \
      $HOME/projects/go/src/github.com/cloudius-systems/capstan/capstan package compose $DEPENDENCIES --fs $FS $LOADER_OPTION "test-$APP_NAME-$FS" && \
      rm -rf $TEMPDIR && popd > /dev/null
  else
    echo "Reusing the test image: $IMAGE_PATH that must have been composed before!"
  fi

  if [ -f "$IMAGE_PATH" ]; then
    cp $CAPSTAN_REPO/repository/"test-$APP_NAME-$FS"/"test-$APP_NAME-$FS".qemu $OSV_DIR/build/last/usr.img
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
    echo "-------------------------------------------------------"
    echo " Testing $OSV_APP_NAME ... "
    echo "-------------------------------------------------------"

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

# Simple stateless apps that should work with both ZFS and ROFS
test_simple_apps() #stateless
{
  compose_and_run_test_app "golang-example"
  compose_and_run_test_app "golang-pie-example"
  compose_and_run_test_app "graalvm-example"
  compose_and_run_test_app "lua-hello-from-host"
  compose_and_run_test_app "rust-example"
  compose_and_run_test_app "stream"
  compose_test_app "python2-from-host" && run_test_app "python-from-host"
  compose_test_app "python3-from-host" && run_test_app "python-from-host"
}

# Stateless http java apps that should work with both ZFS and ROFS
test_http_java_apps()
{
  #TODO: Test with multiple versions of java
  compose_test_app "jetty" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "jetty"
  compose_test_app "tomcat" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "tomcat"
  compose_test_app "vertx" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "vertx"
  compose_test_app "spring-boot-example" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "spring-boot-example" #Really slow
}

# Stateless node apps that should work with both ZFS and ROFS
test_http_node_apps()
{
  #TODO: Test with multiple versions of node
  compose_test_app "node-express-example" "node-from-host" && run_test_app "node-express-example"
  compose_test_app "node-socketio-example" "node-from-host" && run_test_app "node-socketio-example"
}

# Stateless http apps that should work with both ZFS and ROFS except for nginx
test_http_apps() #stateless
{
  compose_and_run_test_app "golang-httpserver"
  compose_and_run_test_app "golang-pie-httpserver"
  compose_and_run_test_app "graalvm-httpserver"
  compose_and_run_test_app "lighttpd"
  if [ "$FS" == "zfs" ]; then #TODO: Fix configuration to make it work with ROFS
    compose_and_run_test_app "nginx" #Not ROFS
  fi
  compose_and_run_test_app "rust-httpserver"
  test_http_java_apps
  test_http_node_apps
}

test_apps_with_tester() #most stateless
{
  compose_and_run_test_app "iperf3"
  compose_and_run_test_app "graalvm-netty-plot"
  compose_test_app "ffmpeg" && run_test_app "ffmpeg" "video_subclip" && run_test_app "ffmpeg" "video_transcode"
  compose_and_run_test_app "redis-memonly" && run_test_app "redis-memonly" "ycsb"
  compose_and_run_test_app "keydb" && run_test_app "keydb" "ycsb"
  compose_and_run_test_app "cli"
  if [ "$FS" == "zfs" ]; then #These are stateful apps
    compose_and_run_test_app "mysql"
    compose_test_app "apache-derby" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "apache-derby"
    compose_test_app "apache-kafka" "run-java" "openjdk8-zulu-compact3-with-java-beans" && run_test_app "apache-kafka"
    compose_and_run_test_app "elasticsearch"
  fi
}

run_unit_tests() #regular unit tests are stateful
{
  # Unit tests are special as the unit tests runner depends on usr.manifest which
  # needs to be placed in the tests module. So let us gegnerate it on the fly from the unit tests mpm
  capstan package describe osv.common-tests -c | grep "/tests/tst-" | grep -o "/tests/tst-.*" | sed 's/$/: dummy/' > $OSV_DIR/modules/tests/usr.manifest
  capstan package describe "osv.$FS-tests" -c | grep "/tests/tst-" | grep -o "/tests/tst-.*" | sed 's/$/: dummy/' >> $OSV_DIR/modules/tests/usr.manifest
  compose_test_app "$FS-tests" "openjdk8-from-host" "common-tests" && run_test_app "tests"
  if [ "$FS" == "zfs" ]; then #These are stateful apps
    compose_test_app "httpserver-api-tests" && run_test_app "httpserver-api" "http"
    rm -rf $OSV_DIR/modules/certs/build && mkdir -p $OSV_DIR/modules/certs/build && pushd $OSV_DIR/modules/certs/build
    tar xf $CAPSTAN_REPO/packages/osv.httpserver-api-tests.mpm /client/client.key && \
    tar xf $CAPSTAN_REPO/packages/osv.httpserver-api-tests.mpm /etc/pki/CA/cacert.pem && \
    tar xf $CAPSTAN_REPO/packages/osv.httpserver-api-tests.mpm /client/client.pem && \
    mv client/* . && mv etc/pki/CA/cacert.pem .
    popd
    compose_test_app "httpserver-api-https-tests" "httpserver-api-tests" && run_test_app "httpserver-api" "https"
  fi
}


case "$TEST_APP_PACKAGE_NAME" in
  simple)
    echo "Testing simple apps ..."
    echo "-----------------------------------"
    test_simple_apps;;
  http)
    echo "Testing HTTP apps ..."
    echo "-----------------------------------"
    test_http_apps;;
  http-java)
    echo "Testing HTTP Java apps ..."
    echo "-----------------------------------"
    test_http_java_apps;;
  http-node)
    echo "Testing HTTP Node apps ..."
    echo "-----------------------------------"
    test_http_node_apps;;
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
    compose_and_run_test_app specjvm
    test_apps_with_tester;;
  *)
    if [ "$TEST_OSV_APP_NAME" == "" ]; then
      compose_and_run_test_app "$TEST_APP_PACKAGE_NAME"
    else
      compose_test_app "$TEST_APP_PACKAGE_NAME" && run_test_app "$TEST_OSV_APP_NAME"
    fi
esac

#
#TODO
#osv.netperf
#osv.memcached
#
#-- Think of Ruby
