#ifndef _YUVBENCH_H_
#define _YUVBENCH_H_

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Ctx Ctx;
struct Ctx
{
    uint32_t inp_w;
    uint32_t inp_h;
    const void* inp_buf;
    size_t inp_len;
    void* out_buf;
    size_t out_len;

    void* user;
};

typedef struct Backend Backend;
struct Backend
{
    bool(*init_fn)(Ctx* ctx);
    bool(*convert_fn)(Ctx* ctx);
    void(*deinit_fn)(Ctx* ctx);
};

#endif // _YUVBENCH_H_
