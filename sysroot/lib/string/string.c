int strlen(const char* str){
    int length = 0;
    while(*str){
        length++;
        str++;
    }
    return length;
}
void strReadInt(int x, char * output){
    if(x == 0){
        output[0] = '0';
        output[1] = 0;
        return;
    }
    if(x < 0){
        output[0] = '-';
        output++;
        x = -x;
    }
    char buff[10];
    int i = 0;
    for(; x > 0; i++){
        buff[i] = '0' + x % 10; 
        x /= 10;
    }
    for(int j = 0; j < i; j++){
       output[i - j - 1] = buff[j];  
    }
    output[i] = 0;
}
