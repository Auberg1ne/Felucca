/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef HOME_LEVELS_H
#define HOME_LEVELS_H
#include <stdint.h>
#ifndef FELUCCA_HOME_LEVELS
#define FELUCCA_HOME_LEVELS 1
#endif
#define HOME_LEVEL_CHANNELS 4u
#define HOME_LEVEL_FRAME_MS 50u /* future sprites: at most 20 fps */
/* CH1..CH4 are always indices 0..3. This state is not persisted. */
#if FELUCCA_HOME_LEVELS
static uint32_t home_level_raw[HOME_LEVEL_CHANNELS]; /* one part writer; read after C1_WAIT */
static uint32_t home_level_env[HOME_LEVEL_CHANNELS]; /* core-0 audio writer only */
static volatile uint32_t home_level_pub[HOME_LEVEL_CHANNELS]; /* independent aligned word mailboxes */
#endif
static uint32_t home_level_peak(uint32_t peak, int32_t l, int32_t r)
{
#if FELUCCA_HOME_LEVELS
    uint32_t a=l<0?0u-(uint32_t)l:(uint32_t)l, b=r<0?0u-(uint32_t)r:(uint32_t)r;
    uint32_t v=a>b?a:b; if(v>32767u)v=32767u;
    return v>peak?v:peak;
#else
    (void)l;(void)r;return peak;
#endif
}
static void home_level_begin(uint32_t part)
{
#if FELUCCA_HOME_LEVELS
    home_level_raw[part]=0; /* includes silent/early-returning parts */
#else
    (void)part;
#endif
}
static void home_level_end(uint32_t part, uint32_t peak)
{
#if FELUCCA_HOME_LEVELS
    home_level_raw[part]=peak;
#else
    (void)part;(void)peak;
#endif
}
/* Call once after all parts have joined, on core 0; ~10 ms attack / 200 ms release.
 * Only four bounded integer updates. No buffers, allocation, locks or retry loop. */
static void home_level_publish(uint32_t n)
{
#if FELUCCA_HOME_LEVELS
    for(uint32_t p=0;p<HOME_LEVEL_CHANNELS;p++) {
        uint32_t target=home_level_raw[p], old=home_level_env[p], delta;
        if(target>old) {delta=((target-old)*n+440u)/441u;if(delta>target-old)delta=target-old;old+=delta;}
        else if(target<old) {delta=((old-target)*n+8819u)/8820u;if(delta>old-target)delta=old-target;old-=delta;}
        home_level_env[p]=old;
        home_level_pub[p]=old; /* payload is one aligned uint32, no shared read/clear */
    }
#else
    (void)n;
#endif
}
static uint16_t home_level_read(uint32_t channel)
{
#if FELUCCA_HOME_LEVELS
    return channel<HOME_LEVEL_CHANNELS?(uint16_t)home_level_pub[channel]:0;
#else
    (void)channel;return 0;
#endif
}
/* UI-only intensity classification: rest (<-54 dBFS), low, medium, high. */
static uint8_t home_level_band(uint16_t level)
{
    return level<64u?0:level<2048u?1:level<8192u?2:3;
}
typedef struct {
    uint32_t last_ms;
    uint16_t level[HOME_LEVEL_CHANNELS];
    uint8_t band[HOME_LEVEL_CHANNELS], valid;
} home_level_view_t; /* caller-owned, initialize to zero; no drawing is performed */
/* Returns a channel dirty mask; hidden pages never animate. Active poses may tick
 * at 20 Hz, unchanged resting poses request no redraw. Unsigned time handles wrap. */
static uint32_t home_level_frame(home_level_view_t *v, uint32_t now, int visible)
{
    uint32_t dirty=0;
    if(!visible){v->valid=0;return 0;}
    if(v->valid && (uint32_t)(now-v->last_ms)<HOME_LEVEL_FRAME_MS)return 0;
    for(uint32_t p=0;p<HOME_LEVEL_CHANNELS;p++) {
        uint16_t level=home_level_read(p);uint8_t band=home_level_band(level);
        if(!v->valid || band || band!=v->band[p])dirty|=1u<<p;
        v->level[p]=level;v->band[p]=band;
    }
    v->last_ms=now;v->valid=1;return dirty;
}
#endif
