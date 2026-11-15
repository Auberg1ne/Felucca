/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <math.h>
#include <stdint.h>
/* Test-only final-output evidence: compare bytes only after validating a sounding capture. */
typedef struct {
    uint64_t samples, nonzero;
    double energy, peak;
} audio_stats_t;
static void audio_stats_push(audio_stats_t *s, double x)
{
    s->samples++;
    s->nonzero += x != 0;
    s->energy += x * x;
    if (fabs(x) > s->peak) s->peak = fabs(x);
}
static double audio_stats_rms(const audio_stats_t *s)
{
    return s->samples ? sqrt(s->energy / s->samples) : 0;
}
static int audio_stats_sounding(const audio_stats_t *s, double peak, double rms)
{
    return s->samples && isfinite(s->energy) && isfinite(s->peak) &&
        s->peak > peak && audio_stats_rms(s) > rms && s->nonzero > s->samples / 100;
}
