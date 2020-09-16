#pragma once
#include "../likely.h"
#ifdef __DGOS__
#include "types.h"
#else
#include <stdint.h>
#include <stddef.h>
#endif
#include "type_traits.h"
#include "memory.h"
#include "cxxiterator.h"
#include "functional.h"

__BEGIN_NAMESPACE_EXT

template<typename _InputIt, typename _T>
constexpr _InputIt find(_InputIt __first, _InputIt __last, _T const& __v)
{
    while (__first != __last && !(*__first == __v))
        ++__first;
    return __first;
}

template<typename _InputIt, typename _UnaryPredicate>
constexpr _InputIt find_if(_InputIt __first, _InputIt __last,
                           _UnaryPredicate&& __p)
{
    while (__first != __last && !__p(*__first))
        ++__first;
    return __first;
}

template<typename _InputIt, typename _UnaryPredicate>
constexpr _InputIt find_if_not(_InputIt __first, _InputIt __last,
                               _UnaryPredicate __p)
{
    while (__first != __last && __p(*__first))
        ++__first;
    return __first;
}

template<typename _InputIt, typename _UnaryPredicate>
constexpr bool all_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (!__p(*__first))
            return false;
        ++__first;
    }
    return true;
}

template<typename _InputIt, typename _UnaryPredicate>
constexpr bool any_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (__p(*__first))
            return true;
        ++__first;
    }
    return false;
}

template<typename _InputIt, typename _UnaryPredicate>
constexpr bool none_of(_InputIt __first, _InputIt __last, _UnaryPredicate __p)
{
    while (__first != __last) {
        if (__p(*__first))
            return false;
        ++__first;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2>
constexpr bool equal(_InputIt1 __first1, _InputIt1 __last1, _InputIt2 __first2)
{
    while (__first1 != __last1) {
        if (*__first1 != *__first2)
            return false;
        ++__first1;
        ++__first2;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2, typename _BinaryPredicate>
constexpr bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _BinaryPredicate __p)
{
    while (__first1 != __last1) {
        if (!__p(*__first1, *__first2))
            return false;
        ++__first1;
        ++__first2;
    }
    return true;
}

template<typename _InputIt1, typename _InputIt2>
constexpr bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _InputIt2 __last2 )
{
    // FIXME: if both are random iterators, should compare distances first
    for(;;) {
        if (unlikely(__first1 == __last1 && __first2 == __last2))
            return true;
        if (unlikely(__first1 == __last1))
            return false;
        if (unlikely(__first2 == __last2))
            return false;
        if (unlikely(*__first1 != *__first2))
            return false;
        ++__first1;
        ++__first2;
    }
}

template<typename _InputIt1, typename _InputIt2, typename _BinaryPredicate>
constexpr bool equal(_InputIt1 __first1, _InputIt1 __last1,
           _InputIt2 __first2, _InputIt2 __last2,
           _BinaryPredicate __p )
{
    // FIXME: if both are random iterators, should compare distances first
    for(;;) {
        if (unlikely(__first1 == __last1 && __first2 == __last2))
            return true;
        if (unlikely(__first1 == __last1))
            return false;
        if (unlikely(__first2 == __last2))
            return false;
        if (unlikely(!__p(*__first1, *__first2)))
            return false;
        ++__first1;
        ++__first2;
    }
}

template<typename _OutputIt, typename _Size, typename _T>
constexpr _OutputIt fill_n(_OutputIt __first, _Size __count, _T const& __value)
{
    static_assert(ext::is_integral<_Size>::value, "Must pass integral count");

    while (__count >= 16) {
        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        *__first = __value;
        ++__first;

        __count -= 16;
    }

    while (__count > 0) {
        *__first = __value;
        ++__first;
        --__count;
    }

    return __first;
}

template<typename _InputIt, typename _OutputIt>
constexpr _OutputIt uninitialized_copy(_InputIt __first, _InputIt __last,
                             _OutputIt __out)
{
    using T = typename ext::remove_reference<decltype(*__out)>::type;
    while (__first != __last) {
        new (&*__out) T(*__first);
        ++__out;
        ++__first;
    }
    return __out;
}

template<typename _OutputIt, typename _T>
constexpr _OutputIt uninitialized_fill(_OutputIt __first, _OutputIt __last,
                             _T const& __value)
{
    using T = decltype(*__first);
    while (__first != __last) {
        new (&*__first) typename ext::remove_reference<T>::type(__value);
        ++__first;
    }
    return __first;
}

template<typename _T, typename _OutputIt, typename... _Args>
constexpr _OutputIt uninitialized_emplace(
        _OutputIt __first, _OutputIt __last, _Args&& ...__args)
{
    using T = decltype(*__first);
    while (__first != __last) {
        new (&*__first) typename ext::remove_reference<T>::type(
                    ext::forward<_Args>(__args)...);
        ++__first;
    }
    return __first;
}

template<typename _InputIt, typename _OutputIt>
constexpr _OutputIt uninitialized_move(_InputIt __first, _InputIt __last,
                             _OutputIt __out)
{
    using T = typename remove_reference<decltype(*__out)>::type;
    while (__first != __last) {
        new (&*__out) T(move(*__first));
        ++__out;
        ++__first;
    }
    return __out;
}

template<typename _InputIt, typename _OutputIt>
constexpr _OutputIt copy(_InputIt __first, _InputIt __last, _OutputIt __out)
{
    for ( ; __first != __last; ++__first, ++__out)
        *__out = *__first;
    return __out;
}

template<typename _ForwardIt>
constexpr _ForwardIt min_element(_ForwardIt __first, _ForwardIt __last)
{
    if (__first != __last) {
        _ForwardIt __smallest = __first;

        for (++__first; __first != __last; ++__first) {
            if (*__first < *__smallest)
                __smallest = __first;
        }

        return __smallest;
    }

    return __last;
}

template<typename _ForwardIt>
constexpr _ForwardIt max_element(_ForwardIt __first, _ForwardIt __last)
{
    if (__first != __last) {
        _ForwardIt __largest = __first;

        for (++__first; __first != __last; ++__first) {
            if (*__largest < *__first)
                __largest = __first;
        }

        return __largest;
    }

    return __last;
}

template<typename _ForwardIt, typename _Compare>
constexpr _ForwardIt min_element(_ForwardIt __first, _ForwardIt __last,
                                 _Compare __cmp)
{
    if (__first != __last) {
        _ForwardIt __smallest = __first;

        for (++__first; __first != __last; ++__first) {
            if (__cmp(*__first, *__smallest) < 0)
                __smallest = __first;
        }

        return __smallest;
    }

    return __last;
}

template<typename _ForwardIt, typename _Compare>
constexpr _ForwardIt max_element(_ForwardIt __first, _ForwardIt __last,
                                 _Compare __cmp)
{
    if (__first != __last) {
        _ForwardIt __largest = __first;

        for (++__first; __first != __last; ++__first) {
            if (__cmp(*__largest, *__first) < 0)
                __largest = __first;
        }

        return __largest;
    }

    return __last;
}

template<typename _RandomIt, typename _Val>
constexpr _RandomIt lower_bound(_RandomIt __first, _RandomIt __last,
                                _Val const& __val)
{
    size_t __st = 0;
    size_t __en = __last - __first;
    size_t __md = 0;
    while (__st < __en) {
        __md = ((__en - __st) >> 1) + __st;
        bool __is_less = __first[__md] < __val;
        __st = __is_less ? __md + 1 : __st;
        __en = __is_less ? __en : __md;
    }
    return __first + __st;
}

template<typename _RandomIt, typename _Val, typename _Compare>
constexpr _RandomIt lower_bound(_RandomIt __first, _RandomIt __last,
                                _Val const& __val, _Compare __cmp)
{
    size_t __st = 0;
    size_t __en = __last - __first;
    while (__st < __en) {
        size_t __md = ((__en - __st) >> 1) + __st;
        bool __is_less = __cmp(__first[__md], __val);
        __st = __is_less ? __md + 1 : __st;
        __en = __is_less ? __en : __md;
    }
    return __first + __st;
}

__BEGIN_NAMESPACE_DETAIL


///algorithm quicksort(A, lo, hi) is
///if lo < hi then
///    p := partition(A, lo, hi)
///    quicksort(A, lo, p)
///    quicksort(A, p + 1, hi)
//
///algorithm partition(A, lo, hi) is
///pivot := A[(hi + lo) / 2]
///loop forever
///    while A[lo] < pivot
///        lo := lo + 1
//
///    while A[hi] > pivot
///        hi := hi - 1
//
///    if lo >= hi then
///        return hi
//
///    swap A[lo] with A[hi]
//
///    lo := lo + 1
///    hi := hi - 1
///

KERNEL_API extern uintptr_t quicksort_cmp_count;
KERNEL_API extern uintptr_t quicksort_swp_count;

// Partition an array given an inclusive index range, return partition index
template<typename _T, typename _Compare>
constexpr size_t hoare_partition(_T* __a, size_t __lo, size_t __hi,
                                 _Compare&& __is_less)
{
    _T *__pivot = __a + (__lo + ((__hi - __lo) >> 1));

    for (;;) {
        while (__is_less(__a[__lo], *__pivot)) {
            ++quicksort_cmp_count;
            ++__lo;
        }

        while (__is_less(*__pivot, __a[__hi])) {
            ++quicksort_cmp_count;
            --__hi;
        }

        if (__lo >= __hi)
            return __hi;

        ++quicksort_swp_count;
        ext::swap(__a[__lo], __a[__hi]);

        // Pivot follows the swap if it was involved
        __pivot = __pivot == &__a[__hi]
                ? &__a[__lo]
                : __pivot == &__a[__lo]
                ? &__a[__hi]
                : __pivot;

        ++__lo;
        --__hi;
    }
}

template<typename _T, typename _Compare>
constexpr void quicksort(_T* a, size_t lo, size_t hi, _Compare&& __is_less)
{
    if (lo < hi) {
        size_t p = hoare_partition(
                    a, lo, hi, ext::forward<_Compare>(__is_less));
        quicksort(a, lo, p, ext::forward<_Compare>(__is_less));
        quicksort(a, p + 1, hi, ext::forward<_Compare>(__is_less));
    }
}

__END_NAMESPACE

template<typename _RandomIt,
typename _V = typename ext::iterator_traits<_RandomIt>::value_type>
constexpr void sort(_RandomIt __first, _RandomIt __last)
{
    detail::quicksort_cmp_count = 0;
    detail::quicksort_swp_count = 0;
    size_t __sz = __last - __first;
    if (__sz) {
        detail::quicksort(&*__first, 0, __sz - 1, less<_V>());
    }
}

template<typename _RandomIt, typename _Compare >
constexpr void sort(_RandomIt __first, _RandomIt __last, _Compare __is_less)
{
    size_t __sz = __last - __first;
    if (__sz)
        detail::quicksort(&*__first, 0, __sz - 1, __is_less);
}

template<typename _ForwardIt1, typename _ForwardIt2>
constexpr void iter_swap(_ForwardIt1 a, _ForwardIt2 b)
{
   swap(*a, *b);
}

template<typename _BidirIt>
void reverse(_BidirIt __first, _BidirIt __last)
{
    while ((__first != __last) && (__first != --__last))
        iter_swap(__first++, __last);
}

template<typename _BidirIt>
bool next_permutation(_BidirIt __first, _BidirIt __last)
{
    if (__first == __last) return false;
    _BidirIt __i = __last;
    if (__first == --__i) return false;

    while (true) {
        _BidirIt __i1, __i2;

        __i1 = __i;
        if (*--__i < *__i1) {
            __i2 = __last;
            while (!(*__i < *--__i2))
                ;
            iter_swap(__i, __i2);
            reverse(__i1, __last);
            return true;
        }
        if (__i == __first) {
            reverse(__first, __last);
            return false;
        }
    }
}

template<typename _BidirIt>
bool prev_permutation(_BidirIt __first, _BidirIt __last)
{
    if (__first == __last) return false;
    _BidirIt __i = __last;
    if (__first == --__i) return false;

    while (1) {
        _BidirIt __i1, __i2;

        __i1 = __i;
        if (*__i1 < *--__i) {
            __i2 = __last;
            while (!(*--__i2 < *__i))
                ;
            iter_swap(__i, __i2);
            reverse(__i1, __last);
            return true;
        }
        if (__i == __first) {
            reverse(__first, __last);
            return false;
        }
    }
}

__END_NAMESPACE_EXT
