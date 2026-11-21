/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint64_t checks,old_in_range,old_defined,old_negative_shift,old_overflow,saturated;
#define REQUIRE(x) do { checks++;if(!(x)){fprintf(stderr,"headroom line %u: %s\n",__LINE__,#x);return 1;} } while(0)
/* Independent floor division, not the implementation's signed shifts. */
static int64_t floorq(__int128 x,unsigned bits)
{
    __int128 d=(__int128)1<<bits;
    return (int64_t)(x>=0?x/d:-((-x+d-1)/d));
}
static int32_t ref_sat(int64_t x) { return x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:(int32_t)x; }
static int64_t ref_amp(const vmod_t *m,unsigned i) { return m->amp0+((int64_t)m->amp1-m->amp0)*i/CTL; }
static int32_t ref_output(int32_t prior,int32_t x,const vmod_t *m,unsigned i)
{
    int64_t a=ref_amp(m,i),first=floorq((__int128)x*a,15);
    int64_t wet=floorq((__int128)first*VOICE_FS,15)*2,sum=(int64_t)prior+wet;
    return ref_sat(sum);
}
static int gain(void)
{
    const int32_t samples[]={INT32_MIN,-738023,-69636,-1,0,1,69636,738023,INT32_MAX};
    const unsigned indices[]={0,1,15,31};
    for(int a=0;a<=65527;a++)for(unsigned ramp=0;ramp<3;ramp++)for(unsigned si=0;si<NELEM(samples);si++)for(unsigned j=0;j<NELEM(indices);j++) {
        vmod_t m={0};m.amp0=ramp==0?0:a;m.amp1=ramp==1?0:a;
        unsigned i=indices[j];int32_t x=samples[si],prior=(a%3==0)?INT32_MAX-123:(a%3==1)?INT32_MIN+123:1000;
        int32_t got=gr_output(prior,x,&m,i);REQUIRE(got==ref_output(prior,x,&m,i));
        int64_t first_product=(int64_t)x*ref_amp(&m,i);
        int64_t first=floorq(first_product,15),second_product=first*VOICE_FS,wet=floorq(second_product,15)*2,sum=(int64_t)prior+wet;
        saturated+=sum>INT32_MAX || sum<INT32_MIN;
        if(first_product>=INT32_MIN && first_product<=INT32_MAX && second_product>=INT32_MIN && second_product<=INT32_MAX && sum>=INT32_MIN && sum<=INT32_MAX) {
            int32_t legacy=voice_amp(x,&m,i);REQUIRE(got==(int64_t)prior+(int64_t)legacy*2);old_in_range++;
            if(legacy>=0)old_defined++;else old_negative_shift++;
        } else old_overflow++;
    }
    /* Bounded negative ramps are stress cases too; preserve exact signs/rounding. */
    for(int a=-65527;a<=65527;a+=127)for(unsigned i=0;i<CTL;i++) {
        vmod_t m={0};m.amp0=a;m.amp1=-a;REQUIRE(gr_output(0,738023,&m,i)==ref_output(0,738023,&m,i));
    }
    return 0;
}
static int32_t ref_filter(const tsvf_t *c,int32_t x,int32_t k,int32_t *s1,int32_t *s2,int high)
{
    int64_t v3=(int64_t)x-*s2,old=*s1;
    int64_t v1=floorq((__int128)c->a1 * *s1+(__int128)c->a2*v3,13);
    int64_t v2=*s2+floorq((__int128)c->a2 * *s1+(__int128)c->a3*v3,13);
    int64_t n1=2*v1-*s1,n2=2*v2-*s2;
    *s1=(int32_t)(n1< -150000?-150000:n1>150000?150000:n1);
    *s2=(int32_t)(n2< -150000?-150000:n2>150000?150000:n2);
    int64_t bp=floorq((int64_t)*s1+old,1),hp=x-floorq((int64_t)k*bp,12)-v2;
    return (int32_t)(high?hp:v2);
}
static int filter_bounds(void)
{
    int64_t max1=0,max2=0,maxlp=0,maxhp=0;
    for(int cut=-50;cut<=50;cut++)if(cut)for(int res=0;res<=127;res++) {
        int q=cut<0?(40<<8)+(-cut)*(60<<8)/50:(46<<8)+(50-cut)*(54<<8)/50,k=8192-res*7600/127;tsvf_t c;tsvf_coef(&c,q,res);
        int64_t t1=(int64_t)c.a1*150000+(int64_t)c.a2*215535,t2=(int64_t)c.a2*150000+(int64_t)c.a3*215535;
        REQUIRE(t1<=INT32_MAX && t2<=INT32_MAX);if(t1>max1)max1=t1;if(t2>max2)max2=t2;
        int64_t boundlp=150000+floorq(t2,13)+1,boundv1=floorq(t1,13)+1;
        REQUIRE(2*boundv1+150000<=INT32_MAX && 2*boundlp+150000<=INT32_MAX);
        REQUIRE((int64_t)k*150000<=INT32_MAX && 65535+(int64_t)k*150000/4096+1+boundlp<=INT32_MAX);
        for(unsigned mask=0;mask<8;mask++) {
            int32_t x=(mask&1)?65535:-65535,s1=(mask&2)?150000:-150000,s2=(mask&4)?150000:-150000,a=s1,b=s2;
            int32_t lp=tsvf_lp(&c,x,&s1,&s2),expected=ref_filter(&c,x,k,&a,&b,0);
            REQUIRE(lp==expected && s1==a && s2==b && abs(s1)<=150000 && abs(s2)<=150000);
            int32_t bp=(s1+((mask&2)?150000:-150000))>>1,hp=x-((k*bp)>>12)-lp;
            if(llabs(lp)>maxlp)maxlp=llabs(lp);if(llabs(hp)>maxhp)maxhp=llabs(hp);
        }
    }
    printf("SVF legal-state bounds: product-sums=%lld/%lld, corner LP/HP peaks=%lld/%lld\n",(long long)max1,(long long)max2,(long long)maxlp,(long long)maxhp);
    return 0;
}
static int render(void)
{
    ui_power_on();track_t *t=&trk[0];host_preset(t,ENGI_GRAIN,0);t->p[P_E0]=3;t->p[P_E7]=127;t->p[P_GB]=127;
    gr_part_t *p=gr_of(t);memset(p,0,sizeof *p);gr_reset(p,3,gr_stamp(3));voice_t *v=&t->v[0];v->active=1;v->note=60;grain_note_on(t,v);
    const int ramps[][2]={{0,0},{32767,32767},{0,65527},{65527,0},{65527,65527},{0,32767},{32767,0}};
    const int states[][2]={{0,0},{150000,150000},{150000,-150000},{-150000,150000},{-150000,-150000}};
    unsigned cases=0;
    for(unsigned type=0;type<4;type++)for(int cut=-50;cut<=50;cut++)for(unsigned r=0;r<2;r++)for(unsigned sign=0;sign<2;sign++)for(unsigned st=0;st<NELEM(states);st++)for(unsigned ramp=0;ramp<NELEM(ramps);ramp++) {
        unsigned count=(unsigned[]){0,1,17,CTL,CTL+7}[cases%5],n=count>CTL?CTL:count;
        t->p[P_GTYPE]=type;t->p[P_GCUT]=cut;t->p[P_GRES]=r?127:0;
        v->s[0]=0;v->s[1]=100000;v->s[7]=-1;v->s[2]=321;v->s[3]=states[st][0];v->s[4]=states[st][1];v->s[6]=GR_TONE_UNITY;
        for(unsigned j=0;j<GR_NG;j++) {
            gr_grain_t *g=&p->g[j];memset(g,0,sizeof *g);g->owner=1;g->z=gr_zone(3,0);g->left=CTL*2;
            g->a=g->b=sign?30000:-30000;g->step=256;g->wph=128u<<24;g->gain=32767;
        }
        vmod_t m={0};m.pitch16=960;m.shape=64<<8;m.amp0=ramps[ramp][0];m.amp1=ramps[ramp][1];
        int32_t out[CTL+4],expected[CTL+4];int32_t initial=cases%3==0?INT32_MAX-123:cases%3==1?INT32_MIN+123:1000;
        for(unsigned i=0;i<NELEM(out);i++)out[i]=expected[i]=initial;
        int32_t a=states[st][0],b=states[st][1],y=321;tsvf_t c;
        if(cut)tsvf_coef(&c,cut<0?(40<<8)+(-cut)*(60<<8)/50:(46<<8)+(50-cut)*(54<<8)/50,r?127:0);
        for(unsigned i=0;i<n;i++) {
            y=cut?ref_filter(&c,sign?65535:-65535,8192-(r?127:0)*7600/127,&a,&b,cut<0):(sign?65535:-65535);
            expected[i+1]=ref_output(initial,y,&m,i);
        }
        grain_render(t,v,out+1,count,&m);REQUIRE(!memcmp(out,expected,sizeof out));
        REQUIRE(v->s[3]==(cut?a:0) && v->s[4]==(cut?b:0) && v->s[2]==(cut?y:0));
        cases++;
    }
    /* Carry real filter state through every live cutoff/resonance transition. */
    v->s[3]=v->s[4]=0;
    for(unsigned b=0;b<4096;b++) {
        int cut=(int)(b%101)-50,res=(b&1)?127:0;t->p[P_GCUT]=cut;t->p[P_GRES]=res;
        v->s[1]=100000;for(unsigned g=0;g<GR_NG;g++){p->g[g].owner=1;p->g[g].left=CTL*2;p->g[g].a=p->g[g].b=b&1?30000:-30000;p->g[g].frac=0;}
        int32_t a=v->s[3],d=v->s[4],y=v->s[2],out[CTL]={0},expected[CTL];tsvf_t c;vmod_t m={0};m.amp0=65527;m.amp1=0;
        /* The transition now has an explicit dry/wet weight. Check the
         * new ramp against the existing wide SVF/amplitude reference. */
        uint32_t state=(uint32_t)v->s[7],wet=state&511u;
        int running=(int)(state>>9)-50;
        if(v->s[7]<0 || wet>256 || running< -50 || running>50 || !running){running=cut;wet=v->s[7]<0&&cut?256:0;}
        if(!wet){a=d=0;running=cut;}
        int toward=cut && ((cut<0)==(running<0));if(toward)running=cut;
        if(running)tsvf_coef(&c,running<0?(40<<8)+(-running)*(60<<8)/50:(46<<8)+(50-running)*(54<<8)/50,res);
        for(unsigned i=0;i<CTL;i++){
            int32_t x=b&1?65535:-65535;
            y=running?ref_filter(&c,x,8192-res*7600/127,&a,&d,running<0):x;
            int32_t mixed=(int32_t)((__int128)x+(((__int128)y-x)*wet)/256);
            expected[i]=ref_output(0,mixed,&m,i);
            if(toward){if(wet<256)wet++;}else if(wet)wet--;
        }
        grain_render(t,v,out,CTL,&m);REQUIRE(!memcmp(out,expected,sizeof out));REQUIRE(abs(v->s[3])<=150000 && abs(v->s[4])<=150000);
    }
    printf("GRAIN actual render reference cases=%u; live transitions=4096\n",cases);
    return 0;
}
int main(void)
{
    if(gain() || filter_bounds() || render())return 1;
    printf("GRAIN headroom: %llu checks passed; old-narrow-in-range=%llu old-defined=%llu old-negative-shift=%llu old-overflow=%llu saturated=%llu\n",(unsigned long long)checks,(unsigned long long)old_in_range,(unsigned long long)old_defined,(unsigned long long)old_negative_shift,(unsigned long long)old_overflow,(unsigned long long)saturated);
    return 0;
}
