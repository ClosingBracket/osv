export OSV_BRIDGE=virbr0
KERNEL="/home/wkozaczuk/projects/osv/build/last/loader-stripped.elf"
#/home/wkozaczuk/projects/qemu/bin/release/native/x86_64-softmmu/qemu-system-x86_64 -M microvm \
#isa-serial=off DOES not work
#strace -tt /home/wkozaczuk/projects/qemu/bin/release/native/x86_64-softmmu/qemu-system-x86_64 -M microvm,x-option-roms=off,pit=off,pic=off,rtc=off \
time /home/wkozaczuk/projects/qemu/bin/release/native/x86_64-softmmu/qemu-system-x86_64 -M microvm,x-option-roms=off,pit=off,pic=off,rtc=off \
   -enable-kvm -cpu host,+x2apic -m 64m -smp 1 \
   -nodefaults -no-user-config -nographic -no-reboot \
   -kernel "$KERNEL" -append "--nopci /hello" \
   -gdb tcp::1234,server,nowait \
   -serial stdio \
   -global virtio-mmio.force-legacy=off \
   -device virtio-blk-device,id=blk0,drive=hd0,scsi=off \
   -drive file=/home/wkozaczuk/projects/osv/build/last/usr.img,if=none,id=hd0,cache=none
#   -netdev tap,id=tap0,script=/home/wkozaczuk/projects/osv/scripts/qemu-ifup.sh,downscript=no \
#   -device virtio-net-device,netdev=tap0

#   -netdev tap,id=tap0,script=/home/wkozaczuk/projects/osv/scripts/qemu-ifup.sh,downscript=no \
#   -device virtio-net-device,netdev=tap0

#   -netdev tap,id=fc_tap0,script=no,downscript=no \
#   -device virtio-net-device,netdev=fc_tap0

#   -netdev tap,id=tap0,script=no,downscript=no \
#   -device virtio-net-device,netdev=tap0

#   -kernel "$KERNEL" -append "--verbose --nopci /hello" \
#   -kernel "$KERNEL" -append "--nopci --verbose /go.so /httpserver.so" \
#   -kernel "$KERNEL" -append "--ip=eth0,172.16.0.2,255.255.255.252 --defaultgw=172.16.0.1 --nameserver=172.16.0.1 --nopci --verbose /go.so /httpserver.so" \

# --------------
# -nopci reduces boot by 50 ms !!!!!!!!!!!!!!!!!!
# -global virtio-mmio.force-legacy=off \
