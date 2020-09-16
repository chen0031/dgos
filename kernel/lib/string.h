#pragma once

#include "types.h"
#include "cpu/nontemporal.h"
#include "type_traits.h"
#include "utf.h"

__BEGIN_DECLS

size_t strlen(char const *src);
void *memchr(void const *mem, int ch, size_t count);
void *memrchr(void const *mem, int ch, size_t count);
char *strchr(char const *s, int ch);
char *strrchr(char const *s, int ch);

int strcmp(char const *lhs, char const *rhs);
int strncmp(char const *lhs, char const *rhs, size_t count);
int memcmp(void const *lhs, void const *rhs, size_t count);
char *strstr(char const *str, char const *substr);

void clear64(void *dest, size_t n);

void *memset(void *dest, int c, size_t n);
void *memcpy(void * restrict dest,
                        void const * restrict src, size_t n);

_no_instrument
void *memcpy_noinstrument(void * restrict dest,
                          void const * restrict src, size_t n);
void *memmove(void *dest, void const *src, size_t n);

char *strcpy(char * restrict dest, char const * restrict src);
char *strcat(char * restrict dest, char const * restrict src);

char *stpcpy(char *dest, char const *src);

size_t strspn(char const *src, char const *chars);
size_t strcspn(char const *src, char const *chars);

char *strncpy(char * restrict dest, char const * restrict src, size_t n);
char *strncat(char * restrict dest, char const * restrict src, size_t n);

void *aligned16_memset(void *dest, int c, size_t n);

// Aligned fill of naturally aligned values
void memfill_16(void *dest, uint16_t v, size_t count);
void memfill_32(void *dest, uint32_t v, size_t count);
void memfill_64(void *dest, uint64_t v, size_t count);

__END_DECLS

template<typename T>
void memzero(T &obj)
{
    memset(&obj, 0, sizeof(obj));
}

//
// Helpers to access device memory safely (KVM explodes if you use vector insn)

// Zero extending loads

template<typename T>
static _always_inline unsigned devload_impl(
        T const *p, ext::integral_constant<size_t, 1>::type, ext::false_type)
{
    unsigned result;
    __asm__ __volatile__ (
        "movzbl (%[src]),%k[result]\n\t"
        : [result] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline unsigned devload_impl(
        T const *p, ext::integral_constant<size_t, 2>::type, ext::false_type)
{
    unsigned result;
    __asm__ __volatile__ (
        "movzwl (%[src]),%k[result]\n\t"
        : [result] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline unsigned devload_impl(
        T const *p, ext::integral_constant<size_t, 4>::type, ext::false_type)
{
    unsigned result;
    __asm__ __volatile__ (
        "movl (%[src]),%k[result]\n\t"
        : [result] "=r" (result)
        : [src] "m" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline uint64_t devload_impl(
        T const *p, ext::integral_constant<size_t, 8>::type, ext::false_type)
{
    uint64_t result;
    __asm__ __volatile__ (
        "movq (%[src]),%q[result]\n\t"
        : [result] "=r" (result)
        : [src] "m" (p)
        : "memory"
    );
    return result;
}

// Sign extending loads

template<typename T>
static _always_inline int devload_impl(
        T const *p, ext::integral_constant<size_t, 1>::type, ext::true_type)
{
    int result;
    __asm__ __volatile__ (
        "movsbl (%[src]),%k[result]\n\t"
        : [result] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline int devload_impl(
        T const *p, ext::integral_constant<size_t, 2>::type, ext::true_type)
{
    int result;
    __asm__ __volatile__ (
        "movswl (%[src]),%k[result]\n\t"
        : [result] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline int devload_impl(
        T const *p, ext::integral_constant<size_t, 4>::type, ext::true_type)
{
    int result;
    __asm__ __volatile__ (
        "movl (%[src]),%k[dest]\n\t"
        : [dest] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

template<typename T>
static _always_inline int64_t devload_impl(
        T const *p, ext::integral_constant<size_t, 8>::type, ext::true_type)
{
    int64_t result;
    __asm__ __volatile__ (
        "movq (%[src]),%q[dest]\n\t"
        : [dest] "=r" (result)
        : [src] "r" (p)
        : "memory"
    );
    return result;
}

// Stores

template<typename T>
static _always_inline void devstore_impl(
        T *p, T const& val, ext::integral_constant<size_t, 1>::type)
{
    __asm__ __volatile__ (
        "movb %b[src],(%[dest])\n\t"
        :
        : [dest] "r" (p)
        , [src] "r" (val)
        : "memory"
    );
}

template<typename T>
static _always_inline void devstore_impl(
        T *p, T const& val, ext::integral_constant<size_t, 2>::type)
{
    __asm__ __volatile__ (
        "movw %w[src],(%[dest])\n\t"
        :
        : [dest] "r" (p)
        , [src] "r" (val)
        : "memory"
    );
}

template<typename T>
static _always_inline void devstore_impl(
        T *p, T const& val, ext::integral_constant<size_t, 4>::type)
{
    __asm__ __volatile__ (
        "movl %k[src],%[dest]\n\t"
        :
        : [dest] "r" (p)
        , [src] "r" (val)
        : "memory"
    );
}

template<typename T>
static _always_inline void devstore_impl(
        T *p, T const& val, ext::integral_constant<size_t, 8>::type)
{
    __asm__ __volatile__ (
        "movq %q[src],%[dest]\n\t"
        :
        : [dest] "r" (p)
        , [src] "r" (val)
        : "memory"
    );
}

// Dispatcher

template<typename T>
static _always_inline auto devload(T const *p) ->
    decltype(devload_impl(
                 p, typename ext::integral_constant<size_t, sizeof(T)>::type(),
                 typename ext::is_signed<T>::type()))
{
    return devload_impl(
                p, typename ext::integral_constant<size_t, sizeof(T)>::type(),
                typename ext::is_signed<T>::type());
}

template<typename T>
static _always_inline auto devstore(T*p, T const& val) ->
    decltype(devstore_impl(
                 p, val,
                 typename ext::integral_constant<size_t, sizeof(T)>::type()))
{
    return devstore_impl(
                p, val,
                typename ext::integral_constant<size_t, sizeof(T)>::type());
}

//


#ifdef _ASAN_ENABLED
extern "C" void __asan_storeN_noabort(uintptr_t addr, size_t size);
#endif

__BEGIN_DECLS

static _always_inline void memset8(char *&d, uint64_t s, size_t n)
{
#ifdef _ASAN_ENABLED
    __asan_storeN_noabort(uintptr_t(d), n * sizeof(uint8_t));
#endif

    __asm__ __volatile__ (
        "rep stosb"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static _always_inline void memset16(char *&d, uint64_t s, size_t n)
{
#ifdef _ASAN_ENABLED
    __asan_storeN_noabort(uintptr_t(d), n * sizeof(uint16_t));
#endif

    __asm__ __volatile__ (
        "rep stosw"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static _always_inline void memset32(char *&d, uint64_t s, size_t n)
{
#ifdef _ASAN_ENABLED
    __asan_storeN_noabort(uintptr_t(d), n * sizeof(uint32_t));
#endif

    __asm__ __volatile__ (
        "rep stosl"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

static _always_inline void memset64(char *&d, uint64_t s, size_t n)
{
#ifdef _ASAN_ENABLED
    __asan_storeN_noabort(uintptr_t(d), n * sizeof(uint64_t));
#endif

    __asm__ __volatile__ (
        "rep stosq"
        : "+D" (d)
        , "+c" (n)
        : "a" (s)
        : "memory"
    );
}

__END_DECLS
