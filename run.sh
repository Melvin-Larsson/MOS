make &&
dd if=build/os.bin of=build/os.img conv=notrunc &&
qemu-system-x86_64 -hda build/os.img \
-drive if=none,id=stick,format=raw,file=stick.img \
-device qemu-xhci,id=xhci \
-device usb-storage,bus=xhci.0,drive=stick \
-m 4G \
-d guest_errors -d unimp -d pcall -d strace \
-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
--trace events=trace.event


#-device usb-kbd,bus=xhci.0 \
