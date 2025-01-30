# An operating system?
Or... at least some parts of an operating system that I found interesting at the time.

# Features
The following features have been worked on, enough to get them running on my machines and/or qemu, but with many shortcuts and assumptions about the hardware. So it is unlikely that this will work on other machines.

- Usb drivers (using xHCI controller)
  - Keyboard
  - Usb mass storage devices using SCSI (currently, this only works in qemu)
- Multithreading
- Serial
- FAT32 filesystem (At least the most important parts. Only FAT32 is tested)
- Paging, logging, memory management, timers, pci, interrupts, msi/msi-x, bootloader, test harness and other stuff necessary to get the features above to work

# How to build
**Don't.** This has not been a consideration, the build system works on my machines and that is enough for me. Also, this is just bits and pieces of an OS, and there is no real "glue" in between them, in order to see certain features, ```main.c``` must be modified. 

But... [this](https://wiki.osdev.org/GCC_Cross-Compiler) is how to compile the cross-compiler, and the [run.sh](https://github.com/Melvin-Larsson/BBP/blob/master/run.sh) might be useful when running in qemu.
**Don't run [build.sh](https://github.com/Melvin-Larsson/BBP/blob/master/build.sh), it is dangerous**.

Also, you need to set the ```PREFIX``` and the ```COMPILER``` environmental variables:
```bash
export PREFIX="{...}/BBP"
export COMPILER="{..}/bin/i686-elf-gcc"
```

 The [Dockerfile](https://github.com/Melvin-Larsson/BBP/blob/master/Dockerfile) might be useful too. 
