#pragma once

#ifdef __DGOS_KERNEL__
#include "types.h"
#else
#include <stdint.h>
#endif

#if 0
#include <initializer_list>
#else
__BEGIN_NAMESPACE_STD

template<typename _T>
class initializer_list
{
public:
    typedef _T value_type;
    typedef size_t size_type;
    typedef _T const& reference;
    typedef _T const& const_reference;
    typedef _T const* iterator;
    typedef _T const* const_iterator;

private:
    iterator __array;
    size_type __len;

    // The compiler can call a private constructor.
    constexpr initializer_list(const_iterator __a, size_type __l)
        : __array(__a)
        , __len(__l)
    {
    }

public:
    constexpr initializer_list() noexcept
        : __array(nullptr)
        , __len(0) { }

    constexpr size_type
    size() const noexcept
    {
        return __len;
    }

    constexpr const_iterator
    begin() const noexcept
    {
        return __array;
    }

    constexpr const_iterator
    end() const noexcept
    {
        return __array + __len;
    }
};

template<typename _T>
constexpr _T const *
begin(initializer_list<_T> __ils) noexcept
{
    return __ils.begin();
}

template<typename _T>
constexpr _T const *
end(initializer_list<_T> __ils) noexcept
{
    return __ils.end();
}

__END_NAMESPACE_STD
#endif
