# OSv

OSv is the versatile modular unikernel designed to run unmodified
Linux applications securely on micro-VMs in the cloud. Built from
the ground up for effortless deployment and management of microservices
and serverless apps, with superior performance.

OSv has new APIs for new applications, but also runs unmodified x86-64 Linux
binaries **as is**, which effectively makes it a **Linux binary compatible unikernel**
(for more details about Linux ABI compatibility please read
[this doc](https://github.com/cloudius-systems/osv/wiki/OSv-Linux-ABI-Compatibility)).
In particular OSv can run many managed language runtimes including unmodified
[**JVM**](https://github.com/cloudius-systems/osv-apps/tree/master/java-example),
**Python** [**2**](https://github.com/cloudius-systems/osv-apps/tree/master/python2x) and
[**3**](https://github.com/cloudius-systems/osv-apps/tree/master/python3x),
[**Node.JS**](https://github.com/cloudius-systems/osv-apps/tree/master/node-from-host),
[**Ruby**](https://github.com/cloudius-systems/osv-apps/tree/master/ruby-example), **Erlang**, and applications built on top of one.
It can also run applications written in languages compiling directly to native machine code like
**C**, **C++**, [**Golang**](https://github.com/cloudius-systems/osv-apps/tree/master/golang-httpserver)
and [**Rust**](https://github.com/cloudius-systems/osv-apps/tree/master/rust-httpserver) as well as native images produced
by [**GraalVM**](https://github.com/cloudius-systems/osv-apps/tree/master/graalvm-example).

OSv can boot as fast as **~5 ms** on Firecracker using as low as 15 MiB of memory.
OSv can run on many hypervisors including QEMU/KVM,
[Firecracker](https://github.com/cloudius-systems/osv/wiki/Running-OSv-on-Firecracker),
Xen, [VMWare](https://github.com/cloudius-systems/osv/wiki/Running-OSv-on-VMware-ESXi),
[VirtualBox](https://github.com/cloudius-systems/osv/wiki/Running-OSv-on-VirtualBox) and
Hyperkit as well as open clouds like AWS EC2, GCE and OpenStack.

For more information about OSv, see [the main wiki page](https://github.com/cloudius-systems/osv/wiki)
and http://osv.io/.

## Building and Running Apps on OSv

In order to run an application on OSv, one needs to build an image by fusing OSv kernel and
the application files together. This, in high level can be achieved in two ways:
- by using the script [build](https://github.com/cloudius-systems/osv/blob/master/scripts/build)
 that builds the kernel from source and fuses it with application files
- by using the [capstan tool](https://github.com/cloudius-systems/capstan) that uses *pre-built kernel*
 and combines it with application files to produce final image

If your intention is to try to run your app on OSv with least effort, you should pursue the 'capstan'
route. For introduction please read this [crash course](https://github.com/cloudius-systems/osv/wiki/Build-and-run-apps-on-OSv-using-Capstan)
and for more details about capstan read [this documentation](https://github.com/cloudius-systems/capstan#documentation).

If you are comfortable with make and GCC toolchain and want to try the latest OSv code, then you should
read remaining part of this page to guide you how to setup your development environment and build OSv kernel
and application images.

## Setting up development environment

OSv can only be built on a 64-bit x86 Linux distribution. Please note that
this means the "x86_64" or "amd64" version, not the 32-bit "i386" version.

In order to build OSv kernel you need a physical or virtual machine with Linux distribution on it and GCC toolchain and
all necessary packages and libraries OSv build process depends on. The easiest way to set it up is to use 
[Docker files](https://github.com/cloudius-systems/osv/tree/master/docker#docker-osv-builder) that OSv comes with.
You can use them to build your own Docker image and then start it in order to build OSv kernel inside of it.

Otherwise, you can manually clone OSv repo and use [setup.py](https://github.com/cloudius-systems/osv/blob/master/scripts/setup.py)
to install GCC and all required packages, as long as it supports your Linux distribution and you have both git and python 2.7 installed on your machine:
```bash
git clone https://github.com/cloudius-systems/osv.git
cd osv && git submodule update --init --recursive
./scripts/setup.py
```

The ```setup.py``` recognizes and installs packages for number of Linux distributions including Fedora, Ubuntu, Debian, LinuxMint and RedHat ones (Scientific Linux, NauLinux, CentOS Linux, Red Hat Enterprise Linux, Oracle Linux). Please note that only Ubuntu and Fedora support is actively maintained and tested so your milage with other distributions may vary. 

## Building OSv kernel and creating images

Building OSv is as easy as using the shell script [build](https://github.com/cloudius-systems/osv/blob/master/scripts/build)
that orchestrates the build process by delegating to the main [makefile](https://github.com/cloudius-systems/osv/blob/master/Makefile)
to build the kernel and by using number of Python scripts like [module.py](https://github.com/cloudius-systems/osv/blob/master/scripts/module.py) to build application and *fuse* it together with the kernel
into a final image placed at ./build/release/usr.img (or ./build/$(arch)/usr.img in general). Please note that *building app* does
not necessarily mean building from source as in many cases the app files would be simply located on and taken from the Linux build machine
(see [manifest_from_host.sh](https://github.com/cloudius-systems/osv/blob/master/scripts/manifest_from_host.sh) for details).

The build script can be used like so per the examples below:
```bash
# Create default image that comes with command line and REST API server
./scripts/build 

# Create image with native-example app
./scripts/build -j4 fs=rofs image=native-example   

# Create image with spring boot app with Java 10 JRE
./scripts/build JAVA_VERSION=10 image=openjdk-zulu-9-and-above,spring-boot-example

 # Create image with 'ls' executable taken from the host
./scripts/manifest_from_host.sh -w ls && ./script/build --append-manifest                  

# Create test image and run all tests in it
./script/build check                               

# Clean the build tree
./script/build clean                               
```

For details how to use run the build script run ```./scripts/build --help```.

By default OSv builds kernel for x86_64 architecture but it is also possible to build one for ARM by adding **arch** parameter like so:
```bash
./scripts/build arch=aarch64
```
Please note that even though the **aarch64** version of OSv kernel should build fine, most likely it will **not** run as the ARM part of OSv has not been well maintained and tested due to lack of resources. 

For more information about various example apps you can build and run on OSv please read [the osv-apps repo README](https://github.com/cloudius-systems/osv-apps#osv-applications).

## Running OSv

Running an OSv image is as easy as:
```bash
./scripts/run.py
./scripts/firecracker.py #Version of run.py to run OSv on firecracker
``` 

For details how to use the script run ```./scripts/run.py --help```.

By default, the ```run.py``` runs OSv under KVM, with 4 VCPUs and 2GB of memory.
If running under KVM you can terminate by hitting Ctrl+A X.

## External Networking

To start osv with external networking:

```
sudo ./scripts/run.py -n -v
```

The -v is for kvm's vhost that provides better performance
and its setup requires a tap and thus we use sudo.

By default OSv spawns a dhcpd that auto config the virtual nics.
Static config can be done within OSv, configure networking like so:

```
ifconfig virtio-net0 192.168.122.100 netmask 255.255.255.0 up
route add default gw 192.168.122.1
```

Test networking:

```
test invoke TCPExternalCommunication
```

### Testing

- can be debugged with gdb
- traced
- profiled
- REST api monitored

* [Debugging OSv](https://github.com/cloudius-systems/osv/wiki/Debugging-OSv)
* [Trace Analysis](https://github.com/cloudius-systems/osv/wiki/Trace-analysis-using-trace.py)



Move to a separate wiki.


**Fedora**

```
scripts/setup.py
```

**Debian stable(wheezy)**
Debian stable(wheezy) requires to compile gcc, gdb and qemu.
And also need to configure bridge manually.

More details are available on wiki page:
[Building OSv on Debian stable][]

[Building OSv on Debian stable]: https://github.com/cloudius-systems/osv/wiki/Building-OSv-on-Debian-stable

**Debian testing(jessie)**
```
apt-get install build-essential libboost-all-dev genromfs autoconf libtool openjdk-7-jdk ant qemu-utils maven libmaven-shade-plugin-java python-dpkt tcpdump gdb qemu-system-x86 gawk gnutls-bin openssl python-requests lib32stdc++-4.9-dev p11-kit
```

**Arch Linux**
```
pacman -S base-devel git python apache-ant maven qemu gdb boost yaml-cpp unzip openssl-1.0
```

Before start building OSv, you'll need to add your account to kvm group.
```
usermod -aG kvm <user name>
```

**Ubuntu**:

```
scripts/setup.py
```

You may use [Oracle JDK][] if you don't want to pull too many
dependencies for ``openjdk-7-jdk``

[Oracle JDK]: https://launchpad.net/~webupd8team/+archive/java

To ensure functional C++11 support, Gcc 4.8 or above is required, as this was
the first version to fully comply with the C++11 standard.

Make sure all git submodules are up-to-date:

```
git submodule update --init --recursive
```

Finally, build everything at once:

```
make -j$(nproc)
```

to build only the OSv kernel, or more usefully,

```
scripts/build -j$(nproc)
```

to build an image of the OSv kernel and the default application.

Command nproc (embedded in bash/core-utils) will calculte the number of jobs/threads for make and scripts/build automatically.
Alternatively, the environment variable MAKEFLAGS can be exported as follows:

```
export MAKEFLAGS=-j$(nproc)
```

In that case, make and scripts/build do not need the parameter -j.

scripts/build creates the image ```build/last/usr.img``` in qcow2 format.
To convert this image to other formats, use the ```scripts/convert```
tool, which can create an image in the vmdk, vdi, raw, or qcow2-old formats
(qcow2-old is an older qcow2 format, compatible with older versions of QEMU).
For example:

```
scripts/convert raw
```

By default make will use the static libraries and headers of gcc in external submodule. To change this pass `host` via *_env variables:

```
make build_env=host
```

This will use static libraries and headers in the system instead (make sure they are installed before run `make`),
if you only want to use C++ static libraries in the system, just set `cxx_lib_env` to `host`:

```
make cxx_lib_env=host
```

## Running OSv

```
./scripts/run.py
```

By default, this runs OSv under KVM, with 4 VCPUs and 2GB of memory,
and runs the default management application containing a shell, a
REST API server and a browser base dashboard (at port 8000).

If running under KVM you can terminate by hitting Ctrl+A X.


## External Networking

To start osv with external networking:

```
sudo ./scripts/run.py -n -v
```

The -v is for kvm's vhost that provides better performance
and its setup requires a tap and thus we use sudo.

By default OSv spawns a dhcpd that auto config the virtual nics.
Static config can be done within OSv, configure networking like so:

```
ifconfig virtio-net0 192.168.122.100 netmask 255.255.255.0 up
route add default gw 192.168.122.1
```

Test networking:

```
test invoke TCPExternalCommunication
```

## Running Java or C applications that already reside within the image:

```
# Building and running a simple java application example
$ scripts/build image=java-example
$ scripts/run.py -e "java.so -cp /java-example Hello"

# Running an ifconfig by explicit execution of ifconfig.so (compiled C++ code)
$ scripts/build
$ sudo scripts/run.py -nv -e "/tools/ifconfig.so"
```
