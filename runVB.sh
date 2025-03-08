make &&
rm -f build/os.vdi &&
VBoxManage convertfromraw --uuid=fc96f184-385b-4757-9137-d82f51422b3e build/fat32.img build/os.vdi &&
VBoxManage discardstate "Test" &&
VBoxHeadless --startvm Test
