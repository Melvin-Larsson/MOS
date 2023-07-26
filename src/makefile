myos.bin : main.o stdio.o strin
	$HOME/opt/cross2/bin/i686-elf-gcc -T linker.ld -o myos.elf -ffreestanding -O2 -nostdlib boot.o main.o stdio.o string.o -lgccg.o
main.o main.c
	$HOME/opt/cross2/bin/i686-elf-gcc -mno-red-zone -c kernel/main.c -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	

	

