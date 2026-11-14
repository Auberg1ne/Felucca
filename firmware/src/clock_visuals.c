/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef COFFEE_CLOCK_VISUALS
#define COFFEE_CLOCK_VISUALS COFFEE_HOME_VISUALS
#endif
static int clock_visuals_page(void)
{
    return COFFEE_CLOCK_VISUALS && !ui.home && !ui.menu && !ui.layer && !ui.confirm &&
        !ui.uboot && !name_on() && !seq_counting() && cur_page()->scope==SC_GLOBAL &&
        cur_page()->id[0]==G_BPM;
}
#if COFFEE_CLOCK_VISUALS
static struct {uint32_t sig[3],position[2],ms,beat;uint8_t valid,channel;} clock_visuals;
static const uint8_t CLOCK_ID[3]={G_BPM,G_SWING,G_CLOCK};
/* Selected track's actual sequencer index, just like the footer. The start
 * sentinel means step zero is pending, not the final step of the old pattern. */
static unsigned clock_step(void)
{
    return TSEL->seq_pos>=0x7fffffffu?0u:(TSEL->seq_idx&15u);
}
static void clock_bean(int x,int y,uint16_t color)
{
    for(int dy=-5;dy<=5;dy++)for(int dx=-4;dx<=4;dx++)
        if(dx*dx*25+dy*dy*16<=400)cv_pset(x+dx,y+dy,color);
    cv_line(x-2,y+4,x-1,y+2,0);cv_line(x-1,y+2,x+1,y,0);
    cv_line(x+1,y,x,y-2,0);cv_line(x,y-2,x+2,y-4,0);
}
static void draw_clock_visuals(void)
{
    int full=ui.force || !clock_visuals.valid;
    uint16_t color=HOME_ACCENT[song.sel&3u];
    uint32_t now=fm1_ms,dt=now-clock_visuals.ms,decay=32768;
    int reset=full && !clock_visuals.valid;
    if(clock_visuals.channel!=(song.sel&3u))full=1;
    if(dt>50)dt=50;
    for(unsigned n=0;n<dt;n++)decay=(decay*32361u+16384u)>>15;
    clock_visuals.ms=now;clock_visuals.channel=song.sel&3u;
    cv_oy=0;cv_scroll=0;cv_flip=0;
    if(full) {
        draw_frame();
        cv_begin(240,H_HEAD,0);cv_text(4,4,&AF_S,"COFFEE GRAINS",0xffff);
        cv_text_r(235,4,&AF_S,"CLOCK",0xffff,0);cv_blit(0,0);
        lcd_fill(0,24,240,4,0);lcd_fill(0,72,240,4,0);
        lcd_fill(0,198,240,4,0);
    }
    for(unsigned k=0;k<3;k++) {
        const param_desc_t *d=&GP[CLOCK_ID[k]];int value=song.g[CLOCK_ID[k]];
        uint32_t sig=(uint32_t)(value-d->min);int moved=0;
        if(k<2) {
            uint32_t target=(uint32_t)(clamp(value,d->min,d->max)-d->min)*32768u/(d->max-d->min),old=clock_visuals.position[k];
            clock_visuals.position[k]=reset?target:mixer_top_advance(old,target,decay);
            moved=old!=clock_visuals.position[k];
        }
        if(!full && !moved && sig==clock_visuals.sig[k])continue;
        clock_visuals.sig[k]=sig;
        char value_text[16],text[24];const char *unit;param_format(d,value,value_text,&unit);
        str_cpy(text,value_text,sizeof text);
        if(*unit){str_cpy(text+str_len(text)," ",2);str_cpy(text+str_len(text),unit,sizeof text-str_len(text));}
        cv_begin(80,44,0);cv_text_in(2,0,76,&AF_S,d->label,0xffff,0);
        if(k<2)mixer_top_ring(40,21,clock_visuals.position[k],color);
        else {cv_frame(33,15,14,13,color);cv_rect(36+value*3,19,2,5,color);}
        cv_text_in(2,31,76,&AF_S,text,0xffff,0);cv_blit(k*80,28);fmt_named=0;
    }
    unsigned step=clock_step();uint32_t beat=step|((song.playing!=0)<<5)|((unsigned)song.g[G_CLOCK]<<6)|((song.sel&3u)<<8);
    if(full || beat!=clock_visuals.beat) {
        static const int8_t xy[16][2]={{0,-48},{18,-44},{34,-34},{44,-18},{48,0},{44,18},{34,34},{18,44},{0,48},{-18,44},{-34,34},{-44,18},{-48,0},{-44,-18},{-34,-34},{-18,-44}};
        if(full) {
            cv_begin(240,122,0);
            for(unsigned i=0;i<16;i++) {
                int x=120+xy[i][0],y=60+xy[i][1];
                clock_bean(x,y,song.playing && i==step?color:(HOME_ACCENT[i>>2]&0xf7deu)>>1);
                if(song.playing && i==step)cv_frame(x-6,y-7,13,15,0xffff);
            }
            cv_blit(0,Y_GRAPH);
        } else {
            /* Only the old/new bean and center change on a sequencer tick. */
            unsigned old=clock_visuals.beat&31u;
            for(unsigned n=0;n<2;n++) {
                unsigned i=n?step:old;int active=n && song.playing;
                cv_begin(13,15,0);clock_bean(6,7,active?color:(HOME_ACCENT[i>>2]&0xf7deu)>>1);
                if(active)cv_frame(0,0,13,15,0xffff);
                cv_blit(114+xy[i][0],Y_GRAPH+53+xy[i][1]);
            }
        }
        cv_begin(72,48,0);
        char text[16];if(song.playing){fmt_int(text,step+1);cv_text_in(0,8,72,&AF_M,text,0xffff,0);}
        else cv_text_in(0,10,72,&AF_S,"STOP",0xffff,0);
        cv_text_in(0,34,72,&AF_S,N_CLOCK[song.g[G_CLOCK]],0xffff,0);
        cv_blit(84,Y_GRAPH+36);clock_visuals.beat=beat;
    }
    clock_visuals.valid=1;
}
#endif
