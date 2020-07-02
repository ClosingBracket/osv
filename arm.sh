#
# ./scripts/build image=empty fs=ramfs -j4 arch=aarch64
# ./scripts/build image=empty fs=rofs -j4 arch=aarch64 --create-disk
qemu-system-aarch64 \
-m 1G \
-smp 2 \
--nographic \
-gdb tcp::1234,server,nowait \
-kernel /home/wkozaczuk/projects/osv/scripts/../build/last/loader.img \
-append "--disable_rofs_cache /tools/hello.so" \
-machine virt \
-machine gic-version=2 \
-cpu cortex-a57 \
-device virtio-blk-pci,id=blk0,drive=hd0,scsi=off \
-drive file=/home/wkozaczuk/projects/osv/build/release/disk.img,if=none,id=hd0,cache=none,aio=native \
-netdev user,id=un0,net=192.168.122.0/24,host=192.168.122.1 \
-device virtio-net-pci,netdev=un0 \
-device virtio-rng-pci
