/* SPDX-License-Identifier: GPL-3.0-only */
#if COFFEE_HOME_VISUALS
/* Q15 exponential decay, tau=80 ms. Bound stalls to 50 ms; the existing
 * monotonic display clock wraps naturally. No timers or audio-side work. */
static uint32_t mixer_top_advance(uint32_t position,uint32_t target,uint32_t decay)
{
    int32_t error=(int32_t)position-(int32_t)target;
    if(error<32 && error>-32)return target;
    int32_t next=(int32_t)target+error*(int32_t)decay/32768;
    if(next-(int32_t)target<32 && next-(int32_t)target>-32)return target;
    return (uint32_t)next;
}
static void mixer_top_ring(int32_t cx,int32_t cy,uint32_t position,uint16_t color)
{
    static const int8_t dir[9][2]={{-5,5},{-7,1},{-6,-4},{-3,-6},{0,-7},{3,-6},{6,-4},{7,1},{5,5}};
    for(int y=-7;y<=7;y++)for(int x=-7;x<=7;x++) {
        int r=x*x+y*y;if(r>=36 && r<=55 && !(y>0 && x>-3 && x<3))cv_pset(cx+x,cy+y,0x4208);
    }
    unsigned scaled=position*8u,i=scaled>>15,f=scaled&32767u;
    if(i>=8){i=7;f=32768;}
    int x=(dir[i][0]*(int32_t)(32768-f)+dir[i+1][0]*(int32_t)f)/32768;
    int y=(dir[i][1]*(int32_t)(32768-f)+dir[i+1][1]*(int32_t)f)/32768;
    cv_line(cx,cy,cx+x,cy+y,color);
}
#endif
