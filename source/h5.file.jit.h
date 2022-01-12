#ifndef H5_FILE_JIT_H
#define H5_FILE_JIT_H

#include "jit.common.h"

typedef struct h5file_jit
{
    t_object ob;
    void *data;
    int datatype;
    size_t datalen;
    void *p;
} h5file_jit;

#endif
