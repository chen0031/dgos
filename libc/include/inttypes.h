#pragma once

#if !defined(__SIZEOF_LONG__) || !defined(__SIZEOF_LONG_LONG__)
#error Required information missing
#endif

#if __SIZEOF_LONG__ != 4 && __SIZEOF_LONG__ != 8
#error long is not 32 bit and not 64 bit!
#endif

#if __SIZEOF_LONG_LONG__ != 8
#error long long is not 64 bit!
#endif

#if __SIZEOF_LONG__ == 4 && __SIZEOF_LONG_LONG__ == 8
// 32 bit code
#define __FMT8      "hh"
#define __FMT16     "h"
#ifdef __clang__
#define __FMT32     ""
#else
#define __FMT32     "l"
#endif
#define __FMT64     "ll"
#define __FMTMX     "j"
#define __FMTPT     "t"
#elif __SIZEOF_LONG__ == 8 && __SIZEOF_LONG_LONG__ == 8
// 64 bit code
#define __FMT8      "hh"
#define __FMT16     "h"
#define __FMT32     ""
#define __FMT64     "l"
#define __FMTMX     "j"
#define __FMTPT     "t"
#else
#error Unknown architecture
#endif

#define PRId8           __FMT8  "d"
#define PRId16          __FMT16 "d"
#define PRId32          __FMT32 "d"
#define PRId64          __FMT64 "d"
#define PRIdLEAST8      __FMT8  "d"
#define PRIdLEAST16     __FMT16 "d"
#define PRIdLEAST32     __FMT32 "d"
#define PRIdLEAST64     __FMT64 "d"
#define PRIdFAST8       __FMT8  "d"
#define PRIdFAST16      __FMT16 "d"
#define PRIdFAST32      __FMT32 "d"
#define PRIdFAST64      __FMT64 "d"
#define PRIdMAX         __FMTMX "d"
#define PRIdPTR         __FMTPT "d"

#define PRIi8           __FMT8  "i"
#define PRIi16          __FMT16 "i"
#define PRIi32          __FMT32 "i"
#define PRIi64          __FMT64 "i"
#define PRIiLEAST8      __FMT8  "i"
#define PRIiLEAST16     __FMT16 "i"
#define PRIiLEAST32     __FMT32 "i"
#define PRIiLEAST64     __FMT64 "i"
#define PRIiFAST8       __FMT8  "i"
#define PRIiFAST16      __FMT16 "i"
#define PRIiFAST32      __FMT32 "i"
#define PRIiFAST64      __FMT64 "i"
#define PRIiMAX         __FMTMX "i"
#define PRIiPTR         __FMTPT "i"

#define PRIu8           __FMT8  "u"
#define PRIu16          __FMT16 "u"
#define PRIu32          __FMT32 "u"
#define PRIu64          __FMT64 "u"
#define PRIuLEAST8      __FMT8  "u"
#define PRIuLEAST16     __FMT16 "u"
#define PRIuLEAST32     __FMT32 "u"
#define PRIuLEAST64     __FMT64 "u"
#define PRIuFAST8       __FMT8  "u"
#define PRIuFAST16      __FMT16 "u"
#define PRIuFAST32      __FMT32 "u"
#define PRIuFAST64      __FMT64 "u"
#define PRIuMAX         __FMTMX "u"
#define PRIuPTR         __FMTPT "u"

#define PRIo8           __FMT8  "o"
#define PRIo16          __FMT16 "o"
#define PRIo32          __FMT32 "o"
#define PRIo64          __FMT64 "o"
#define PRIoLEAST8      __FMT8  "o"
#define PRIoLEAST16     __FMT16 "o"
#define PRIoLEAST32     __FMT32 "o"
#define PRIoLEAST64     __FMT64 "o"
#define PRIoFAST8       __FMT8  "o"
#define PRIoFAST16      __FMT16 "o"
#define PRIoFAST32      __FMT32 "o"
#define PRIoFAST64      __FMT64 "o"
#define PRIoMAX         __FMTMX "o"
#define PRIoPTR         __FMTPT "o"

#define PRIx8           __FMT8  "x"
#define PRIx16          __FMT16 "x"
#define PRIx32          __FMT32 "x"
#define PRIx64          __FMT64 "x"
#define PRIxLEAST8      __FMT8  "x"
#define PRIxLEAST16     __FMT16 "x"
#define PRIxLEAST32     __FMT32 "x"
#define PRIxLEAST64     __FMT64 "x"
#define PRIxFAST8       __FMT8  "x"
#define PRIxFAST16      __FMT16 "x"
#define PRIxFAST32      __FMT32 "x"
#define PRIxFAST64      __FMT64 "x"
#define PRIxMAX         __FMTMX "x"
#define PRIxPTR         __FMTPT "x"

#define PRIX8           __FMT8  "X"
#define PRIX16          __FMT16 "X"
#define PRIX32          __FMT32 "X"
#define PRIX64          __FMT64 "X"
#define PRIXLEAST8      __FMT8  "X"
#define PRIXLEAST16     __FMT16 "X"
#define PRIXLEAST32     __FMT32 "X"
#define PRIXLEAST64     __FMT64 "X"
#define PRIXFAST8       __FMT8  "X"
#define PRIXFAST16      __FMT16 "X"
#define PRIXFAST32      __FMT32 "X"
#define PRIXFAST64      __FMT64 "X"
#define PRIXMAX         __FMTMX "X"
#define PRIXPTR         __FMTPT "X"
