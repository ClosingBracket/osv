***OSv was originally designed and implemented by Cloudius Systems (now ScyllaDB) however
 currently it is being maintained and enhanced by a small community of volunteers.
 If you are into systems programming or want to learn and help us improve OSv please
 contact us on [OSv Google Group forum](https://groups.google.com/forum/#!forum/osv-dev).
 For details on how to format and send patches, please read 
 [this wiki](https://github.com/cloudius-systems/osv/wiki/Formatting-and-sending-patches) 
 (__we do NOT accept pull requests__).***

# OSv

OSv is an open-source versatile modular **unikernel** designed to run **unmodified
Linux applications** securely on micro-VMs in the cloud. Built from
the ground up for effortless deployment and management of microservices
and serverless apps, with superior performance.

OSv has been designed to run unmodified x86-64 Linux
binaries **as is**, which effectively makes it a **Linux binary compatible unikernel**
(for more details about Linux ABI compatibility please read
[this doc](https://github.com/cloudius-systems/osv/wiki/OSv-Linux-ABI-Compatibility)).
In particular OSv can run many managed language runtimes including
[**JVM**](https://github.com/cloudius-systems/osv-apps/tree/master/java-example),
**Python** [**2**](https://github.com/cloudius-systems/osv-apps/tree/master/python2x) and
[**3**](https://github.com/cloudius-systems/osv-apps/tree/master/python3x),
[**Node.JS**](https://github.com/cloudius-systems/osv-apps/tree/master/node-from-host),
[**Ruby**](https://github.com/cloudius-systems/osv-apps/tree/master/ruby-example), **Erlang**, 
and applications built on top of one.
It can also run applications written in languages compiling directly to native machine code like
**C**, **C++**, 
[**Golang**](https://github.com/cloudius-systems/osv-apps/tree/master/golang-httpserver)
and [**Rust**](https://github.com/cloudius-systems/osv-apps/tree/master/rust-httpserver) 
as well as native images produced
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

In order to run an application on OSv, one needs to build an image by fusing OSv kernel, and
the application files together. This, in high level can be achieved in two ways:
- by using the script [build](https://github.com/cloudius-systems/osv/blob/master/scripts/build)
 that builds the kernel from source and fuses it with application files
- by using the [capstan tool](https://github.com/cloudius-systems/capstan) that uses *pre-built
 kernel* and combines it with application files to produce final image

If your intention is to try to run your app on OSv with the least effort possible, you should pursue the *capstan*
route. For introduction please read this 
[crash course](https://github.com/cloudius-systems/osv/wiki/Build-and-run-apps-on-OSv-using-Capstan).
For more details about *capstan* please read 
[this documentation](https://github.com/cloudius-systems/capstan#documentation). Pre-built OSv kernel files
(`ovs-loader.qemu`) can be downloaded using *capstan* from 
[the OSv regular releases page](https://github.com/cloudius-systems/osv/releases) or manually from 
[the nightly releases repo](https://github.com/osvunikernel/osv-nightly-releases/releases/tag/ci-master-latest).

If you are comfortable with make and GCC toolchain and want to try the latest OSv code, then you should
read [this part of the page](#setting-up-development-environment) to guide you how to set up your
 development environment and build OSv kernel and application images.

## Releases

We aim to release OSv 2-3 times a year. You can find [the latest one on github](https://github.com/cloudius-systems/osv/releases)
along with number of published artifacts including kernel and some modules.

In addition, we have set-up [Travis-based CI/CD pipeline](https://travis-ci.org/github/cloudius-systems/osv) where each
commit to the master and ipv6 branches triggers full build of the latest kernel and publishes some artifacts to 
[the "nightly releases repo"](https://github.com/osvunikernel/osv-nightly-releases/releases). Each commit also
triggers publishing of new Docker "build tool chain" images to [the Docker hub](https://hub.docker.com/u/osvunikernel).

## Design

Good bit of the design of OSv is pretty well explained in 
[the Components of OSv](https://github.com/cloudius-systems/osv/wiki/Components-of-OSv) wiki page. You 
can find even more information in the original 
[USENIX paper and its presentation](https://www.usenix.org/conference/atc14/technical-sessions/presentation/kivity).

In addition, you can find a lot of good information about design of specific OSv components on
[the main wiki page](https://github.com/cloudius-systems/osv/wiki) and http://osv.io/ and http://blog.osv.io/.
Unfortunately some of that information may be outdated (especially on http://osv.io/), so it is always
best to ask on [the mailing list](https://groups.google.com/forum/#!forum/osv-dev) if in doubt.

## Metrics and Performance

There are no official **up-to date** performance metrics comparing OSv to other unikernels or Linux.
In general file I/O heavy not very good primarily due to coarse-grain locking in VFS around read/write operation [find issue],
 but network I/O should be better. You can find some old "numbers" on the wiki, http://osv.io/ and some papers listed below.
 So OSv is probably not best suited to run MySQL or ElasticSearch, but should be pretty for general stateless
 apps [Improve].

### Kernel Size

At this moment (as of May 2020) the size of the uncompressed OSv kernel (`kernel.elf` artifact) is around
6.7 MB (the compressed is ~ 2.7 MB). Which is not that small comparing to Linux kernel or very large comparing
to other unikernels. However, bear in mind that OSv kernel (being unikernel) provides **subset** of functionality
 of the following Linux libraries (see their approximate size on Linux host):
- `libresolv.so.2` (_100 K_)
- `libc.so.6` (_2 MB_)
- `libm.so.6` (_1.4 MB_)
- `ld-linux-x86-64.so.2` (_184 K_)
- `libpthread.so.0` (_156 K_)
- `libdl.so.2` (_20 K_)
- `librt.so.1` (_40 K_)
- `libstdc++.so.6` (_2 MB_)
- `libaio.so.1` (_16 K_)
- `libxenstore.so.3.0` (_32 K_)
- `libcrypt.so.1` (_44 K_)

The equivalent static version of `libstdc++.so.6` is actually linked `--whole-archive` so that
any C++ apps can run without having to add `libstdc++.so.6` to the image. Finally, OSv kernel
comes with ZFS implementation which in theory later can be extracted as a separate library. The
point of this is to illustrate that comparing OSv kernel size to Linux kernel size does not
quite make sense.

### Boot Time

OSv with _Read-Only FS with networking off_ can boot as fast as **~5 ms** on Firecracker 
and even faster around **~3 ms** on QEMU with the microvm machine. However, in general the boot time
will depend on many factors like hypervisor including settings of individual para-virtual devices, 
filesystem (ZFS, RoFS or RAMFS) and some boot parameters. Please note that by default OSv images
get built with ZFS filesystem, but you can change it to other one using `fs` parameter of the `scripts/build`.

For example, the boot time of ZFS image on Firecracker is around ~40 ms and regular QEMU around 200 ms these days. Also,
newer versions of QEMU (>=4.0) are typically faster to boot. Booting on QEMU in PVH/HVM mode (aka direct kernel boot, enabled 
by `-k` option of `run.py`) should always be faster as it directly jumps to 64-bit long mode. Please see
[this Wiki](https://github.com/cloudius-systems/osv/wiki/OSv-boot-methods-overview) for the brief review of the boot
modes OSv supports.  

Finally, some boot parameters passed to the kernel may affect the boot time:
- `--console serial`
- `--nopci`
- `--redirect=/tmp/out`

You can always see boot time breakdown by adding `--bootchart` parameter:

### Memory Utilization

## Testing

Mention docker runner

## Setting up Development Environment

OSv can only be built on a 64-bit x86 Linux distribution. Please note that
this means the "x86_64" or "amd64" version, not the 32-bit "i386" version.

In order to build OSv kernel you need a physical or virtual machine with Linux distribution on it and GCC toolchain and
all necessary packages and libraries OSv build process depends on. The easiest way to set it up is to use
[Docker files](https://github.com/cloudius-systems/osv/tree/master/docker#docker-osv-builder) that OSv comes with.
You can use them to build your own Docker image and then start it in order to build OSv kernel inside of it.
Alternatively, you may find it even easier to pull pre-built base **Docker images** for 
[Ubuntu](https://hub.docker.com/repository/docker/osvunikernel/osv-ubuntu-19.10-builder-base) 
and [Fedora](https://hub.docker.com/repository/docker/osvunikernel/osv-fedora-31-builder-base) 
from Docker hub that get rebuilt and published to upon every commit. 

Otherwise, you can manually clone OSv repo and use [setup.py](https://github.com/cloudius-systems/osv/blob/master/scripts/setup.py)
to install GCC and all required packages, as long as it supports your Linux distribution, and you have both git 
and python 3 installed on your machine:
```bash
git clone https://github.com/cloudius-systems/osv.git
cd osv && git submodule update --init --recursive
./scripts/setup.py
```

The `setup.py` recognizes and installs packages for number of Linux distributions including Fedora, Ubuntu, 
[Debian](https://github.com/cloudius-systems/osv/wiki/Building-OSv-on-Debian-stable), LinuxMint and RedHat ones 
(Scientific Linux, NauLinux, CentOS Linux, Red Hat Enterprise Linux, Oracle Linux). Please note that only Ubuntu and Fedora 
support is actively maintained and tested, so your mileage with other distributions may vary.

## Building OSv Kernel and Creating Images

Building OSv is as easy as using the shell script [build](https://github.com/cloudius-systems/osv/blob/master/scripts/build)
that orchestrates the build process by delegating to the main [makefile](https://github.com/cloudius-systems/osv/blob/master/Makefile)
to build the kernel and by using number of Python scripts like [module.py](https://github.com/cloudius-systems/osv/blob/master/scripts/module.py) 
to build application and *fuse* it together with the kernel
into a final image placed at ./build/release/usr.img (or ./build/$(arch)/usr.img in general). Please note that *building app* does
not necessarily mean building from source as in many cases the app files would be simply located on and taken from the Linux build machine
(see [this Wiki page](https://github.com/cloudius-systems/osv/wiki/Running-unmodified-Linux-executables-on-OSv) for details).

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

Command nproc will calculate the number of jobs/threads for make and scripts/build automatically.
Alternatively, the environment variable MAKEFLAGS can be exported as follows:

```
export MAKEFLAGS=-j$(nproc)
```

In that case, make and scripts/build do not need the parameter -j.

For details on how to use the build script, please run `./scripts/build --help`.

The `.scripts/build` creates the image `build/last/usr.img` in qcow2 format.
To convert this image to other formats, use the `scripts/convert`
tool, which can create an image in the vmdk, vdi or raw formats.
For example:

```
scripts/convert raw
```

By default, OSv kernel gets built for x86_64 architecture, but it is also possible
 to build one for ARM by adding **arch** parameter like so:
```bash
./scripts/build arch=aarch64
```
At this point cross-compiling the **aarch64** version of OSv is only supported on Fedora and relevant aarch64 gcc and libraries'
binaries can be downloaded using [this script](https://raw.githubusercontent.com/cloudius-systems/osv/master/scripts/download_fedora_aarch64_packages.py).
Please note that simple "hello world" app should work just fine, but overall the ARM part of OSv has not been
 as well maintained and tested as x86_64 due to the lack of volunteers. 
 For more information about the aarch64 port please read [this Wiki page](https://github.com/cloudius-systems/osv/wiki/AArch64).
Mention Raspberry Pi 4.

### Filesystems
 ZFS, ROFS, RAMFS

## Running OSv

Running an OSv image, built by `scripts/build`, is as easy as:
```bash
./scripts/run.py
```

By default, the `run.py` runs OSv under KVM, with 4 vCPUs and 2 GB of memory. 
You can control these and tens of other ones by passing relevant parameters to the `run.py`. For details on how to use the script please
 run `./scripts/run.py --help`.

The `run.py` can run OSv image on QEMU/KVM, Xen and VMware. If running under KVM you can terminate by hitting Ctrl+A X.

Alternatively you can use `./scripts/firecracker.py` to run OSv on [Firecracker](https://firecracker-microvm.github.io/). 
This script automatically downloads firecracker and accepts number of parameters like number ot vCPUs, memory named exactly like `run.py` does.

### Mention direct kernel boot Firecraker and QEMU PVH

Please note that in order to run OSv with the best performance on Linux under QEMU or Firecracker you need KVM enabled 
(this is only possible on *physical* Linux machines, EC2 bare metal instances or VMs that support nested virtualization with KVM on). 
The easiest way to verify KVM is enabled is to check if `/dev/kvm` is present, and your user can read from and write to it. 
Adding your user to the kvm group may be necessary like so:
```bash
usermod -aG kvm <user name>
```

For more information about building and running JVM, Node.JS, Python and other managed runtimes as well as Rust, Golang or C/C++ apps
 on OSv, please read this [wiki page](https://github.com/cloudius-systems/osv/wiki#running-your-application-on-osv). 
 For more information about various example apps you can build and run on OSv, please read 
 [the osv-apps repo README](https://github.com/cloudius-systems/osv-apps#osv-applications).

### Networking

By default, the `run.py`  starts OSv with
 [user networking/SLIRP](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29) on. 
To start OSv with more performant external networking:

```
sudo ./scripts/run.py -n -v
```

The -v is for kvm's vhost that provides better performance
and its setup requires tap device and thus we use sudo.

By default, OSv spawns a dhcpd-like thread that automatically configures virtual NICs.
A static configuration can be done within OSv, configure networking like so:

```
ifconfig virtio-net0 192.168.122.100 netmask 255.255.255.0 up
route add default gw 192.168.122.1
```

Test networking:

```
test invoke TCPExternalCommunication
```

## Debugging, Monitoring, Profiling OSv

- OSv can be debugged with gdb; for more details please read this
 [wiki](https://github.com/cloudius-systems/osv/wiki/Debugging-OSv)
- OSv kernel and application can be traced and profiled; for more details please read 
this [wiki](https://github.com/cloudius-systems/osv/wiki/Trace-analysis-using-trace.py)
- OSv comes with the admin/monitoring REST API server; for more details please read 
[this](https://github.com/cloudius-systems/osv/wiki/Command-Line-Interface-(CLI)) and
 [that wiki page](https://github.com/cloudius-systems/osv/wiki/Using-OSv-REST-API).
