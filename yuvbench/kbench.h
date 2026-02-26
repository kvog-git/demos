#ifndef _KBENCH_H_
#define _KBENCH_H_

#include <stdint.h>

uintptr_t KBenchTS(void);
uintptr_t KBenchElapsedCounts(uintptr_t t0, uintptr_t t1);
double KBenchElapsedTime(uintptr_t t0, uintptr_t t1);

#ifdef KBENCH_IMPLEMENTATION

uintptr_t KBenchTS(void)
{
    uintptr_t result = 0;
    // macos+aarch64+clang: use CNTVCT_EL0
#if defined(__APPLE__) && defined(__clang__) && defined(__aarch64__)
    __asm__ volatile("mrs %0, CNTVCT_EL0" : "=r"(result) :: "memory");
#endif
    return result;
}

uintptr_t KBenchElapsedCounts(uintptr_t t0, uintptr_t t1)
{
    // @@
    return t1 - t0;
}

double KBenchElapsedTime(uintptr_t t0, uintptr_t t1)
{
    uintptr_t elapsed = KBenchElapsedCounts(t0, t1);
    double val = 0.0f;
    // macos+aarch64+clang: divide by CNTFRQ_EL0
#if defined(__APPLE__) && defined(__clang__) && defined(__aarch64__)
    uintptr_t cntfreq_el0 = 0;
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(cntfreq_el0) :: "memory");
    val = (double)elapsed / (double)cntfreq_el0;
#endif
    return val;
}

#endif // KBENCH_IMPLEMENTATION
#endif // _KBENCH_H_
