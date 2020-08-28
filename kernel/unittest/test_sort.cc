#include "unittest.h"
#include "algorithm.h"
#include "rand.h"
#include "printk.h"
#include "inttypes.h"
#include "chrono.h"
#include "utility.h"

UNITTEST(test_sort)
{
    int cases[4] = {
        1, 2, 3, 4
    };

    do {
        int tcase[4];
        ext::copy(&cases[0], &cases[4], tcase);

        ext::sort(tcase, tcase + 4);

        eq(1, tcase[0]);
        eq(2, tcase[1]);
        eq(3, tcase[2]);
        eq(4, tcase[3]);
    } while (ext::next_permutation(cases, cases + countof(cases)));
}

UNITTEST(test_stress_sort)
{
    lfsr113_state_t r;

    lfsr113_seed(&r, 42);

    static int const shuffle_count = 5113;
    static int const item_count = 5113;
    static uint32_t const rand_cap =
            ((uint64_t(1) << 32) - (uint64_t(1) << 32) % shuffle_count) - 1;
    ext::unique_ptr<int[]> items(new (ext::nothrow) int[item_count]);

    for (int i = 0; i < item_count; ++i)
        items[i] = i;

    for (size_t iter = 0, e = 4; iter < e; ++iter) {
        // Shuffle
        for (int shuffle = 0; shuffle < shuffle_count; ++shuffle) {
            uint32_t value = lfsr113_rand(&r);
            if (value > rand_cap) {
                // Random number no good
                --shuffle;
                continue;
            }
            value %= item_count;
            ext::swap(items[shuffle], items[value]);
        }

        auto st = ext::chrono::high_resolution_clock::now();
        ext::sort(items.get(), items.get() + item_count);
        auto en = ext::chrono::high_resolution_clock::now();
        auto ns = ext::chrono::microseconds(en - st).count();

        printdbg("Sort %" PRIu64 " swaps"
                                 ", %" PRIu64" comparisons"
                                 ", %" PRIu64" us\n",
                 ext::detail::quicksort_cmp_count,
                 ext::detail::quicksort_swp_count,
                 ns);

        for (int i = 0; i < item_count; ++i)
            eq(i, items[i]);
    }
}
