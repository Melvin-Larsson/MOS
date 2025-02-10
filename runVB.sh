make &&
dd if=build/os.bin of=build/os.img conv=notrunc &&
rm -f build/os.vdi &&
VBoxManage convertfromraw --uuid=fc96f184-385b-4757-9137-d82f51422b3e build/os.img build/os.vdi &&
VBoxManage discardstate "Test" &&
VBoxHeadless --startvm Test
