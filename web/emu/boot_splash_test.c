/* SPDX-License-Identifier: GPL-3.0-only */
/* Native execution of the real browser framebuffer/clock/audio startup path. */
#include "felucca_web.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"splash line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t screen_hash(void)
{
    const uint8_t *p=(const uint8_t *)web_fb;uint32_t h=2166136261u;
    for(unsigned i=0;i<sizeof web_fb;i++)h=(h^p[i])*16777619u;
    return h;
}
int main(void)
{
    REQUIRE(startup_splash_active(0,0) && startup_splash_active(3499,0));
    REQUIRE(!startup_splash_active(3500,0) && !startup_splash_active(3501,0));
    uint32_t start=UINT32_MAX-100;
    REQUIRE(startup_splash_active(start+3499u,start));
    REQUIRE(!startup_splash_active(start+3500u,start));
    web_nor_erase();web_boot();
    REQUIRE(web_splash_pending && kb_boot_hold && fm1_ms==0);
    REQUIRE(screen_hash()==0xe4dd1404u); /* independent Python RGB565 reconstruction */
    unsigned draws=web_draws;REQUIRE(draws==240);
    for(unsigned i=0;i<1000;i++)web_ms();
    web_midi(0x90,60,110); /* audio must work while the splash still holds the UI */
    while(fm1_ms<3499)web_ms();
    REQUIRE(web_splash_pending && kb_boot_hold && web_draws==draws);
    REQUIRE(screen_hash()==0xe4dd1404u);
    double energy=0;for(unsigned i=0;i<RING*2;i++)energy+=(double)ring[i]*ring[i];
    printf("startup MIDI audio energy: %.6f\n",energy);REQUIRE(energy>0.01);
    web_ms();REQUIRE(fm1_ms==3500 && !web_splash_pending && !kb_boot_hold);
    REQUIRE(screen_hash()!=0xe4dd1404u && web_draws>draws);
    REQUIRE(ui.home && !ui.menu); /* normal synth screen, no input needed */
    web_midi(0x80,60,0);web_ms();REQUIRE(!web_splash_pending);
    REQUIRE(sizeof BOOT_ART_DATA+sizeof BOOT_ART_PALETTE+sizeof BOOT_ART_ROWS==38755);
    printf("boot splash: %u checks passed; exact pixels, 3499/3500 ms, wrap, audio and HOME transition\n",checks);
    return 0;
}
