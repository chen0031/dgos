#pragma once

#if defined(__GNUC__)

#define __packed                __attribute__((packed))
#define __const                 __attribute__((const))
#define __pure                  __attribute__((pure))
#define __aligned(n)            __attribute__((aligned(n)))
#define __always_inline         __attribute__((always_inline)) inline
#define __noreturn              __attribute__((noreturn))
#define __used                  __attribute__((used))
#define __returns_twice         __attribute__((returns_twice))
#define __vector_size(n)        __attribute__((vector_size(n)))
#define __noinline              __attribute__((noinline))
#define __assume_aligned(n)     __attribute__((assume_aligned(n)))
#define __printf_format(m,n)    __attribute__((format(printf, m, n)))
#define __malloc                __attribute__((malloc))
#define __section(name)         __attribute__((section(name)))

#ifndef __SIG_ATOMIC_TYPE__
#define __SIG_ATOMIC_TYPE__ long int
#endif

typedef __SIZE_TYPE__ size_t;

typedef __PTRDIFF_TYPE__ ptrdiff_t;

//typedef __WCHAR_TYPE__ wchar_t;

typedef __WINT_TYPE__ wint_t;

typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

typedef __SIG_ATOMIC_TYPE__ sig_atomic_t;

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;

typedef int_fast64_t off_t;
typedef intptr_t ssize_t;
typedef uintptr_t uptrdiff_t;
#else
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef uint64_t size_t;
typedef int64_t ssize_t;

typedef uint64_t uintptr_t;
typedef int64_t intptr_t;

typedef int64_t off_t;

typedef int wint_t;

typedef int64_t ptrdiff_t;
typedef uint64_t uptrdiff_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

typedef int wchar_t;
#endif

/// SSE vectors
typedef int8_t __vector_size(16) __ivec16;
typedef int16_t __vector_size(16) __ivec8;
typedef int32_t __vector_size(16) __ivec4;
typedef int64_t __vector_size(16) __ivec2;
typedef uint8_t __vector_size(16) __uvec16;
typedef uint16_t __vector_size(16) __uvec8;
typedef uint32_t __vector_size(16) __uvec4;
typedef uint64_t __vector_size(16) __uvec2;
typedef float __vector_size(16) __fvec4;
typedef double __vector_size(16) __dvec2;

// Some builtins unnecessarily insist on long long types
typedef long long __vector_size(16) __ivec2LL;
typedef unsigned long long __vector_size(16) __ivec2ULL;

#define countof(arr) (sizeof(arr)/sizeof(*arr))
#define offsetof(t, m) __builtin_offsetof(t, m)

#define CHAR_BIT        8

#define MB_LEN_MAX      4

#define CHAR_MIN        -128
#define CHAR_MAX        127

#define SCHAR_MIN       -128
#define SCHAR_MAX       127

#define SHRT_MIN        -32768
#define SHRT_MAX        32767

#define INT_MIN         -2147483648
#define INT_MAX         2147483647

#define LONG_MIN        -9223372036854775808L
#define LONG_MAX        9223372036854775807L

#define LLONG_MIN       -9223372036854775808L
#define LLONG_MAX       9223372036854775807L

#define UCHAR_MAX       255U
#define USHRT_MAX       65535U
#define UINT_MAX        4294967295U
#define ULONG_MAX       18446744073709551615UL
#define ULLONG_MAX      18446744073709551615ULL

#define PTRDIFF_MAX     (-__PTRDIFF_MAX__-1)
#define PTRDIFF_MIN     __PTRDIFF_MAX__
#define SIZE_MAX        __SIZE_MAX__
#define SIG_ATOMIC_MIN  __SIG_ATOMIC_MIN__
#define SIG_ATOMIC_MAX  __SIG_ATOMIC_MAX__
#define WCHAR_MIN       __WCHAR_MIN__
#define WCHAR_MAX       __WCHAR_MAX__
#define WINT_MIN        __WINT_MIN__
#define WINT_MAX        __WINT_MAX__

#define INT_FAST8_MAX   __INT_FAST8_MAX__
#define INT_FAST8_MIN   (-__INT_FAST8_MAX__-1)
#define INT_FAST16_MAX  __INT_FAST16_MAX__
#define INT_FAST16_MIN  (-__INT_FAST16_MAX__-1)
#define INT_FAST32_MAX  __INT_FAST32_MAX__
#define INT_FAST32_MIN  (-__INT_FAST32_MAX__-1)
#define INT_FAST64_MAX  __INT_FAST64_MAX__
#define INT_FAST64_MIN  (-__INT_FAST64_MAX__-1)
#define INT_LEAST8_MAX  __INT_LEAST8_MAX__
#define INT_LEAST8_MIN  (-__INT_LEAST8_MAX__-1)
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#define INT_LEAST16_MIN (-__INT_LEAST16_MAX__-1)
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#define INT_LEAST32_MIN (-__INT_LEAST32_MAX__-1)
#define INT_LEAST64_MAX __INT_LEAST64_MAX__
#define INT_LEAST64_MIN (-__INT_LEAST64_MAX__-1)
#define INT8_MAX        __INT8_MAX__
#define INT8_MIN        (-__INT8_MAX__-1)
#define INT16_MAX       __INT16_MAX__
#define INT16_MIN       (-__INT16_MAX__-1)
#define INT32_MAX       __INT32_MAX__
#define INT32_MIN       (-__INT32_MAX__-1)
#define INT64_MAX       __INT64_MAX__
#define INT64_MIN       (-__INT64_MAX__-1)
#define INTMAX_MAX      __INTMAX_MAX__
#define INTMAX_MIN      (-__INTMAX_MAX__-1)
#define INTPTR_MAX      __INTPTR_MAX__
#define INTPTR_MIN      (-__INTPTR_MAX__-1)

#define UINT_FAST8_MAX   __UINT_FAST8_MAX__
#define UINT_FAST8_MIN   0
#define UINT_FAST16_MAX  __UINT_FAST16_MAX__
#define UINT_FAST16_MIN  0
#define UINT_FAST32_MAX  __UINT_FAST32_MAX__
#define UINT_FAST32_MIN  0
#define UINT_FAST64_MAX  __UINT_FAST64_MAX__
#define UINT_FAST64_MIN  0
#define UINT_LEAST8_MAX  __UINT_LEAST8_MAX__
#define UINT_LEAST8_MIN  0
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#define UINT_LEAST16_MIN 0
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#define UINT_LEAST32_MIN 0
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__
#define UINT_LEAST64_MIN 0
#define UINT8_MAX        __UINT8_MAX__
#define UINT8_MIN        0
#define UINT16_MAX       __UINT16_MAX__
#define UINT16_MIN       0
#define UINT32_MAX       __UINT32_MAX__
#define UINT32_MIN       0
#define UINT64_MAX       __UINT64_MAX__
#define UINT64_MIN       0
#define UINTMAX_MAX      __UINTMAX_MAX__
#define UINTMAX_MIN      0
#define UINTPTR_MAX      __UINTPTR_MAX__
#define UINTPTR_MIN      0
