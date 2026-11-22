/* SPDX-License-Identifier: GPL-3.0-only */
/* Hardware row A=TYPE, B=personality A, C=personality B, D=internal C/118.
 * Keep the canonical editor/storage name unchanged. */
static const char *grain_effect_label(const track_t *t)
{
    static const char *const labels[]={"SPREAD","DRIFT","CHAOS","SPARKLE"};
    unsigned type=(unsigned)t->p[P_GTYPE];
    return labels[type<4?type:0];
}
static int grain_grid_active(void)
{
    return !ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot &&
        !name_on() && !seq_counting() && grain_grid_page(cur_page());
}
static int parameter_grid_active(void)
{
    return !ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot &&
        !name_on() && !seq_counting() && parameter_grid_rows(cur_page());
}
static struct {uint32_t sig[12];uint8_t valid,channel,page,row;} grain_grid;
static void draw_grain_grid(void)
{
    int env=env_grid_page(cur_page()),lfo=lfo_grid_page(cur_page());
    int wave=ui.force;
    unsigned page=lfo?3u:env?2u:grain_grid_page(cur_page())-1,channel=song.sel&3u;
    uint16_t accent=GRAIN_ACCENT[channel];
    int full=ui.force || !grain_grid.valid || grain_grid.channel!=channel || grain_grid.page!=page || grain_grid.row!=grain_edit_row;
    if(full)wave=1;
    cv_oy=0;cv_scroll=0;cv_flip=0;
    if(full) {
        memset(ui.roll,0,sizeof ui.roll); /* cards from the previous page are no longer rendered */
        lcd_fill(0,24,240,Y_FOOT-24,0);
    }
    for(unsigned row=0;row<(env?2u:3u);row++)for(unsigned col=0;col<4;col++) {
        unsigned id=lfo?lfo_grid_param(row,col):env?env_grid_param(row,col):grain_grid_param(page,row,col),cell=row*4+col;
        int selected=row==grain_edit_row;
        int value=id<P_COUNT?TSEL->p[id]:0;
        uint32_t sig=lfo?((uint32_t)(value+64)|((uint32_t)TSEL->p[P_LSYNC]<<8)|((uint32_t)id<<16)):
            ((uint32_t)(value+64)|((uint32_t)TSEL->p[env?P_ESYNC:P_GTYPE]<<9)|((uint32_t)id<<12));
        if(!full && grain_grid.sig[cell]==sig)continue;
        grain_grid.sig[cell]=sig;
        if(lfo && (id==P_LWAVE || id==P_LPHASE || id==P_LPOL))wave=1;
        uint16_t bg=selected?0x18c4:0x0841;
        uint16_t edge=selected?accent:GRAIN_COFFEE;
        unsigned h=lfo?44u:50u;
        cv_begin(57,h,0);cv_rrect(0,0,57,h,-7,edge,0);
        cv_rrect(1,1,55,h-2,-6,selected?edge:bg,edge);
        if(selected)cv_rrect(2,2,53,h-4,-5,bg,edge);
        if(id<P_COUNT) {
            const param_desc_t *d=track_desc(TSEL,id);char text[16];const char *unit;
            const char *label=id==P_GRC?grain_effect_label(TSEL):id==P_E5?"G-SPRD":id==P_SPRD?"V-SPRD":id==P_GRES?"RESON":d->label;
            int split=str_eq(label,"HARMONICS");
            if(split) {
                cv_text_in(2,1,53,&AF_S,"HARMO",0xffff,bg);
                cv_text_in(2,12,53,&AF_S,"NICS",0xffff,bg);
            } else {
                cv_text_in(2,3,53,&AF_S,label,0xffff,bg);
                cv_rect(7,16,43,1,selected?accent:GRAIN_COFFEE);
            }
            param_format(d,value,text,&unit);
            if(!env && !lfo && id==P_GWIN)unit=value==0?"GAUSS":value==64?"HANN":value==127?"TUKEY":"";
            const aafont_t *font=text_w(&AF_M,text)<=51?&AF_M:&AF_S;
            if(lfo) {
                str_cpy(text+str_len(text),unit,sizeof text-str_len(text));
                cv_text_in(3,21,51,&AF_S,text,0xffff,bg);
            } else if(split) {
                str_cpy(text+str_len(text),unit,sizeof text-str_len(text));
                cv_text_in(3,28,51,&AF_S,text,0xffff,bg);
            } else {
                cv_text_in(3,17,51,font,text,0xffff,bg);
                if(*unit)cv_text_in(3,35,51,&AF_S,unit,0xbdf7,bg);
            }
            if(d->max>d->min && d->fmt!=F_ENUM) {
                cv_rect(5,h-4,47,1,GRAIN_COFFEE);
                unsigned w=(uint32_t)(clamp(value,d->min,d->max)-d->min)*47u/(d->max-d->min);
                if(w)cv_rect(5,h-4,w,1,accent);
            }
            fmt_named=0;
        } else cv_text_in(2,20,53,&AF_S,"--",0x8410,bg);
        cv_blit(3+59*col,28+(lfo?47u:54u)*row);
    }
    if(lfo && wave) {
        cv_begin(240,32,0);cv_oy=0;graph_ht=30;
        graph_lfo(TSEL,0xfce0);graph_ht=100;cv_blit(0,169);
    }
    if(env) {
        int changed=full;
        for(unsigned k=0;k<4;k++) {
            uint32_t value=(uint32_t)TSEL->p[env_grid_param(0,k)];
            if(grain_grid.sig[8+k]!=value)changed=1;
            grain_grid.sig[8+k]=value;
        }
        if(changed) {
            /* Existing linear attack/exponential decay+release, scaled to this panel. */
            cv_begin(240,44,0);cv_oy=0;graph_ht=42;
            graph_adsr(TSEL,0xfce0);graph_ht=100;cv_blit(0,140);
        }
    }
    grain_grid.valid=1;grain_grid.channel=channel;grain_grid.page=page;grain_grid.row=grain_edit_row;
}

/* Share the grid signature cache; these pages cannot be visible simultaneously. */
static int matrix_visuals_page(void)
{
    return FELUCCA_GRAIN_ONLY && !ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot &&
        !name_on() && !seq_counting() && cur_page()->graph==GR_MOD;
}
static void draw_matrix_visuals(void)
{
    unsigned ch=song.sel&3u,slot=mod_ui_slot;uint16_t accent=GRAIN_ACCENT[ch];
    int full=ui.force || !grain_grid.valid || grain_grid.page!=4 || grain_grid.channel!=ch || grain_grid.row!=slot;
    int top=full;cv_oy=cv_scroll=cv_flip=0;
    if(full){memset(ui.roll,0,sizeof ui.roll);lcd_fill(0,24,240,Y_FOOT-24,0);}
    for(unsigned row=0;row<4;row++) {
        const int16_t *p=&TSEL->p[P_M1SRC+3*row];int changed=full;
        for(unsigned k=0;k<3;k++){unsigned value=(unsigned)(p[k]+64);if(grain_grid.sig[row*3+k]!=value)changed=1;grain_grid.sig[row*3+k]=value;}
        if(!changed)continue;if(row==slot)top=1;
        uint16_t bg=row==slot?0x18c4:0x0841,edge=row==slot?accent:GRAIN_COFFEE;
        cv_begin(234,26,0);cv_rrect(0,0,234,26,-6,edge,0);cv_rrect(1,1,232,24,-5,bg,edge);
        char b[16];const char *unit;b[0]=(char)('1'+row);b[1]=0;
        cv_text_on(6,7,&AF_S,b,0xffff,bg);
        cv_text_in(21,7,55,&AF_S,N_MSRC[clamp(p[0],0,MS_N-1)],0xffff,bg);
        cv_line(79,12,91,12,accent);cv_line(87,8,91,12,accent);cv_line(87,16,91,12,accent);
        cv_text_in(98,7,65,&AF_S,mod_dst_name(TSEL,p[1]),0xffff,bg);
        param_format(&TP[P_M1AMT],p[2],b,&unit);str_cpy(b+str_len(b),unit,sizeof b-str_len(b));
        cv_text_r(228,7,&AF_S,b,0xffff,bg);cv_blit(3,82+29*row);fmt_named=0;
    }
    if(top)for(unsigned col=0;col<4;col++) {
        unsigned id=P_M1SRC+3*slot;char b[16];const char *unit="",*value=b;
        if(!col){fmt_int(b,slot+1);str_cpy(b+str_len(b),"/4",4);}
        else if(col==1)value=N_MSRC[clamp(TSEL->p[id],0,MS_N-1)];
        else if(col==2)value=mod_dst_name(TSEL,TSEL->p[id+1]);
        else{param_format(&TP[P_M1AMT],TSEL->p[id+2],b,&unit);str_cpy(b+str_len(b),unit,sizeof b-str_len(b));}
        cv_begin(57,44,0);cv_rrect(0,0,57,44,-7,accent,0);cv_rrect(1,1,55,42,-6,0x18c4,accent);
        cv_text_in(2,3,53,&AF_S,(col==0?"SLOT":col==1?"SRC":col==2?"DST":"AMT"),0xffff,0x18c4);cv_rect(7,16,43,1,GRAIN_COFFEE);
        cv_text_in(3,23,51,&AF_S,value,0xffff,0x18c4);cv_blit(3+59*col,28);fmt_named=0;
    }
    grain_grid.valid=1;grain_grid.page=4;grain_grid.channel=ch;grain_grid.row=slot;
}
