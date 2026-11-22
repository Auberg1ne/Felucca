/* SPDX-License-Identifier: GPL-3.0-only */
/* Step240B feasibility counterexamples ONLY, not a protective implementation. */
#define BRIDGE_TEST_NO_MAIN 1
#include "project_downgrade_bridge_test.c"
#include "grc_migration_model.h"
static int record_valid(const uint8_t*p,size_t n){up_rec_t r;if(n!=192)return 0;memcpy(&r,p,192);return up_valid(&r);}
static void future_bank(up_bank_t*b){
 mc_preset old,next;memcpy(&old,up_rec(0),192);CHECK(mc_preset_pack(&next,&old,55,record_valid));
 *b=up_bank[0];memcpy(&b->r[0],&next,192);
}
int main(void){
 boot();CHECK(!up_store(0,"SEED"));up_bank_t bank;future_bank(&bank);CHECK(!st_save(OBJ_UPRESET0,&bank,sizeof bank));up_boot();
 up_rec_t unknown=*up_rec(0);CHECK(grain_bank_readonly(&up_bank[0]));unsigned er=erases;CHECK(!up_store(1,"OTHER"));CHECK(erases>er);CHECK(!memcmp(&unknown,up_rec(0),192));
 puts("Valid shape: writing another slot rewrites the bank but preserves the unknown record bytes");
 for(unsigned order=0;order<2;order++)for(unsigned fault=0;fault<2;fault++){
  boot();CHECK(!up_store(0,"SEED"));if(order)CHECK(!up_store(0,"SEED2"));future_bank(&bank);CHECK(!st_save(OBJ_UPRESET0,&bank,sizeof bank));st_hdr_t header;int future_copy=st_current(OBJ_UPRESET0,&header);CHECK(future_copy==(order?0:1));
  unsigned address=st_sector(OBJ_UPRESET0,(unsigned)future_copy);nor[address+ST_PAYLOAD_OFF+20]^=1;
  up_boot();CHECK(up_used(0)&&up_rec(0)->ver==6&&!grain_bank_readonly(&up_bank[0]));
  uint8_t raw[4096];memcpy(raw,nor+address,4096);er=erases;if(fault)fail_after=0;
  int rc=up_store(0,"REPLACE");CHECK(fault?rc!=0:rc==0);CHECK(erases>er&&memcmp(raw,nor+address,4096));
  if(fault)for(unsigned i=0;i<4096;i++)CHECK(nor[address+i]==255);
  printf("CRC fallback: future copy%d erased during %s; caller rc=%d\n",future_copy,fault?"failed write before first payload program":"successful older-format replacement",rc);
 }
 printf("Feasibility probe: %u checks passed; no claim of protection for the demonstrated unsafe paths\n",checks);return 0;
}
