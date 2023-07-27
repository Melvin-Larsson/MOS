#include "stdio.h"
#include "kernel/interrupt.h"

char message[] = "Kernel started!\n";
void kernel_main(){
    stdioinit();
    printf(message);
    interruptDescriptorTableInit(); 
    while(1){
        printf("end\n");
    }
}
