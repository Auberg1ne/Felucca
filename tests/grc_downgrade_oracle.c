/* SPDX-License-Identifier: GPL-3.0-only */
/* Compile against the exact Step240E source, never against the new reader. */
#ifndef GRC_OLD_BRIDGE_SOURCE
#error supply the pinned old bridge source
#endif
#define BRIDGE_TEST_NO_MAIN 1
#include GRC_OLD_BRIDGE_SOURCE
static void read_blob(const char *dir,const char *name,void *dst,unsigned len){char path[512];snprintf(path,sizeof path,"%s/%s",dir,name);FILE*f=fopen(path,"rb");CHECK(f&&fread(dst,1,len,f)==len&&fgetc(f)==EOF);fclose(f);}
static void physical(uint32_t obj,unsigned copy,const void *data,unsigned len,unsigned seq){uint32_t base=st_sector(obj,copy);st_hdr_t h;memset(&h,255,sizeof h);h.magic=ST_MAGIC;h.type=obj;h.slot=copy;h.seq=seq;h.len=len;h.crc=st_crc32(data,len);h.hcrc=st_crc32(&h,sizeof h-4);memcpy(nor+base+ST_PAYLOAD_OFF,data,len);memcpy(nor+base,&h,sizeof h);}
int main(int argc,char**argv){CHECK(argc==2);CHECK(P_COUNT==118 && UP_VER==6 && sizeof(project_t)==4840);static project_store_t c;static up_bank_t bank,known;
 read_blob(argv[1],"fund-c100.bin",&c,3840);read_blob(argv[1],"bank-c100.bin",&bank,3080);CHECK(*(uint32_t*)c.raw==0x46554e44u && c.raw[66]==119);CHECK(bank.r[0].ver==8 && bank.r[0].np==119 && bank.r[0].packed[118]==164);
 for(unsigned side=0;side<2;side++)for(unsigned slot=0;slot<4;slot++){
  boot();physical(OBJ_PROJECT0+slot,side,&c,3840,1);CHECK(project_slot_state(slot)==PS_UNSUPPORTED);CHECK(project_used(slot));freeze();unsigned e=erases,p=bridge_programs;
  CHECK(project_save(slot)!=0);unchanged(e,p);CHECK(project_rename(slot,"NO")!=0);unchanged(e,p);CHECK(project_erase(slot)!=0);unchanged(e,p);
  memset(&proj_slot[slot],0,sizeof proj_slot[slot]);freeze();CHECK(project_save(slot)!=0);unchanged(e,p);
 }
 for(unsigned b=0;b<2;b++)for(unsigned side=0;side<2;side++){
  boot();CHECK(!up_store(b*16,"KNOWN"));known=up_bank[b];memset(nor+st_sector(OBJ_UPRESET0+b,0),255,8192);physical(OBJ_UPRESET0+b,side,&bank,3080,2);physical(OBJ_UPRESET0+b,1-side,&known,3080,1);up_boot();
  freeze();unsigned e=erases,p=bridge_programs;CHECK(up_store(b*16,"NO")!=0);unchanged(e,p);CHECK(up_store(b*16+1,"NO")!=0);unchanged(e,p);
  memset(up_bank,0,sizeof up_bank);freeze();CHECK(up_store(b*16+1,"NO")!=0);unchanged(e,p);CHECK(st_save(OBJ_UPRESET0+b,&known,sizeof known)!=0);unchanged(e,p);
 }
 printf("GRC downgrade oracle: %u checks passed; actual FUND/8 C100 raw bytes preserved by Step240E\n",checks);return 0;
}
