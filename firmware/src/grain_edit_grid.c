/* SPDX-License-Identifier: GPL-3.0-only */
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
static const uint16_t GRAIN_ACCENT[4]={0x07ff,0xffe0,0xf81f,0x07e0};
/* Warm brown from HOME_SPRITE_PALETTE[189], converted from panel byte order.
 * Constants avoid linking scientist bitmaps on hardware just for a color. */
#define GRAIN_COFFEE 0x4964u
static void draw_grain_grid(void)
{
    int env=env_grid_page(cur_page());
    unsigned page=env?2u:grain_grid_page(cur_page())-1,channel=song.sel&3u;
    uint16_t accent=GRAIN_ACCENT[channel];
    int full=ui.force || !grain_grid.valid || grain_grid.channel!=channel || grain_grid.page!=page || grain_grid.row!=grain_edit_row;
    cv_oy=0;cv_scroll=0;cv_flip=0;
    if(full) {
        memset(ui.roll,0,sizeof ui.roll); /* cards from the previous page are no longer rendered */
        lcd_fill(0,24,240,Y_FOOT-24,0);
    }
    for(unsigned row=0;row<(env?2u:3u);row++)for(unsigned col=0;col<4;col++) {
        unsigned id=env?env_grid_param(row,col):grain_grid_param(page,row,col),cell=row*4+col;
        int selected=row==grain_edit_row;
        int value=id<P_COUNT?TSEL->p[id]:0;
        uint32_t sig=(uint32_t)(value+64)|((uint32_t)TSEL->p[env?P_ESYNC:P_GTYPE]<<9)|((uint32_t)id<<12);
        if(!full && grain_grid.sig[cell]==sig)continue;
        grain_grid.sig[cell]=sig;
        uint16_t bg=selected?0x18c4:0x0841;
        uint16_t edge=selected?accent:GRAIN_COFFEE;
        cv_begin(57,50,0);cv_rrect(0,0,57,50,-7,edge,0);
        cv_rrect(1,1,55,48,-6,selected?edge:bg,edge);
        if(selected)cv_rrect(2,2,53,46,-5,bg,edge);
        if(id<P_COUNT) {
            const param_desc_t *d=track_desc(TSEL,id);char text[16];const char *unit;
            const char *label=id==P_E5?"G-SPRD":id==P_SPRD?"V-SPRD":id==P_GRES?"RESON":d->label;
            int split=str_eq(label,"HARMONICS");
            if(split) {
                cv_text_in(2,1,53,&AF_S,"HARMO",0xffff,bg);
                cv_text_in(2,12,53,&AF_S,"NICS",0xffff,bg);
            } else {
                cv_text_in(2,3,53,&AF_S,label,0xffff,bg);
                cv_rect(7,16,43,1,selected?accent:GRAIN_COFFEE);
            }
            param_format(d,value,text,&unit);
            const aafont_t *font=text_w(&AF_M,text)<=51?&AF_M:&AF_S;
            if(split) {
                str_cpy(text+str_len(text),unit,sizeof text-str_len(text));
                cv_text_in(3,28,51,&AF_S,text,0xffff,bg);
            } else {
                cv_text_in(3,17,51,font,text,0xffff,bg);
                if(*unit)cv_text_in(3,35,51,&AF_S,unit,0xbdf7,bg);
            }
            if(d->max>d->min && d->fmt!=F_ENUM) {
                cv_rect(5,46,47,1,GRAIN_COFFEE);
                unsigned w=(uint32_t)(clamp(value,d->min,d->max)-d->min)*47u/(d->max-d->min);
                if(w)cv_rect(5,46,w,1,accent);
            }
            fmt_named=0;
        } else cv_text_in(2,20,53,&AF_S,"--",0x8410,bg);
        cv_blit(3+59*col,28+54*row);
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
    if(full) {
        char text[32];str_cpy(text,env?"ENV 1/1 R":page?"CHAR/VOICE 2/2 R":"GRAINS 1/2 R",sizeof text);
        fmt_int(text+str_len(text),grain_edit_row+1);str_cpy(text+str_len(text),env?"/2":"/3",4);
        cv_begin(240,12,0);cv_text(4,0,&AF_S,text,0xffff);cv_text_r(236,0,&AF_S,"SELECT ROW",0xbdf7,0);cv_blit(0,188);
    }
    grain_grid.valid=1;grain_grid.channel=channel;grain_grid.page=page;grain_grid.row=grain_edit_row;
}
