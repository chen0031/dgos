#include "cxxstring.h"

// Explicit instantiations
//template class std::char_traits<char>;
//template class std::char_traits<wchar_t>;
//template class std::char_traits<char16_t>;
//template class std::char_traits<char32_t>;
//template class std::basic_string<char>;
//template class std::basic_string<wchar_t>;
//template class std::basic_string<char16_t>;
//template class std::basic_string<char32_t>;

ext::string ext::to_string(int value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%d", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string ext::to_string(long value)
{
    ext::string result(22, 0);
    int len = snprintf(result.data(), result.size(), "%ld", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string ext::to_string(long long value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%lld", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string ext::to_string(unsigned value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%u", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string ext::to_string(unsigned long value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%lu", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string ext::to_string(unsigned long long value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%llu", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

#ifndef __DGOS_KERNEL__
ext::string ext::to_string(float value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%f", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string std::to_string(double value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%lf", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}

ext::string std::to_string(long double value)
{
    ext::string result(11, 0);
    int len = snprintf(result.data(), result.size(), "%llf", value);
    if (unlikely(!result.resize(len >= 0 ? len : 0)))
        result.clear();
    return result;
}
#endif

void ext::detail::throw_bad_alloc()
{
    throw ext::bad_alloc();
}

ext::string ext::to_hex(unsigned long long value, bool prefix)
{
    char digits[20];
    char *out = digits + sizeof(digits);

    do {
        int nibble = value & 0xf;
        *--out = nibble < 10 ? nibble + '0' : nibble - 10 + 'a';
    } while (value >>= 4);

    if (prefix) {
        *--out = 'x';
        *--out = '0';
    }

    return ext::string(out, digits + sizeof(digits));
}
