#include "stdio.h"
#include "stdint.h"

int main(int argc, char **args){
    if(argc != 3){
        printf("Invalid number of arguments\n");
        printf("Expected: file-to-str input output\n");
        return 1;
    }

    FILE *src = fopen(args[1], "r");
    if(!src){
        fprintf(stderr, "Could not open file %s\n", args[1]);
        return 1;
    }
    FILE *dst = fopen(args[2], "w");
    if(!dst){
        fprintf(stderr, "Could not open file %s\n", args[2]);
        return 1;
    }


    fprintf(dst, "uint32_t bytes[] = {");

    uint8_t buffer[5];
    char *c = fgets((void*)&buffer, 5, src);
    while(c){
        uint32_t *res = (uint32_t*)&buffer;
        fprintf(dst, "0x%X", *res);
        c = fgets((void*)&buffer, 5, src);
        if(c){
            fprintf(dst, ", ");
        }
    }
    fprintf(dst, "};");

    fclose(src);
    fclose(dst);

    return 0;
}
