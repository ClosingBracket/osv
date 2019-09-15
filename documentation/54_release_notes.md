This new release of OSv focuses on improving Linux compatibility and tooling aimed to make it possible to run unmodified Linux apps on OSv "as-is".

# Overview
From the beginning, OSv was designed to implement a subset of Linux POSIX API superset. But until this release most Linux applications had to be **re-compiled from source** as shared libraries or some, like Java, rely on OSv version of /usr/bin/java wrapper to run. This meant that one could NOT run a Linux executable "as is". In other words, OSv has always been *Linux-compatible at source level but not at binary level*.

This release offers a breakthrough and allows running unmodified Linux **position-independent executables** (so-called "pies") and **position-dependant executables** "as-is" as long as they *do not use "fork/execve" or other unsupported Linux API*. It means that very often one can take a **binary from Linux host and run it on OSv** without having to locate the source code on the Internet and build it as shared library.

In addition, this release makes OSv more Linux-compatible from another end - booting on a hypervisor. The previous release 0.53 made OSv kernel *"look like"* ELF64 uncompressed Linux kernel. The new release 0.54 has enhanced OSv loader to *"look like"* **vmlinuz** and thus allow booting on Docker's **Hyperkit** on OSX. The OSv loader has also been enhanced to boot as Linux ELF64 **PVH/HVM** loader on QEMU with `--kernel` option.

Finally two new 
Mention tooling - manifest_from_host.sh and build_capstan_mpm_ (better capstan). Example to run docker image.

# Highlights
### Linux compatibility
* Applications
    * Enhanced **getopt** family of functions to work correctly with both **position-independent executables** and **position-dependent executables** in order to allow receiving program arguments
    * Enhanced dynamic linker to be capable of executing **position-dependent executables**
    * Mapped kernel higher in virtual memory - from 0x00200000 to **0x40200000** (2nd GiB) in order to make space for position-dependent executables
    * Added new GNU libc extensions: `error()`, `__prognames` and `__progname_full`
    * Added missing pseudo-files to procfs and minimal implementation of **sysfs** in order to support **libnuma** to allow programs like ffmpeg using x265 codec run on OSv "as-is"
    * Encanced `/proc/self/maps` to include i-node number and device ID to support GraalVM apps with isolates
    * Enhanced `epoll_pwait()` implementation
    * Improved dynamic linker by making it:
      * Ignore old version symbols so that new version symbols are resolved correctly instead
      * Delay resolving symbols found missing during `relocate_rela()` phase for certain relocation types to allow more unmodified Linux executables run on OSv
      * Handle DT_RUNPATH
* Booting    
    * Added vmlinuz-compatible version of kernel to allow OSv boot on Docker's Hyperkit 
    * Enhanced loader to support [PVH/HVM](https://patchwork.kernel.org/patch/10741013/) boot to allow OSV run on QEMU with `--kernel` option
    * Added support of QEMU 4.x
    * Enhanced HPET driver to support 32-bit main counter
### Filesystem improvements
* VFS
    * Hardened implementation of `open()/sys_open()/task_conv()` to handle null path
    * Enhanced `__fxstata` to handle `AT_SYMLINK_NOFOLLOW`
* RAMFS 
    * Greatly improved speed of write/append operations 
    * Fixed bugs
        * Delay freeing data until i-node closed
        * Keep i-node number the same
### Tools
 * Added script `manifest_from_host.sh` to allow building images from artifacts on host “as-is” without need to compile
 * Added script `build-capstan-mpm-packages` to create capstan MPM packages
 * Added Ubuntu- and Fedora-based Docker files to help create build and test environment
 * Enhanced `test.py` to allow executing unit tests on Firecracker
### Bugs and other enhancements
 * Fixed `sem_trywait()` that for example allows Java 12 run properly on OSv
 * Improved memory utilization by using memory below kernel
 * Introduced new command line suffix `!` allowing to force termination of lingering threads
 * Revamped building the cli and httpserver apps to use OpenSSL 1.1 and Lua 5.3 and minimize compilation 
 * Tweaked OSv code to support compilation by GCC 9
### Improved Documentation
 * Refreshed main README
 * OSv-apps
 * Scripts   
### Apps
 * From Docker image demo app
 * GraalVM isolates
 * Can run many core-utils (ls, cat, find, tree, …)
 * Support Mono
 * Improve suppoort of Golang PIEs 
 * Can run unmodified Linux executables (from host):
   * Java
   * Python 2 and 3
   * Node
   * Lua
   * Ffmpeg
   * MySQL on RAMFS

# Closed issues
* #1050 - Can't run anything with 1.01G of memory
* #1049 - tst_huge hangs with memory over 1GB.
* #1048 - VM with memory larger than 4GB doesn't boot
* #1043 - Map kernel higher in virtual memory
* #1039 - Handle new DT_RUNPATH in object::load_needed()
* #1035 - iperf3 fails with exception nested to deeply on ROFS/RamFS image
* #1034 - Build failures when build directory's pathname has a space
* #1031 - The graalvm-example fails with graalvm 1.0.0-rc13
* #1026 - golang-pie-httpserver crashes on control-C
* #1023 - Ignore missing symbols when loading objects with BIND_NOW in relocate_rela()
* #1022 - lua package requires openssl 1.0
* #1012 - Improve physical memory utilization by using memory below 2MB
* #884 - slow write/append to files on ramfs
* #689 - PIE applications using "optarg" do not work on newer gcc
* #561 - OSv failed to run a pthread application.
* #534 - imgedit.py can't always connect to qemu-nbd
* #305 - Fail to run iperf3 on osv
* #190 - Allow running a single unmodified regular (non-PIE) Linux executable
* #34 - Mono support

# Commits by author
##### KANATSU Minoru (1):
* [libc: add __explicit_bzero_chk()](https://github.com/cloudius-systems/osv/commit/982acdbd)

##### Nadav Har'El (9):
* [scripts/build: gracefully handle spaces in image= parameter](https://github.com/cloudius-systems/osv/commit/3f7ce190)
* [build: don't fail build if pathname has space](https://github.com/cloudius-systems/osv/commit/15c8ff7d)
* [trace.py: fix failure on newest Python](https://github.com/cloudius-systems/osv/commit/8cd7d8aa)
* [tracepoints: fix for compiling on gcc 9](https://github.com/cloudius-systems/osv/commit/cb96e938)
* [sched: fix gcc 9 warning](https://github.com/cloudius-systems/osv/commit/27cf8784)
* [libc: avoid weak_alias() warnings from gcc 9.](https://github.com/cloudius-systems/osv/commit/51722a95)
* [acpi: ignore new gcc 9 warning](https://github.com/cloudius-systems/osv/commit/c4c155c9)
* [imgedit.py: do not open a port to the entire world](https://github.com/cloudius-systems/osv/commit/e37d8edb)
* [sched.hh: add missing include](https://github.com/cloudius-systems/osv/commit/9e34f428)

##### Waldemar Kozaczuk (86):
* [Added initial version of README under scripts directory](https://github.com/cloudius-systems/osv/commit/14b2fe27)
* [Lowered default ZFS qcow2 image size from 10GB to to 256MB](https://github.com/cloudius-systems/osv/commit/979f685c)
* [Add script to setup external bridge](https://github.com/cloudius-systems/osv/commit/e36de53b)
* [Refactor and enhance firecracker script](https://github.com/cloudius-systems/osv/commit/053e0a40)
* [Changed loader to print total boot time by default](https://github.com/cloudius-systems/osv/commit/c8395118)
* [Enhanced setup.py to support Ubuntu 18.10 and 19.04](https://github.com/cloudius-systems/osv/commit/e37bbc91)
* [Add GNU libc extension function error()](https://github.com/cloudius-systems/osv/commit/3b606e9a)
* [Add GNU libc extension variables __progname and __progname_full](https://github.com/cloudius-systems/osv/commit/4e425d49)
* [Update nbd_client.py to support both old- and new-style handshake](https://github.com/cloudius-systems/osv/commit/fcfa8770)
* [Simplify building images out of artifacts found on host filesystem](https://github.com/cloudius-systems/osv/commit/83128b2b)
* [Move getopt* files to libc folder and convert to C++](https://github.com/cloudius-systems/osv/commit/7a4d96b6)
* [Enhance getopt family of functions to work with PIEs](https://github.com/cloudius-systems/osv/commit/e688e21f)
* [Tweaked nbd_client.py to properly handle handshake and transport flags in new handshake protocol](https://github.com/cloudius-systems/osv/commit/9cb7e73e)
* [Tweak open() and sys_open() to return EFAULT error when pathname null](https://github.com/cloudius-systems/osv/commit/a0dcf853)
* [elf: handle new DT_RUNPATH](https://github.com/cloudius-systems/osv/commit/31926c3d)
* [Enhanced __fxstata to handle AT_SYMLINK_NOFOLLOW](https://github.com/cloudius-systems/osv/commit/edcf2593)
* [vfs: Harden task_conv() to return EFAULT when cpath argument is null](https://github.com/cloudius-systems/osv/commit/7bc0155d)
* [Added option suffix "!" to force termination of remaining application threads](https://github.com/cloudius-systems/osv/commit/b4a04221)
* [Move _post_main invocation to run_main](https://github.com/cloudius-systems/osv/commit/5deec8ff)
* [Provide full implementation of epoll_pwait](https://github.com/cloudius-systems/osv/commit/c1b8059b)
* [Start using memory below kernel](https://github.com/cloudius-systems/osv/commit/97fe8aa3)
* [Change vmlinux_entry64 to switch to protected mode and jump to start32](https://github.com/cloudius-systems/osv/commit/414a54cb)
* [Fixed indentation in xen.cc](https://github.com/cloudius-systems/osv/commit/e1260d56)
* [Move kernel to 0x40200000 address (1 GiB higher) in virtual memory](https://github.com/cloudius-systems/osv/commit/2a1795db)
* [Allow running non-PIE executables that do not collide with kernel](https://github.com/cloudius-systems/osv/commit/1e22a86f)
* [Make RAMFS not to free file data when file is still opened](https://github.com/cloudius-systems/osv/commit/2e45b337)
* [Fix slow write/append to files on ramfs](https://github.com/cloudius-systems/osv/commit/6a5784cc)
* [hpet: Support 32-bit counter](https://github.com/cloudius-systems/osv/commit/4c2c72df)
* [mprotect: page-align len parameter instead of returning error](https://github.com/cloudius-systems/osv/commit/276aa1bf)
* [procfs: populate maps file with i-node numbers](https://github.com/cloudius-systems/osv/commit/6f8d6cdd)
* [procfs: Add device ID information to the maps file](https://github.com/cloudius-systems/osv/commit/ef0696c5)
* [syscall: add getpid](https://github.com/cloudius-systems/osv/commit/0dd2ce0f)
* [signal: tag user handler thread as an application one](https://github.com/cloudius-systems/osv/commit/33bea608)
* [Make OSv boot as vmlinuz](https://github.com/cloudius-systems/osv/commit/1e460f59)
* [Prepare for local-exec TLS patch](https://github.com/cloudius-systems/osv/commit/b04d8fdd)
* [Clean boot logic from redundant passing OSV_KERNEL_BASE](https://github.com/cloudius-systems/osv/commit/dea2e307)
* [Support PVH/HVM direct kernel boot](https://github.com/cloudius-systems/osv/commit/68727220)
* [Enhance scripts/test.py to allow running unit tests on firecracker](https://github.com/cloudius-systems/osv/commit/d8366282)
* [Refine confstr() to conform more closely to Linux spec](https://github.com/cloudius-systems/osv/commit/e9ef51b2)
* [Enhanced manifest_from_host.sh to support building apps from host and docker images](https://github.com/cloudius-systems/osv/commit/9f1f6456)
* [Fixed compilation errors in modules mostly related to strlcpy](https://github.com/cloudius-systems/osv/commit/7eda847a)
* [Enhance scripts/build to allow passing arguments to modules/apps](https://github.com/cloudius-systems/osv/commit/0e271f0d)
* [Remove obsolete loader.bin build artifact and related source files](https://github.com/cloudius-systems/osv/commit/2a34f9f2)
* [Add --help|-h option to build script to explain usage](https://github.com/cloudius-systems/osv/commit/513e776b)
* [Remove reference to external from httpserver-api makefile](https://github.com/cloudius-systems/osv/commit/b0f78ec9)
* [Removed remains of externals reference from httpserver-api Makefile](https://github.com/cloudius-systems/osv/commit/0b411c99)
* [scripts: hardened manifest_from_host.hs to verify lddtree is installed on the system](https://github.com/cloudius-systems/osv/commit/160aec3a)
* [scripts: remove old Ubuntu and Fedora from setup.py; added support of Fedora 29](https://github.com/cloudius-systems/osv/commit/e5880d0b)
* [scripts: fixed typo in setup.py](https://github.com/cloudius-systems/osv/commit/a31a1aa8)
* [Reverted some changes related to upgrading openssl that got checked in prematurely](https://github.com/cloudius-systems/osv/commit/fef26ec0)
* [scripts: Enhanced manifest_from_host.sh to better support regular expressions and filter x86_64 ELF files](https://github.com/cloudius-systems/osv/commit/cbc423f3)
* [Fixed compilation errors in modules httpserver-jolokia-plugin, josvsym and monitoring-agent mostly related to strlcpy](https://github.com/cloudius-systems/osv/commit/df6c2358)
* [Fixed httpserver file system integration test](https://github.com/cloudius-systems/osv/commit/ee7a2cd4)
* [ramfs: fix arithmetic bug leading to write overflows](https://github.com/cloudius-systems/osv/commit/d7054e0b)
* [scheduler: Initialize _cpu field in detached_state struct](https://github.com/cloudius-systems/osv/commit/ef56fde7)
* [semaphores: fix sem_trywait](https://github.com/cloudius-systems/osv/commit/6b459dd9)
* [pthreads: implement pthread_attr_getdetachstate](https://github.com/cloudius-systems/osv/commit/5d0c8b7a)
* [pthreads: make implementation of pthread_attr_getdetachstate more correct](https://github.com/cloudius-systems/osv/commit/66ab7975)
* [java: add basic java test that does NOT use OSv wrapper](https://github.com/cloudius-systems/osv/commit/b619927e)
* [Made maven more quiet and only show errors](https://github.com/cloudius-systems/osv/commit/5eb8f9eb)
* [java: tweaked openjdk7 to add /usr/bin/java symlink](https://github.com/cloudius-systems/osv/commit/2e71ce34)
* [setup: add pax-utils package for Ubuntu and Fedora](https://github.com/cloudius-systems/osv/commit/18df2ad2)
* [Add docker files to help setup OSv build environment for Ubuntu/Fedora](https://github.com/cloudius-systems/osv/commit/0aad78b3)
* [Upgrade cli, lua and httpserver-api modules to use OpenSSL 1.1 and Lua 5.3](https://github.com/cloudius-systems/osv/commit/ad1eda6a)
* [dynamic linker: adjust message when symbol missing](https://github.com/cloudius-systems/osv/commit/d8453982)
* [docs: Updated main README to make it better reflect current state of OSv](https://github.com/cloudius-systems/osv/commit/783a49a5)
* [hpet: handle wrap-around with 32-bit counter](https://github.com/cloudius-systems/osv/commit/f7b6bee5)
* [Fix bug in arch_setup_free_memory](https://github.com/cloudius-systems/osv/commit/5377a50b)
* [memory: enforce physical free memory ranges do not start at 0](https://github.com/cloudius-systems/osv/commit/fb7ef9a7)
* [pthreads: provide minimal implementation to handle SCHED_OTHER policy](https://github.com/cloudius-systems/osv/commit/d39860d1)
* [memory setup: ignore 0-sized e820 region](https://github.com/cloudius-systems/osv/commit/7114af21)
* [libc: added __exp2_finite wrapper needed by newer libx265](https://github.com/cloudius-systems/osv/commit/8cb3ffe3)
* [elf: skip old version symbols during lookup](https://github.com/cloudius-systems/osv/commit/ed1eed7a)
* [syscall: add set_mempolicy and sched_setaffinity needed by libnuma](https://github.com/cloudius-systems/osv/commit/9feea361)
* [Ignore missing symbols when processing certain relocation types on load](https://github.com/cloudius-systems/osv/commit/89472ab5)
* [procfs: add minimum subset of status file intended for linuma consumption](https://github.com/cloudius-systems/osv/commit/70c72b31)
* [fs: extracted common pseudofs logic](https://github.com/cloudius-systems/osv/commit/30c9e8a0)
* [fs: add subset of sysfs implementation needed by numa library](https://github.com/cloudius-systems/osv/commit/d14a5dca)
* [ramfs: make sure to pass absolute paths for mkbootfs.py](https://github.com/cloudius-systems/osv/commit/7635774c)
* [ramfs: make sure i-node number stay the same after node allocation](https://github.com/cloudius-systems/osv/commit/0b75cf74)
* [scripts: fix export_manifest.py to handle all symlinks properly](https://github.com/cloudius-systems/osv/commit/4120ae29)
* [scripts: enhance manifest_from_host.sh to put cmdline example for executables](https://github.com/cloudius-systems/osv/commit/0d913b6d)
* [httpserver: make test images use openjdk8 as new jetty app requires it](https://github.com/cloudius-systems/osv/commit/d49d29db)
* [httpserver: enhance test script to accept different location of test image](https://github.com/cloudius-systems/osv/commit/8dbf5fe7)
* [loader: print boot command line and expanded runscript line if applicable](https://github.com/cloudius-systems/osv/commit/dfc89490)
* [capstan: add script to automate building capstan MPM packages](https://github.com/cloudius-systems/osv/commit/e0831468)
