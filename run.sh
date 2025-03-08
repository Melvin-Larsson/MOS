make &&
qemu-system-x86_64 -hda build/fat32.img \
-device qemu-xhci,id=xhci \
-drive if=none,id=stick,format=raw,file=stick.img \
-device usb-storage,bus=xhci.0,drive=stick \
-m 4G \
-d guest_errors -d unimp -d pcall -d strace \
-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
--trace events=trace.event \
-d int,guest_errors -no-reboot

# -device usb-kbd,bus=xhci.0 \
