/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef STARTUP_SPLASH_H
#define STARTUP_SPLASH_H
#include <stdint.h>
#define STARTUP_SPLASH_MS 3500u
/* Until a target link proves fit, artwork is enabled by default only in preview. */
#ifndef COFFEE_STARTUP_ART
#if defined(FELUCCA_WEB) && FELUCCA_WEB
#define COFFEE_STARTUP_ART 1
#else
#define COFFEE_STARTUP_ART 0
#endif
#endif
static int startup_splash_active(uint32_t now, uint32_t start)
{
    return (uint32_t)(now - start) < STARTUP_SPLASH_MS;
}
#endif
