/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"C editor %u: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){reset();uint8_t neg[]={0,1,1,1,0,119,0};CHECK(request(ED_STABLE,neg,sizeof neg)>5 && host_wire[6]==0);for(unsigned t=0;t<4;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}
 for(unsigned t=0;t<4;t++)for(unsigned c=0;c<=100;c++){
  uint8_t a[]={2,1,1,1,0,t,118,0,(c+8192)&127,(c+8192)>>7};CHECK(request(ED_STABLE,a,sizeof a)>5 && host_wire[6]==0);CHECK(trk[t].p[118]==(int)c);CHECK(grain_grid_param(1,0,3)==118 && track_desc(&trk[t],grain_grid_param(1,0,3))->max==100);
  CHECK(!motion.count);CHECK(ml_learn(70,t,118)==2);for(unsigned j=0;j<t;j++)CHECK(trk[j].p[118]==100);
 }
 song.sel=0;uint8_t a[]={0,118};CHECK(!request(ED_DESC,a,2));CHECK(!request(ED_SET,(uint8_t[]){0,118,55,64},4));CHECK(param_desc_of(ENGI_GRAIN,118)->max==100);
 for(unsigned cmd=0;cmd<2;cmd++){uint8_t arg[]={1,20,8,64};CHECK(!request(cmd?ED_DUMP:ED_SET,arg,cmd?0:4));CHECK(TSEL->p[118]==100);}
 uint8_t track[]={0};CHECK(!request(ED_TRACK_DUMP,track,1));CHECK(TSEL->p[118]==100);
 CHECK(up_store(0,"C100")==3);uint8_t slot[]={0};CHECK(!request(ED_UP_GET,slot,1));CHECK(up_value(up_rec(0),118)==100);
 uint8_t msg[4+2*119+32]={0,ENGI_GRAIN,'X',0};for(unsigned i=0;i<119;i++){msg[4+2*i]=0;msg[5+2*i]=64;}
 up_rec_t r,old=*up_rec(0);unsigned k=0;CHECK(up_parse(msg,sizeof msg,&r,&k)!=0);CHECK(!memcmp(&old,up_rec(0),192));
 CHECK(!up_parse(msg,sizeof msg-2,&r,&k));CHECK(r.np==118&&r.ver==4);CHECK(up_put(0,&r)==1);CHECK(!memcmp(&old,up_rec(0),192));
 CHECK(request(ED_UP_ERASE,slot,1)>5 && host_wire[6]!=0);CHECK(up_value(up_rec(0),118)==100);
 CHECK(request(ED_UP_STORE,(uint8_t[]){0,'X',0},3)>5 && host_wire[6]!=0);CHECK(up_value(up_rec(0),118)==100);
 CHECK(request(ED_UP_LOAD,slot,1)>5 && host_wire[6]!=0);CHECK(TSEL->p[118]==100);
 CHECK(!request(ED_PRESET,(uint8_t[]){ENGI_GRAIN,0},2));CHECK(TSEL->p[118]==100);
 CHECK(request(ED_SET,(uint8_t[]){0,P_LEVEL,50,64},4)>5);CHECK(TSEL->p[118]==100);
 TSEL->p[118]=0;CHECK(request(ED_DUMP,0,0)>5);CHECK(request(ED_TRACK_DUMP,track,1)>5);
 CHECK(up_store(1,"C0")==3);slot[0]=1;CHECK(request(ED_UP_GET,slot,1)>5);
 /* Factory/init defaults, contextual sharing and undo. */
 TSEL->p[118]=55;TSEL->p[P_GTYPE]=2;CHECK(TSEL->p[118]==55);apply_preset_to(TSEL,1);CHECK(TSEL->p[118]==0);undo_swap();CHECK(TSEL->p[118]==55);
 clean_engine_of(TSEL);CHECK(TSEL->p[118]==0);
 printf("GRC editor: %u checks passed; headless scalar, legacy bulk refusal, defaults and undo\n",checks);return 0;
}
