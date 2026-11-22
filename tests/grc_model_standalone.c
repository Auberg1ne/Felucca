/* SPDX-License-Identifier: GPL-3.0-only */
/* No production headers, engine code, P_COUNT, or generated assets. */
#include <stdio.h>
#include "grc_migration_model.h"
int main(void){
 int16_t input[144]={0},defaults[118]={0},historical[119],stable[119];
 for(unsigned i=0;i<119;i++)input[i]=(int16_t)((int)i-64);
 input[109]=64;input[118]=55;
 if(!mc_historical(historical,input,119,119,defaults)||historical[110]!=47||historical[117]!=55||historical[118]!=0)return 1;
 if(!mc_stable(stable,input,119,119,8)||stable[110]!=46||stable[117]!=53||stable[118]!=55)return 1;
 if(sizeof(mc_runtime118)!=4840||sizeof(mc_runtime119)!=4848||sizeof(mc_project)!=3840||sizeof(mc_preset)!=192)return 1;
 puts("Standalone GRC model passed without production headers or definitions");return 0;
}
