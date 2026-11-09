/* SPDX-License-Identifier: GPL-3.0-only */
/* Deployment must contain the committed bank, not silently pass on sine fallback. */
#define SAMPLE_XIP_TEST_NO_MAIN 1
#include "../../web/emu/sample_xip_test.c"
int main(void)
{
    if (SMP_NSETS != 5 || SMP_NALL != 13) return 1;
    for (unsigned src = 0; src < 5; src++) {
        const smp_set_t *set = &SMP_SETS[src];
        if (!set->nz || set->z0 >= NELEM(SMP_ZONES) || set->nz > NELEM(SMP_ZONES)-set->z0) return 2;
        if (!SMP_ZONES[set->z0].n || smp_set_missing(src)) return 3;
    }
    if (SMP_SETS[1].z0 != SMP_SETS[0].z0 || SMP_SETS[4].z0 != SMP_SETS[0].z0) return 4;
    puts("Factory deployment gate: five populated IDs, three historical materials, eight user IDs preserved");
    return 0;
}
