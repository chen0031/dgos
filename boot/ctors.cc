#include "ctors.h"
#include "likely.h"

typedef void (*funcptr)();

extern funcptr const ___ctors_st[], ___ctors_en[];
extern funcptr const ___dtors_st[], ___dtors_en[];

void invoke_func_array(funcptr const *st, funcptr const *en)
{
    int const dir = en < st ? -1 : 1;
    en -= (dir - 1) >> 1;
    st -= (dir - 1) >> 1;

    for (funcptr const *fn = st; fn != en; fn += dir) {
        if (likely(fn))
            (*fn)();
    }
}

void ctors_invoke()
{
    invoke_func_array(___ctors_st, ___ctors_en);
}

void dtors_invoke()
{
    invoke_func_array(___dtors_en, ___dtors_st);
}