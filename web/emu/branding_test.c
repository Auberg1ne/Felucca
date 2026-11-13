/* SPDX-License-Identifier: GPL-3.0-only */
/* The browser build's real splash and About renderer, without changing firmware identity. */
#include <stdint.h>
#include <string.h>
static unsigned branded, old_brand;
static void branding_text(const char *s)
{
    if (!strcmp(s,"COFFEE GRAINS")) branded++;
    if (!strcmp(s,"FELUCCA") || !strcmp(s,"Felucca")) old_brand++;
}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) branding_text(s)
#define GFX_HOOK_BLIT(x,y,r0) ((void)0)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
static unsigned checks;
#define REQUIRE(x) do {checks++;if(!(x)){fprintf(stderr,"branding line %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    REQUIRE(!strcmp(SPLASH_LINES[0],"COFFEE GRAINS"));
    REQUIRE(text_w(&AF_M,SPLASH_LINES[0])<=SPL_SQ-2*SPL_PAD);
    REQUIRE(text_w(&AF_L,"COFFEE GRAINS")<=226);
    web_nor_erase();web_boot();
    REQUIRE(branded>0 && old_brand==0);
    branded=old_brand=0;ui.menu=2;ui.menu_scroll=0;ui.force=1;ui_draw();
    REQUIRE(branded>0 && old_brand==0);
    REQUIRE(P_COUNT==118 && PROJ_STORE_SIZE==3840 && SMP_NALL==15 && SMP_USER_SLOTS==8);
    REQUIRE(smp_user_from_source(5)==0 && smp_user_from_source(12)==7);
    REQUIRE(!strcmp(SMP_ALL_NAMES[13],"FLUTE") && !strcmp(SMP_ALL_NAMES[14],"SAX"));
    printf("browser branding: %u checks passed; splash/About names fit existing fonts\n",checks);
    return 0;
}
