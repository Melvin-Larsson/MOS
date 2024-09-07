#define UNUSED(x) (void)(x)

void *printf(char *, ...);
void *calloc(long unsigned int, long unsigned int);
void *malloc(long unsigned int);
void *exit(int);
void free(void *);

void memory_init(){}
void *kcalloc(int size){
      return calloc(size, 1);
}
void *kcallocco(int size, int alignment, int boundary){
      UNUSED(size);
      UNUSED(alignment);
      UNUSED(boundary);
      printf("Memory-mock: kcallocco not implemented");
      exit(1);
      return 0;
}
void *kmalloc(int size){
      return malloc(size);
}
void *kmallocco(int size, int alignment, int boundary){
      UNUSED(size);
      UNUSED(alignment);
      UNUSED(boundary);
      printf("Memory-mock: kmallocco not implemented");
      exit(1);
      return 0;
}
void kfree(void *ptr){
      free(ptr);
}
