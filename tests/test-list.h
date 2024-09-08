#ifndef TEST_LIST_H_INCLUDED
#define TEST_LIST_H_INCLUDED

typedef struct{
    char *group;
    char *name;
    void (*setup)(void);
    void (*teardown)(void);
    void (*test)(void);
}Test;

typedef struct{
    Test *tests;
    unsigned int testCount;
}TestArray;

extern TestArray tests;

#endif
