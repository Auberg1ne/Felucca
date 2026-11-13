/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
static uint32_t bad_blits,blit_pixels;
static void watch_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{if(x+w>240 || y+h>240 || w*h>240u*124u)bad_blits++;blit_pixels+=w*h;}
#define GFX_HOOK_TEXT(x0,y0,x1,y1,s,flags) ((void)0)
#define GFX_HOOK_BLIT(x,y,r0) watch_blit(x,y,cv_w,cv_h)
#define GFX_HOOK_BEGIN() ((void)0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "felucca_web.c"
static unsigned checks;
#define REQUIRE(x) do{checks++;if(!(x)){fprintf(stderr,"HOME visuals line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static void picture(const char *path)
{
    if(!path)return;FILE *f=fopen(path,"wb");if(!f)return;fprintf(f,"P6\n240 240\n255\n");
    for(unsigned p=0;p<240*240;p++) {
        unsigned v=swap16(web_fb[p]);unsigned char rgb[]={((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,3,1,f);
    }fclose(f);
}
int main(int argc,char **argv)
{
    web_nor_erase();web_boot();while(fm1_ms<3500)web_ms();
    REQUIRE(ui.home && !web_splash_pending && scientist_home.valid);
    REQUIRE(scientist_home.selected==song.sel);
    for(unsigned c=0;c<4;c++)REQUIRE(scientist_home.pose[c]==0 && scientist_home.mode[c]==0);
    for(unsigned c=0;c<4;c++) {
        unsigned ink=0;for(unsigned y=2;y<14;y++)for(unsigned x=6;x<30;x++)
            ink+=swap16(web_fb[(scientist_y(c)+y)*240+scientist_x(c)+x])>0x8410;
        REQUIRE(ink>5); /* labels remain readable in the default GREY theme */
    }
    if(argc>1)picture(argv[1]);
    REQUIRE(scientist_mode(0,95)==0 && scientist_mode(0,96)==1);
    REQUIRE(scientist_mode(1,48)==1 && scientist_mode(1,47)==0);
    REQUIRE(scientist_mode(1,8999)==1 && scientist_mode(1,9000)==2);
    REQUIRE(scientist_mode(2,7000)==2 && scientist_mode(2,6999)==1);
    for(unsigned c=0;c<4;c++) {
        for(unsigned p=0;p<4;p++)home_level_pub[p]=p==c?20000:0;
        fm1_ms+=50;ui.force=0;ui_draw();
        REQUIRE(scientist_home.mode[c]==2 && scientist_home.pose[c]==2);
        REQUIRE(web_fb[scientist_y(song.sel)*240+scientist_x(song.sel)]==swap16(HOME_ACCENT[song.sel]));
        for(unsigned p=0;p<4;p++)if(p!=c)REQUIRE(scientist_home.pose[p]==0);
    }
    for(unsigned p=0;p<4;p++)home_level_pub[p]=0;
    fm1_ms+=50;ui_draw();
    for(unsigned c=0;c<4;c++) {
        song.sel=c;ui.force=1;ui_draw(); /* selection moves even at the same animation time */
        REQUIRE(scientist_home.selected==c);
        for(unsigned p=0;p<4;p++) {
            REQUIRE(web_fb[scientist_y(p)*240+scientist_x(p)]==swap16(scientist_frame_color(p,p==c)));
            REQUIRE(scientist_home.pose[p]==0);
        }
    }
    web_enc(EN_ALGO,-1);ui_input();ui_draw();
    REQUIRE(song.sel==2 && scientist_home.selected==2); /* actual physical channel encoder */
    home_level_pub[0]=20000;fm1_ms+=1;uint8_t old=scientist_home.pose[0];ui.force=0;ui_draw();
    REQUIRE(scientist_home.pose[0]==old); /* no early animation */
    fm1_ms+=49;ui_draw();REQUIRE(scientist_home.pose[0]==2);
    uint8_t phase=scientist_home.phase[0];ui.menu=1;fm1_ms+=500;ui.force=1;ui_draw();
    REQUIRE(!scientist_home.valid && scientist_home.phase[0]==phase);ui.menu=0;
    ui.home=0;fm1_ms+=500;ui.force=1;ui_draw();REQUIRE(!scientist_home.valid && scientist_home.phase[0]==phase);
    ui.home=1;ui.force=1;ui_draw();REQUIRE(scientist_home.valid);
    home_level_pub[0]=0;home_level_pub[1]=500;home_level_pub[2]=5000;home_level_pub[3]=20000;
    song.sel=1;fm1_ms+=50;ui.force=1;ui_draw();if(argc>2)picture(argv[2]);
    REQUIRE(!bad_blits);
    home_level_pub[0]=500;home_level_pub[1]=home_level_pub[2]=home_level_pub[3]=0;
    fm1_ms+=50;ui.force=0;ui_draw();
    blit_pixels=0;ui.force=0;ui_draw();REQUIRE(blit_pixels==0); /* unchanged frame: no copy */
    home_level_pub[0]=1000;fm1_ms+=50;ui_draw();
    REQUIRE(blit_pixels<=230); /* same pose, only the tiny meter changes */
    ui_prefs|=PREF_ANIM_OFF;home_level_pub[0]=20000;
    fm1_ms+=50;ui_draw();uint8_t still=scientist_home.pose[0],phase_off=scientist_home.phase[0];
    for(unsigned i=0;i<8;i++){fm1_ms+=50;ui_draw();REQUIRE(scientist_home.pose[0]==still && scientist_home.phase[0]==phase_off);}
    ui_prefs&=~PREF_ANIM_OFF;
    REQUIRE(HOME_SPRITE_W+8<HOME_PANEL_W-2 && HOME_SPRITE_H+14<=HOME_PANEL_H-2);
    REQUIRE(scientist_y(3)+HOME_PANEL_H<=Y_FOOT && scientist_x(3)+HOME_PANEL_W<=240);
    printf("HOME visuals: %u checks passed; state %zu B, sprite data %zu B\n",checks,sizeof scientist_home,sizeof HOME_SPRITE_DATA+sizeof HOME_SPRITE_OFFSETS+sizeof HOME_SPRITE_PALETTE);
    return 0;
}
