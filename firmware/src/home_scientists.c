/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME-only UI. Audio publishing and all physical input paths remain unchanged. */
#ifndef COFFEE_HOME_VISUALS
#if defined(FELUCCA_WEB) && FELUCCA_WEB
#define COFFEE_HOME_VISUALS 1
#else
#define COFFEE_HOME_VISUALS 0
#endif
#endif
#if COFFEE_HOME_VISUALS
#include "../../assets/home-scientists/home_sprites.h"
#define HOME_PANEL_W 116u
#define HOME_PANEL_H 72u
static const uint16_t HOME_ACCENT[4]={0x07ff,0xffe0,0xf81f,0x07e0};
static struct {
    home_level_view_t levels;
    uint32_t head_sig;
    uint8_t mode[4],phase[4],pose[4],meter[4],selected,valid;
} scientist_home;
static uint8_t scientist_mode(uint8_t old,uint16_t level)
{
    if(old==2 && level>=7000)return 2;
    if(level>=9000)return 2;
    if(old && level>=48)return 1;
    return level>=96?1:0;
}
static uint8_t scientist_meter(uint16_t level)
{
    static const uint16_t edge[]={64,256,512,1024,2048,4096,8192,16384};
    uint8_t count=0;for(unsigned i=0;i<8;i++)count+=level>=edge[i];return count;
}
static void scientists_hide(void)
{
    scientist_home.valid=0;scientist_home.levels.valid=0;
}
static void scientist_sprite(uint32_t channel,uint32_t pose,uint32_t dx,uint32_t dy)
{
    uint32_t frame=channel*3u+pose,p=HOME_SPRITE_OFFSETS[frame],end=HOME_SPRITE_OFFSETS[frame+1],out=0;
    while(p<end) {
        uint32_t token=HOME_SPRITE_DATA[p++],n=(token&127u)+1u;
        uint32_t color=token&128u?HOME_SPRITE_DATA[p++]:0;
        while(n--) {
            uint16_t pixel=HOME_SPRITE_PALETTE[token&128u?color:HOME_SPRITE_DATA[p++]];
            cv_px[(dy+out/HOME_SPRITE_W)*cv_w+dx+out%HOME_SPRITE_W]=pixel;out++;
        }
    }
}
static uint32_t scientist_x(uint32_t c){return 2u+(c&1u)*120u;}
static uint32_t scientist_y(uint32_t c){return 54u+(c>>1u)*74u;}
static uint16_t scientist_frame_color(uint32_t c,int selected)
{
    return selected?HOME_ACCENT[c]:(HOME_ACCENT[c]&0xf7deu)>>1;
}
static void scientist_border(uint32_t c,int selected)
{
    uint32_t x=scientist_x(c),y=scientist_y(c);uint16_t color=scientist_frame_color(c,selected);
    lcd_fill(x,y,HOME_PANEL_W,2,color);lcd_fill(x,y+HOME_PANEL_H-2,HOME_PANEL_W,2,color);
    lcd_fill(x,y+2,2,HOME_PANEL_H-4,color);lcd_fill(x+HOME_PANEL_W-2,y+2,2,HOME_PANEL_H-4,color);
}
static void scientist_panel(uint32_t c)
{
    char label[]="CH1";label[2]+=(char)c;
    cv_begin(HOME_PANEL_W,HOME_PANEL_H,0);
    scientist_sprite(c,scientist_home.pose[c],8,14);
    cv_text(6,2,&AF_S,label,0xffff);
    for(unsigned i=0;i<8;i++)cv_rect(104,64-(int32_t)i*6,5,4,i<scientist_home.meter[c]?HOME_ACCENT[c]:0x2104);
    uint16_t edge=scientist_frame_color(c,c==(song.sel&3u));
    cv_frame(0,0,HOME_PANEL_W,HOME_PANEL_H,edge);cv_frame(1,1,HOME_PANEL_W-2,HOME_PANEL_H-2,edge);
    cv_blit(scientist_x(c),scientist_y(c));
}
static void scientist_pose_draw(uint32_t c)
{
    cv_begin(HOME_SPRITE_W,HOME_SPRITE_H,0);scientist_sprite(c,scientist_home.pose[c],0,0);
    cv_blit(scientist_x(c)+8,scientist_y(c)+14);
}
static void scientist_meter_draw(uint32_t c)
{
    cv_begin(5,46,0);
    for(unsigned i=0;i<8;i++)cv_rect(0,42-(int32_t)i*6,5,4,i<scientist_home.meter[c]?HOME_ACCENT[c]:0x2104);
    cv_blit(scientist_x(c)+104,scientist_y(c)+22);
}
static void scientist_knobs(void)
{
    cv_begin(240,28,0);
    for(unsigned c=0;c<4;c++) {
        int x=12+(int)c*60;char label[]="K1";label[1]+=(char)c;
        /* Neutral fixed indicator, deliberately not a value or parameter mapping. */
        for(int y=-6;y<=6;y++)for(int a=-6;a<=6;a++) {
            int r=a*a+y*y;if(r>=25 && r<=40)cv_rect(x+a,12+y,1,1,0x8410);
        }
        cv_line(x,12,x,7,0xffff);cv_text(x+12,1,&AF_S,label,0xbdf7);
        cv_text(x+12,14,&AF_S,"--",0x8410);
    }
    cv_blit(0,24);
}
static void scientist_header(int force)
{
    char text[12];uint32_t sig=(song.sel&3u)|(((uint32_t)song.g[G_BPM]&1023u)<<2)|
        ((uint32_t)(song.playing!=0)<<12)|(((uint32_t)(song.octave+8)&31u)<<13)|
        ((uint32_t)batt_shown()<<18)|((uint32_t)(song.rec&15u)<<21);
    /* Keep the existing tempo-roll history synchronized for unchanged pages. */
    if(song.g[G_BPM]!=ui.roll_bpm) {
        char before[8],after[8];fmt_int(before,ui.roll_bpm);fmt_int(after,song.g[G_BPM]);
        roll_note(ROLL_BPM,before,after,ui.force);ui.roll_bpm=song.g[G_BPM];
    } else if(ui.force)ui.roll[ROLL_BPM].from[0]=0;
    if(ui.roll[ROLL_BPM].from[0] && (uint8_t)(ui.frame-ui.roll[ROLL_BPM].t0)+1u>=ROLL_FRAMES)
        ui.roll[ROLL_BPM].from[0]=0;
    if(ui.msg_t){draw_head();scientist_home.head_sig=UINT32_MAX;return;}
    if(!force && sig==scientist_home.head_sig)return;
    scientist_home.head_sig=sig;
    cv_begin(240,H_HEAD,0);cv_text(4,4,&AF_S,"COFFEE GRAINS",0xffff);
    if(song.rec){cv_frame(99,8,6,6,0xf800);if((song.rec>>song.sel)&1u)cv_rect(100,9,4,4,0xf800);}
    fmt_int(text,song.g[G_BPM]);cv_text(116,4,&AF_S,text,0xbdf7);
    cv_text(145,4,&AF_S,song.playing?">":"-",0xbdf7);
    text[0]='O';fmt_int(text+1,song.octave);cv_text(161,4,&AF_S,text,0x8410);
    text[0]='C';text[1]='H';text[2]=(char)('1'+(song.sel&3u));text[3]=0;
    cv_text(201,4,&AF_S,text,0xffff);
    cv_rect(201,18,24,2,HOME_ACCENT[song.sel&3u]);
    cv_frame(231,7,7,10,0x8410);cv_rect(233,9,3,batt_level()*2,0xbdf7);
    if(batt_shown()==4)cv_line(235,8,233,15,0xffff);
    cv_blit(0,0);
}
static void draw_scientists_home(void)
{
    cv_oy=0;cv_scroll=0;cv_flip=0;
    uint32_t old_time=scientist_home.levels.last_ms;
    int was_valid=scientist_home.levels.valid;
    (void)home_levels_poll(&scientist_home.levels,fm1_ms);
    int tick=scientist_home.levels.valid && (!was_valid || old_time!=scientist_home.levels.last_ms);
    int full=!scientist_home.valid || (ui.force && scientist_home.selected==(song.sel&3u));
    if(full){lcd_fill(0,0,240,Y_FOOT,0);scientist_knobs();}
    for(unsigned c=0;c<4;c++) {
        uint8_t pose=scientist_home.pose[c],meter=scientist_home.meter[c];
        if(tick || !scientist_home.valid) {
            uint16_t level=scientist_home.levels.level[c];
            uint8_t mode=scientist_mode(scientist_home.mode[c],level);
            if(mode!=scientist_home.mode[c] || !scientist_home.valid)scientist_home.phase[c]=0;
            else if(mode && !(ui_prefs&PREF_ANIM_OFF))scientist_home.phase[c]=(scientist_home.phase[c]+1u)%12u;
            if(ui_prefs&PREF_ANIM_OFF)pose=mode;
            else if(!mode)pose=0;
            else if(mode==2)pose=((scientist_home.phase[c]/2u)&1u)?1:2;
            else pose=((scientist_home.phase[c]/(level<2048u?6u:3u))&1u)?0:1;
            scientist_home.mode[c]=mode;meter=scientist_meter(level);
        }
        int pose_dirty=pose!=scientist_home.pose[c],meter_dirty=meter!=scientist_home.meter[c];
        scientist_home.pose[c]=pose;scientist_home.meter[c]=meter;
        if(full)scientist_panel(c);
        else {if(pose_dirty)scientist_pose_draw(c);if(meter_dirty)scientist_meter_draw(c);}
        if(!full && scientist_home.selected!=(song.sel&3u) && (c==scientist_home.selected || c==(song.sel&3u)))
            scientist_border(c,c==(song.sel&3u)); /* immediate selection, independent of 20-Hz poses */
    }
    scientist_header(full);
    scientist_home.selected=(uint8_t)(song.sel&3u);scientist_home.valid=1;
}
#endif
