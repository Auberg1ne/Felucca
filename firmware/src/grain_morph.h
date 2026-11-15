/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Neighboring nonnegative Q15 values, round-nearest (ties up).
 * First segment denominator 64; second 63. Max numerator 32767*64+32.
 * Used only at window endpoints; no variable division or table generation. */
static inline int32_t gr_morph_mix(uint32_t morph, int32_t a, int32_t b)
{
    if (morph > 127u) return a;
    if (morph < 64u) return (a * (int32_t)(64u-morph) + b * (int32_t)morph + 32) / 64;
    uint32_t n = morph-64u;
    return (a * (int32_t)(63u-n) + b * (int32_t)n + 31) / 63;
}
