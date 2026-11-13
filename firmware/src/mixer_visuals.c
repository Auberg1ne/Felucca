/* SPDX-License-Identifier: GPL-3.0-only */
/* MIXER-only preview skin, using original parameters, meters and sprite data. */
#ifndef COFFEE_MIXER_VISUALS
#define COFFEE_MIXER_VISUALS COFFEE_HOME_VISUALS
#endif
#ifndef COFFEE_MIXER_TOP_KNOBS
#define COFFEE_MIXER_TOP_KNOBS COFFEE_MIXER_VISUALS
#endif
static int mixer_visuals_page(void)
{
    return COFFEE_MIXER_VISUALS && !ui.home && !ui.menu && !ui.layer && !ui.confirm &&
        !ui.uboot && !name_on() && !seq_counting() && cur_page()->scope==SC_TRK && cur_page()->graph==GR_TRK;
}
#if COFFEE_MIXER_VISUALS
static struct {uint32_t sig[4];uint8_t valid;} mixer_visuals;
static void mixer_portrait(uint32_t c)
{
    uint32_t p=HOME_SPRITE_OFFSETS[c*3u],end=HOME_SPRITE_OFFSETS[c*3u+1u],x=0,y=0;
    /* Fixed 2:1 sampling while decoding existing idle poses. No bitmap copies or
     * general transforms: even source rows/columns map into a 45x28 portrait. */
    while(p<end) {
        uint32_t token=HOME_SPRITE_DATA[p++],n=(token&127u)+1u,color=token&128u?HOME_SPRITE_DATA[p++]:0;
        while(n--) {
            uint16_t pixel=HOME_SPRITE_PALETTE[token&128u?color:HOME_SPRITE_DATA[p++]];
            if(!((x|y)&1u))cv_px[(16u+(y>>1))*cv_w+5u+(x>>1)]=pixel;
            if(++x==HOME_SPRITE_W){x=0;y++;}
        }
    }
}
static void mixer_ring(int32_t cx,int32_t cy,int32_t value,int32_t lo,int32_t hi,uint16_t color)
{
    static const int8_t dir[9][2]={{-5,5},{-7,1},{-6,-4},{-3,-6},{0,-7},{3,-6},{6,-4},{7,1},{5,5}};
    for(int y=-7;y<=7;y++)for(int x=-7;x<=7;x++) {
        int r=x*x+y*y;if(r>=36 && r<=55 && !(y>0 && x>-3 && x<3))cv_pset(cx+x,cy+y,0x4208);
    }
    uint32_t i=(uint32_t)(clamp(value,lo,hi)-lo)*8u/(uint32_t)(hi-lo);
    cv_line(cx,cy,cx+dir[i][0],cy+dir[i][1],color);
}
#if COFFEE_MIXER_TOP_KNOBS
/* Upper cells only: reuse the existing canvas, parameter formats and ranges. */
static struct {uint32_t sig[4];uint8_t valid;} mixer_top;
static const uint8_t MIXER_TOP_ID[4]={P_LEVEL,P_PAN,P_REV,P_MUTE};
static const char *const MIXER_TOP_LABEL[4]={"LEVEL","PAN","REV","MUTE"};
static void mixer_binary_indicator(int32_t x,int32_t y,int on,uint16_t color)
{
    /* Closed rim plus binary center: mute has no continuous position. */
    for(int32_t dy=-7;dy<=7;dy++)for(int32_t dx=-7;dx<=7;dx++) {
        int32_t r=dx*dx+dy*dy;
        if((r>=36 && r<=55) || (on && r<=16))cv_pset(x+dx,y+dy,color);
    }
}
static void draw_mixer_top_knobs(void)
{
    const track_t *t=TSEL;uint16_t color=HOME_ACCENT[song.sel&3u];
    cv_oy=0;cv_scroll=0;cv_flip=0;
    for(unsigned k=0;k<4;k++) {
        unsigned id=MIXER_TOP_ID[k];const param_desc_t *d=&TP[id];int32_t value=t->p[id];
        int hot=ui.hot_t && ui.hot_col==k;
        uint32_t sig=(uint32_t)(value-d->min)|((song.sel&3u)<<16)|((uint32_t)hot<<18);
        if(!ui.force && mixer_top.valid && sig==mixer_top.sig[k])continue;
        mixer_top.sig[k]=sig;
        char val[16],text[24];const char *unit;param_format(d,value,val,&unit);
        str_cpy(text,val,sizeof text);
        if(unit[0]){str_cpy(text+str_len(text)," ",2);str_cpy(text+str_len(text),unit,sizeof text-str_len(text));}
        cv_begin(CARD_W,CARD_H,0);
        cv_text_in(2,0,CARD_W-4,&AF_S,MIXER_TOP_LABEL[k],0xffff,0);
        if(hot)cv_rect(24,12,9,1,color);
        if(id==P_MUTE)mixer_binary_indicator(28,21,value!=0,color);
        else mixer_ring(28,21,value,d->min,d->max,color);
        cv_text_in(2,31,CARD_W-4,&AF_S,text,0xffff,0);
        cv_blit((uint32_t)CARD_X(k),Y_LABEL);
        fmt_named=0;
    }
    mixer_top.valid=1;
}
#endif
static void mixer_visuals_strip(uint32_t c)
{
    const track_t *t=&trk[c];uint16_t color=HOME_ACCENT[c];char text[16];
    int mute=t->p[P_MUTE]!=0,pan=clamp(t->p[P_PAN],-64,63),rev=clamp(t->p[P_REV],0,127);
    uint32_t level=trk_level(c);cv_begin(CARD_W,H_GRAPH,0);cv_oy=0;cv_scroll=0;
    text[0]='C';text[1]='H';text[2]=(char)('1'+c);text[3]=0;cv_text(5,2,&AF_S,text,0xffff);
    if((song.rec>>c)&1u)cv_text(36,2,&AF_S,song.playing?"R":"A",0xffff);
    if(midi_lit(c))cv_rect(49,9,3,3,color);
    mixer_portrait(c);
    cv_rect(51,16,3,28,0x2104);
    unsigned meter=(unsigned)ts.meter[c]*28u/(TS_MH-2u);if(meter>28)meter=28;
    if(meter)cv_rect(51,44-(int32_t)meter,3,meter,color);
    if(level){int db=LEVEL_DB_X10[level];fmt_int(text,(db+(db<0?-5:5))/10);str_cpy(text+str_len(text),"dB",4);}
    else str_cpy(text,"OFF",sizeof text);
    cv_text_in(3,45,51,&AF_S,text,0xffff,0);
    cv_rect(6,58,45,2,0x2104);if(level)cv_rect(6,58,(int32_t)level*45/127,2,color);
    mixer_ring(12,71,pan,-64,63,color);cv_text(22,65,&AF_S,"P",0xffff);
    text[0]='+';fmt_int(pan>0?text+1:text,pan*100/(pan>0?63:64));cv_text_r(53,65,&AF_S,text,0xffff,0);
    mixer_ring(12,91,rev,0,127,color);cv_text(22,85,&AF_S,"R",0xffff);
    fmt_int(text,(rev*100+63)/127);cv_text_r(53,85,&AF_S,text,0xffff,0);
    cv_text_in(3,103,51,&AF_S,mute?"M ON":"M OFF",0xffff,0);
    if(mute)cv_frame(4,102,49,14,0xf800);
    cv_rect(6,117,45,2,0x2104);if(rev)cv_rect(6,117,(rev*45+63)/127,2,color);
    uint16_t edge=c==(song.sel&3u)?color:(color&0xf7deu)>>1;
    cv_frame(0,0,CARD_W,H_GRAPH,edge);
    if(c==(song.sel&3u))cv_frame(1,1,CARD_W-2,H_GRAPH-2,edge);
    cv_blit((uint32_t)CARD_X(c),Y_GRAPH);
}
static void mixer_meter_only(uint32_t c)
{
    unsigned h=(unsigned)ts.meter[c]*28u/(TS_MH-2u);if(h>28)h=28;
    cv_begin(3,28,0x2104);if(h)cv_rect(0,28-(int32_t)h,3,h,HOME_ACCENT[c]);
    cv_blit((uint32_t)CARD_X(c)+51,Y_GRAPH+16);
}
static void draw_mixer_visuals(void)
{
    int full=ui.force || !mixer_visuals.valid;
    cv_oy=0;cv_scroll=0;cv_flip=0;
    if(full) {
        lcd_fill(0,24,240,4,0);lcd_fill(0,Y_SEP_END,240,Y_GRAPH-Y_SEP_END,0);
        lcd_fill(0,Y_GRAPH,3,H_GRAPH,0);lcd_fill(237,Y_GRAPH,3,H_GRAPH,0);
        for(unsigned c=0;c<3;c++)lcd_fill((uint32_t)CARD_X(c)+CARD_W,Y_GRAPH,2,H_GRAPH,0);
        lcd_fill(0,Y_GRAPH+H_GRAPH,240,Y_FOOT-(Y_GRAPH+H_GRAPH),0);
    }
    for(unsigned c=0;c<4;c++) {
        track_t *t=&trk[c];uint8_t previous=ts.meter[c];int m=t->p[P_MUTE]?0:meter_px(t->peak);t->peak=0;
        if(full)ts.meter[c]=0;
        if(m<ts.meter[c]-1)m=ts.meter[c]-1;
        ts.meter[c]=(uint8_t)(m<0?0:m);
        uint32_t sig=(uint32_t)trk_level(c)|((uint32_t)(t->p[P_PAN]+64)<<7)|
            ((uint32_t)t->p[P_REV]<<14)|((uint32_t)(t->p[P_MUTE]!=0)<<21)|
            ((uint32_t)(c==(song.sel&3u))<<22)|((uint32_t)((song.rec>>c)&1u)<<23)|
            ((uint32_t)(song.playing!=0)<<24)|((uint32_t)(midi_lit(c)!=0)<<25);
        if(full || sig!=mixer_visuals.sig[c]){mixer_visuals.sig[c]=sig;mixer_visuals_strip(c);}
        else if(previous!=ts.meter[c])mixer_meter_only(c);
    }
    mixer_visuals.valid=1;
}
#endif
