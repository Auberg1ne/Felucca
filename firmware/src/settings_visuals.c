/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef COFFEE_SETTINGS_VISUALS
#define COFFEE_SETTINGS_VISUALS 1
#endif
#if COFFEE_SETTINGS_VISUALS
static const uint16_t SETTINGS_ACCENT[MTAB_COUNT]={0x07ff,0xffe0,0xf81f,0x07e0,0xa81f};
/* Explicit keycap colors keep the dark skin readable with legacy light palettes. */
static int32_t settings_visual_hint(int32_t x,int32_t y,unsigned key,const char *action)
{
    x=cv_keycap(x,y,key,0xbdf7,0,0);
    return cv_text_on(x+KH_GAP,y-1,&AF_S,action,0xffff,0);
}
static void settings_visual_row(unsigned k,unsigned row,unsigned tab,unsigned count)
{
    int h=count>6?18:20,y=78+(int)k*(h+2),selected=row==ui.menu_sel;
    uint16_t accent=SETTINGS_ACCENT[tab],bg=selected?0x18c4:0x0841;
    const char *value=menu_valued(row)?menu_vname(row,menu_get(row)):"";
    char text[16];str_cpy(text,value,sizeof text);
    if(row==MI_HOLD){text[0]='0';text[1]='.';text[2]=(char)('0'+HOLD_MS[settings_hold%4]/100);str_cpy(text+3," s",3);}
    if(row==MI_TUNE)str_cpy(text+str_len(text)," ct",sizeof text-str_len(text));
    if(row==MI_LCLEAR){str_cpy(text,"NONE",sizeof text);if(ml_count()){fmt_int(text,ml_count());str_cpy(text+str_len(text)," CC",sizeof text-str_len(text));}}
    int toggle=menu_valued(row) && menu_n(row)==2 && !MI_TEXT(row) && (str_eq(value,"ON") || str_eq(value,"OFF"));
    const aafont_t *label=&AF_M,*font=&AF_M;
    int width=toggle?70:row>=MI_VALUES?14:text_w(font,text);
    if(text_w(label,MI_NAME[row])+width+12>208)label=&AF_S;
    if(text_w(label,MI_NAME[row])+width+12>208){font=&AF_S;width=text_w(font,text);}
    cv_rrect(5,y,230,h,5,bg,0);
    if(selected){cv_rrect(5,y,230,h,5,accent,0);cv_rrect(6,y+1,228,h-2,4,bg,accent);cv_rect(8,y+4,2,h-8,accent);}
    int ly=y+(label==&AF_M?CAP_IN(M,h):CAP_IN(S,h));
    GFX_HOOK_ALIGN(0,y,0,y+h,AL_V,"menu row name centred up/down");
    cv_text_on(16,ly,label,MI_NAME[row],0xffff,bg);
    if(row>=MI_VALUES)cv_icon_in(216,y,0,h,12,ICON_X_RIGHT,0xffff,bg);
    else if(toggle){
        int x=191,ty=y+(h-12)/2,on=str_eq(value,"ON");
        GFX_HOOK_ALIGN(0,y,0,y+h,AL_V,"menu row value centred up/down");
        cv_text_r(184,y+CAP_IN(S,h),&AF_S,value,0xffff,bg);
        cv_rrect(x,ty,33,12,-6,on?accent:0x4208,bg);
        cv_rrect(on?x+22:x+2,ty+2,9,8,-4,on?0:0xffff,on?accent:0x4208);
    } else {
        int vy=y+(font==&AF_M?CAP_IN(M,h):CAP_IN(S,h));
        GFX_HOOK_ALIGN(0,y,0,y+h,AL_V,"menu row value centred up/down");
        cv_text_r(224,vy,font,text,selected?0xffff:0xbdf7,bg);
    }
}
static void draw_settings_visuals(void)
{
    unsigned tab=menu_tab(),count=menu_visible_count(tab);
    uint16_t accent=SETTINGS_ACCENT[tab];
    cv_oy=0;cv_scroll=0;cv_flip=0;
    cv_begin(240,24,0);cv_text(4,4,&AF_S,"COFFEE GRAINS",0xffff);
    settings_visual_hint(232-kh_w(KC_HOME,"CLOSE"),6,KC_HOME,"CLOSE");
    if(song.rec)cv_rect(122,9,4,4,0xf800);
    cv_blit(0,0);
    for(unsigned pass=0;pass<2;pass++) {
        unsigned top=pass?132:24;
        cv_begin(240,108,0);cv_oy=-(int)top;
        if(!pass) {
            for(unsigned t=0;t<MTAB_COUNT;t++) {
                int x=7+(int)t*46;
                cv_rrect(x,29,42,27,5,t==tab?0x18c4:0x0841,0);
                if(t==tab)cv_frame(x,29,42,27,SETTINGS_ACCENT[t]);
                cv_icon_in(x+13,29,0,27,16,MTAB_ICON[t],0xffff,t==tab?0x18c4:0x0841);
                cv_rect(x+10,57,22,t==tab?2:1,SETTINGS_ACCENT[t]);
            }
            cv_text_in(8,63,224,&AF_S,MTAB_NAME[tab],0xffff,0);
            cv_rect(8,74,224,1,accent);
        }
        for(unsigned k=0;k<count;k++) {
            int y=78+(int)k*((count>6?18:20)+2),h=count>6?18:20;
            if(y+h>(int)top && y<(int)top+108)settings_visual_row(k,menu_visible_row(tab,k),tab,count);
        }
        if(pass) {
            static const khint_t keys[3][3]={{{KC_ALGO,"TAB"},{KC_PRESETS,"ROW"},{KC_OCTPM,"VALUE"}},
                {{KC_ALGO,"TAB"},{KC_PRESETS,"ROW"},{KC_OCTUP,"OPEN"}},{{KC_ALGO,"TAB"},{KC_PRESETS,"ROW"},{KC_OCTUP,"CLEAR"}}};
            const khint_t *hint=keys[ui.menu_sel==MI_LCLEAR?2:!menu_valued(ui.menu_sel)];
            int w=kh_w(hint[0].key,hint[0].act)+kh_w(hint[1].key,hint[1].act)+kh_w(hint[2].key,hint[2].act);
            int gap=(224-w)/2,x=8;
            for(unsigned i=0;i<3;i++)x=settings_visual_hint(x,224,hint[i].key,hint[i].act)+gap;
        }
        cv_oy=0;cv_blit(0,top);
    }
}
#endif
