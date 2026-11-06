/* SPDX-License-Identifier: GPL-3.0-only */
#define SAMPLE_XIP_TEST_NO_MAIN 1
#include "sample_xip_test.c"
static uint32_t snap(void)
{
    uint32_t h = 0;
#define HASH(x) h = h * 33u + st_crc32(&(x), sizeof(x))
    HASH(nor); HASH(song); HASH(trk); HASH(motion); HASH(ed_w); HASH(as); HASH(persist_pending);
    HASH(proj_scratch); HASH(usr_zone); HASH(usr_nz); HASH(smp_user_phys); HASH(smp_user_sectors);
    HASH(smp_journal_state); HASH(flash_ok); HASH(web_flash_writes);
    HASH(transport_req); HASH(settings); HASH(persist_saved);
    HASH(smp_user_generation); HASH(smp_user_header_bound); HASH(smp_user_header_crc);
    HASH(smp_user_unresolved); HASH(smp_user_occupied); HASH(slc_man); HASH(slc_usr);
#undef HASH
    return h;
}
static uint32_t submit(uint32_t e, uint32_t t, uint32_t cmd, uint32_t n, uint32_t a, uint32_t b)
{
    uint8_t p[8] = {0xF0,0x7D,0x46,0x4C,cmd,a,b,0}; p[5+n]=0xF7;
    memcpy(web_editor_input(),p,6+n); return web_editor_submit(e,t,6+n);
}
static int allowed(uint32_t media)
{
    if(media==2)web_nor_erase();else fixture(media==1);
    if(media==3)nor[SMP_USER_BASE]^=1; /* torn anchor: never format/recover it */
    web_boot(); if(web_editor_open(1))return 0;
    const uint32_t cases[][4]={{1,0,0,0},{2,2,0,0},{2,2,1,0},{5,2,0,P_E0},{10,1,4,0},{25,0,0,0},{27,0,0,0},{29,1,0,0},{31,2,3,P_E0}};
    uint32_t before=snap();
    for(uint32_t i=0;i<NELEM(cases);i++) {
        if(submit(1,i+1,cases[i][0],cases[i][1],cases[i][2],cases[i][3]))return 0;
        if(web_ed_calls || i) { /* submit itself must never dispatch */ if(web_ed_calls!=i)return 0; }
        fm1_ms+=15;web_render(0);
        uint32_t n=web_editor_length();const uint8_t *r=web_editor_output();
        if(n<6 || n>664 || r[0]!=0xF0 || r[4]!=cases[i][0] || r[n-1]!=0xF7 || web_ed_calls!=i+1)return 0;
        web_editor_pop(1,i+1);
    }
    return before==snap() && !web_editor_forbidden && !FELUCCA_OTA;
}
static int deny(uint32_t unused)
{
    (void)unused;web_nor_erase();web_boot();web_editor_open(7);uint32_t before=snap();
    for(uint32_t cmd=0;cmd<128;cmd++)for(uint32_t n=0;n<=2;n++) {
        uint8_t p[8]={0xF0,0x7D,0x46,0x4C,cmd,0,0,0};p[5+n]=0xF7;
        int yes=(n==0 && (cmd==1||cmd==25||cmd==27)) || (n==1 && (cmd==10||cmd==29)) || (n==2 && (cmd==2||cmd==5||cmd==31));
        memcpy(web_ed_input,p,6+n);
        uint32_t r=web_editor_submit(7,1,6+n);
        if((r==0)!=yes)return 0;
        web_editor_cancel(7,1);web_ed_last_ticket=0; /* test fixture only, no dispatch */
    }
    for(uint32_t pos=0;pos<8;pos++) {
        uint8_t p[8]={0xF0,0x7D,0x46,0x4C,31,0,0,0xF7};p[pos]=0xFF;
        memcpy(web_ed_input,p,8);if(web_editor_submit(7,1,8)!=1)return 0;
    }
    if(submit(7,1,31,2,NTRK,0)!=1 || submit(7,1,31,2,0,P_COUNT)!=1 ||
       submit(7,1,2,2,2,0)!=1 || submit(7,1,2,2,1,G_COUNT)!=1 || submit(7,1,10,1,NENGINES,0)!=1)return 0;
    for(uint32_t cmd=2;cmd<=5;cmd+=3)
        if(submit(7,1,cmd,2,0,P_COUNT)!=1 || submit(7,1,cmd,2,1,G_COUNT)!=1 || submit(7,1,cmd,2,2,0)!=1)return 0;
    if(submit(7,1,29,1,NTRK,0)!=1)return 0;
    for(uint32_t n=0;n<6;n++)if(web_editor_submit(7,1,n)!=1)return 0;
    if(web_editor_submit(7,1,9)!=1 || web_editor_submit(7,1,UINT32_MAX)!=1)return 0;
    fm1_ms+=15;web_render(0);
    return !web_ed_calls && before==snap() && !web_editor_forbidden;
}
static int queues(uint32_t unused)
{
    (void)unused;web_nor_erase();web_boot();fm1_ms=0;
    if(web_editor_open(0)!=3 || web_editor_open(3) || web_editor_open(4)!=3)return 0;
    if(submit(3,1,1,0,0,0) || submit(3,2,25,0,0,0)!=2 || submit(4,2,25,0,0,0)!=3)return 0;
    web_render(0);if(web_ed_calls)return 0;
    fm1_ms=14;web_render(0);if(web_ed_calls)return 0;
    fm1_ms=15;web_render(0);if(web_ed_calls!=1 || !web_ed_len)return 0;
    if(submit(3,2,25,0,0,0)!=2)return 0;
    web_editor_pop(4,1);if(!web_ed_len)return 0;
    web_editor_pop(3,1);
    if(submit(3,1,25,0,0,0)!=3 || submit(3,2,25,0,0,0))return 0;
    web_editor_cancel(3,2);fm1_ms=30;web_render(0);if(web_ed_calls!=1)return 0;
    submit(3,3,25,0,0,0);web_editor_close(3);fm1_ms=45;web_render(0);
    if(web_ed_calls!=1 || web_ed_len || web_ed_pending || web_editor_open(3)!=3 || web_editor_open(2)!=3 || web_editor_open(4))return 0;
    return submit(3,4,25,0,0,0)==3 && !web_editor_forbidden;
}
static int sources(uint32_t unused)
{
    (void)unused;fixture(0);web_boot();web_editor_open(1);uint32_t ticket=0,baseline=snap();
    for(uint32_t e=0;e<NENGINES;e++) {
        host_preset(&trk[0],e,0);
        /* Every factory-name reply must fit in one nonblocking response. */
        if(submit(1,++ticket,10,1,e,0))return 0;fm1_ms+=15;web_render(0);
        uint8_t *p=web_editor_output();uint32_t n=web_editor_length();
        if(p[5]!=e || p[6]!=ENGINES[e]->npresets || n>=664)return 0;
        web_editor_pop(1,ticket);
    }
    for(uint32_t j=0;j<3;j++) {
        uint32_t e=j==0?4:j==1?8:13;host_preset(&trk[0],e,0);
        if(submit(1,++ticket,5,2,0,P_E0))return 0;fm1_ms+=15;web_render(0);
        uint8_t *p=web_editor_output();uint32_t n=web_editor_length();
        for(uint32_t k=0;k<8;k++) {
            char name[]="USR1";name[3]+=k;int found=0;
            for(uint32_t at=0;at+5<=n;at++)found|=!memcmp(p+at,name,5);
            if(!found)return 0;
        }
        web_editor_pop(1,ticket);
    }
    submit(1,++ticket,29,1,0,0);fm1_ms+=15;web_render(0);
    return web_editor_length()>192 && !web_editor_forbidden;
}
static int audio(uint32_t unused)
{
    (void)unused;int fds[2];if(pipe(fds))return 0;
    for(uint32_t mode=0;mode<2;mode++) {
        pid_t p=fork();if(p<0)return 0;
        if(!p) {
            close(fds[0]);fixture(0);web_boot();web_editor_open(1);uint32_t h=0,t=0;
            for(uint32_t b=0;b<600;b++) {
                if(b==180)web_midi(0x90,60,100);
                if(b==250)web_midi(0xB0,1,80);
                if(b==500)web_midi(0x80,60,0);
                if(mode && !web_ed_pending && !web_ed_len)submit(1,++t,29,1,0,0);
                web_render(128);if(web_ed_len)web_editor_pop(1,t);
                h=h*33u+st_crc32(out_l,sizeof(float)*128)+st_crc32(out_r,sizeof(float)*128);
            }
            h=h*33u+snap();write(fds[1],&h,sizeof h);_exit(web_editor_forbidden?1:0);
        }
        int status;waitpid(p,&status,0);if(!WIFEXITED(status)||WEXITSTATUS(status))return 0;
    }
    close(fds[1]);uint32_t h[2];return read(fds[0],h,sizeof h)==sizeof h && h[0]==h[1];
}
static int forbidden(uint32_t unused)
{
    (void)unused;web_nor_erase();web_boot();memcpy(pool_before,nor+SMP_USER_BASE,sizeof pool_before);
    uint32_t took;web_editor_dispatching=1;
    if(fl_write(SMP_USER_BASE,"x",1)!=-8 || fl_erase4k(SMP_USER_BASE,&took)!=-8)return 0;
    fl_inval(0,1);audio_silence();if(ed_flash_stop()!=1)return 0;
    if(st_prog(SMP_USER_BASE,"x",1)!=-8 || st_erase(SMP_USER_BASE)!=-8)return 0;
    uint32_t before=snap();web_ed_settings_save();web_ed_autosave_hold();
    return web_editor_forbidden==9 && unchanged() && before==snap();
}
/* Native integration adapter for Node tests: byte length + complete frame in;
 * status,u16 length,complete firmware reply out. Test fixture only. */
static int wire(uint32_t engine)
{
    fixture(0);web_boot();host_preset(&trk[0],engine,0);web_editor_open(1);uint32_t ticket=0,baseline=snap();
    int n;
    while((n=getchar())!=EOF) {
        uint8_t p[255];if(fread(p,1,n,stdin)!=(uint32_t)n)return 1;
        if(n<=8)memcpy(web_ed_input,p,n);
        uint32_t rc=web_editor_submit(1,++ticket,n);
        if(!rc){fm1_ms+=15;web_render(0);}
        uint32_t len=web_editor_length();putchar(rc);putchar(len&255);putchar(len>>8);
        if(len)fwrite(web_ed_output,1,len,stdout);fflush(stdout);web_editor_pop(1,ticket);
    }
    return web_editor_forbidden || baseline!=snap()?1:0;
}
int main(int argc,char **argv)
{
    if(argc>1 && !strcmp(argv[1],"--wire"))return wire(argc>2?(uint32_t)atoi(argv[2]):0);
    run("S4A actual handlers: eight query forms, snapshots unchanged",allowed,0,0);
    run("S4A quarantined historical publications remain byte-for-byte unchanged",allowed,1,0);
    run("S4A blank pool queries never format or claim flash",allowed,2,0);
    run("S4A locked torn anchor queries preserve flash and state",allowed,3,0);
    run("S4A default-deny all IDs, lengths, ranges and framing",deny,0,0);
    run("S4A bounded RX/TX, 15 ms cadence, cancellation and epochs",queues,0,0);
    run("S4A real source numbering, all NAMES, response over 192 bytes",sources,0,0);
    run("S4A identical audio and musical state with queries and ordinary MIDI",audio,0,0);
    run("S4A forbidden adapters fail closed and record every attempt",forbidden,0,0);
    return failures?1:0;
}
