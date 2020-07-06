#pragma once
#include <stdint.h>

/// emit_chars callback takes null pointer and a character,
/// or, a pointer to null terminated string and a 0
/// or, a pointer to an unterminated string and a length
long formatter(
        char const *format, va_list ap,
        int (*emit_chars)(char const *, intptr_t, void*),
        void *emit_context);

int __emit_stream_chars(char const *p, intptr_t n, void* ctx);
