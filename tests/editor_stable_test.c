/* SPDX-License-Identifier: GPL-3.0-only */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"stable line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t neg[]={0,1,1,17,2,118,0};
static unsigned ext(uint8_t *a,unsigned n){unsigned got=request(ED_STABLE,a,n);return got>=12?host_wire[6]:99;}
int main(void){
 reset();memset(&ed_stable,0,sizeof ed_stable);for(unsigned t=0;t<4;t++){set_engine_of(&trk[t],ENGI_GRAIN);trk[t].engine=ENGI_GRAIN;}
 CHECK(P_COUNT==118);CHECK(request(ED_INFO,0,0)>5);int cap=0;for(unsigned i=0;i+6<host_wire_n;i++)if(host_wire[i]==0x58&&host_wire[i+1]==1&&host_wire[i+2]==82){cap++;CHECK(host_wire[i+4]==118&&host_wire[i+5]==0&&host_wire[i+6]==7);}CHECK(cap==1);
 uint8_t get[]={1,1,1,17,2,0,109,0};CHECK(ext(get,sizeof get)==2);
 CHECK(ext(neg,sizeof neg)==0);CHECK(host_wire[11]==118&&host_wire[13]==7);
 for(unsigned t=0;t<4;t++)for(unsigned w=0;w<128;w++){
  uint8_t put[]={2,1,1,17,2,t,109,0,w,64};CHECK(ext(put,sizeof put)==0);CHECK(trk[t].p[109]==(int)w);CHECK(!motion.count);
  get[5]=t;CHECK(ext(get,sizeof get)==0);CHECK(ed_rv(host_wire+14)==(int)w);
 }
 /* Every truncated prefix/extra tail must reject without touching tracks or NOR. */
 uint8_t put[]={2,1,1,17,2,0,109,0,64,64,0};static track_t before[4];
 for(unsigned n=0;n<sizeof put;n++){if(n==10)continue;memcpy(before,trk,sizeof before);unsigned e=host_erases,p=host_writes;CHECK(ext(put,n)!=0);CHECK(!memcmp(before,trk,sizeof before));CHECK(e==host_erases&&p==host_writes);CHECK(ext(neg,sizeof neg)==0);}
 CHECK(ext(put,sizeof put)==1);put[6]=118;CHECK(ext(put,10)==4);put[6]=109;put[9]=65;CHECK(ext(put,10)==4);put[9]=64;
 uint8_t newer[]={0,2,1,17,2,118,0};CHECK(ext(newer,sizeof newer)==3);CHECK(ext(get,sizeof get)==2);
 newer[1]=1;newer[5]=119;CHECK(ext(newer,sizeof newer)==3);CHECK(ext(get,sizeof get)==2);CHECK(ext(neg,sizeof neg)==0);
 uint8_t wrong[]={1,1,1,18,2,0,109,0};CHECK(ext(wrong,sizeof wrong)==2);CHECK(ext(get,sizeof get)==0);
 usb.config=0;CHECK(ext(get,sizeof get)==2);usb.config=1;CHECK(ext(get,sizeof get)==2);CHECK(ext(neg,sizeof neg)==0);
 uint8_t badcount[]={0,1,1,17,2,0,0};CHECK(ext(badcount,sizeof badcount)==3);CHECK(ext(neg,sizeof neg)==0);
 for(unsigned seed=0;seed<128;seed++){
  uint8_t invalid[]={2,1,1,17,2,0,118,0,seed,64};memcpy(before,trk,sizeof before);
  CHECK(ext(invalid,sizeof invalid)==4);CHECK(!memcmp(before,trk,sizeof before));
 }
 usb.resets++;CHECK(ext(get,sizeof get)==2);CHECK(ext(neg,sizeof neg)==0);fm1_ms+=5000;CHECK(ext(get,sizeof get)==2);
 fm1_ms=UINT32_MAX-100;CHECK(ext(neg,sizeof neg)==0);fm1_ms=50;CHECK(ext(get,sizeof get)==0);
 uint8_t snap[]={3,1,1,17,2,0};CHECK(ext(snap,sizeof snap)==0);CHECK(host_wire_n==253 && host_wire[14]==118);
 uint8_t bulk[8+2*119]={4,1,1,17,2,0,119,0};memcpy(before,trk,sizeof before);CHECK(ext(bulk,sizeof bulk)==1);CHECK(!memcmp(before,trk,sizeof before));
 /* Unmarked119 is never the stable layout. Real legacy framing remains118. */
 uint8_t legacy[4+2*119+32]={0,ENGI_GRAIN,'X',0};
 for(unsigned i=0;i<119;i++){int v=i<118?param_desc_of(ENGI_GRAIN,i)->def:55;unsigned u=v+8192;legacy[4+2*i]=u&127;legacy[5+2*i]=u>>7;}
 up_bank_t bank=up_bank[0];unsigned erase_before=host_erases,write_before=host_writes;
 CHECK(request(ED_UP_PUT,legacy,sizeof legacy)>5 && host_wire[6]==1);CHECK(!memcmp(&bank,&up_bank[0],sizeof bank));CHECK(host_erases==erase_before&&host_writes==write_before);
 uint8_t oldframe[4+2*118+32]={0,ENGI_GRAIN,'X',0};memcpy(oldframe+4,legacy+4,2*118);up_rec_t parsed;unsigned slot;
 CHECK(!up_parse(oldframe,sizeof oldframe,&parsed,&slot));CHECK(parsed.np==118 && parsed.ver==UP_LEGACY_PACKED);
 /* Pure coverage contract for future storage, NOT a live/persistent C value. */
 CHECK(ed_legacy_coverage(118,118));CHECK(!ed_legacy_coverage(119,118));CHECK(!ed_legacy_coverage(119,119));
 for(unsigned c=0;c<4;c++){
  int16_t model[119]={0},frozen[119],replacement[118]={0};model[118]=(int[]){0,1,55,100}[c];memcpy(frozen,model,sizeof model);
  /* Model the mandatory future guard before memcpy, not a live C parameter. */
  if(ed_legacy_coverage(119,118))memcpy(model,replacement,sizeof replacement);
  CHECK(!memcmp(model,frozen,sizeof model));
  uint8_t unrelated[]={0,P_LEVEL,50,64};CHECK(request(ED_SET,unrelated,4)>5);
  model[P_LEVEL]=TSEL->p[P_LEVEL];CHECK(model[118]==frozen[118]);
 }
 CHECK(!motion_param(109));CHECK(ml_learn(70,0,109)==2);
 printf("Stable editor: %u checks passed; negotiated existing IDs only, malformed atomicity, no C/bulk advertisement\n",checks);return 0;
}
