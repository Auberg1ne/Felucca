/* SPDX-License-Identifier: GPL-3.0-only */
/* Default-off integration: original mono is lane2, independent L/R lanes0/1.
 * One physical control progression for all three. No decoder/scheduler here. */
static uint32_t trk_vmode(const track_t *t);
typedef struct { gr_lr_lane_t lane[3]; int32_t tone_a,filter; } gr_lr_filter_t;
static void gr_lr_filter(gr_lr_filter_t *s, int32_t signal[3][CTL], int32_t out[3][CTL],
                       uint32_t n, const vmod_t *m, int32_t tone_target,
                       int32_t cutoff, int32_t resonance, uint32_t lanes)
{
    if (!n || n>CTL || !lanes || lanes>3) return;
    int bypass=s->tone_a==GR_TONE_UNITY&&tone_target==GR_TONE_UNITY;
    for(uint32_t i=0;i<n;i++) {
        if(!bypass) {
            int32_t d=tone_target-s->tone_a;
            if(d){int32_t step=(d<0?-d:d)>>8;if(!step)step=1;s->tone_a+=d<0?-step:step;}
        }
        for(uint32_t ch=0;ch<lanes;ch++) {
            int32_t x=clamp(signal[ch][i],-65535,65535);
            if(bypass)s->lane[ch].tone_y=x;
            else s->lane[ch].tone_y+=((x-s->lane[ch].tone_y)*s->tone_a)/GR_TONE_UNITY;
            signal[ch][i]=s->tone_a==GR_TONE_UNITY?signal[ch][i]:s->lane[ch].tone_y;
        }
    }
    uint32_t state=(uint32_t)s->filter,wet=state&511u;
    int32_t running=(int32_t)(state>>9)-50;
    if(s->filter<0||wet>GR_FILTER_FADE||running < -50||running>50||!running) {
        running=cutoff;wet=s->filter<0&&cutoff?GR_FILTER_FADE:0;
    }
    if(!wet){for(uint32_t ch=0;ch<lanes;ch++)s->lane[ch].last=s->lane[ch].ic1=s->lane[ch].ic2=0;running=cutoff;}
    if(!running){
        for(uint32_t i=0;i<n;i++)for(uint32_t ch=0;ch<lanes;ch++)out[ch][i]=gr_output(out[ch][i],clamp(signal[ch][i],-65535,65535),m,i);
        s->filter=0;return;
    }
    int toward=cutoff&&((cutoff<0)==(running<0));if(toward)running=cutoff;
    int32_t cut=running<0?(46<<8)+((50+running)*(54<<8)/50):(40<<8)+(running*(60<<8)/50);
    int32_t k=8192-resonance*7600/127;tsvf_t fc;tsvf_coef(&fc,cut,resonance);
    for(uint32_t i=0;i<n;i++) {
        for(uint32_t ch=0;ch<lanes;ch++) {
            gr_lr_lane_t *v=&s->lane[ch];int32_t x=clamp(signal[ch][i],-65535,65535),old=v->ic1;
            int32_t lp=tsvf_lp(&fc,x,&v->ic1,&v->ic2),bp=(v->ic1+old)>>1;
            int32_t hp=x-((k*bp)>>12)-lp;v->last=running<0?lp:hp;
            int32_t mixed=wet==GR_FILTER_FADE?v->last:x+(int32_t)(((int64_t)v->last-x)*wet/GR_FILTER_FADE);
            out[ch][i]=gr_output(out[ch][i],mixed,m,i);
        }
        if(toward){if(wet<GR_FILTER_FADE)wet++;}else if(wet)wet--;
    }
    if(!wet)for(uint32_t ch=0;ch<lanes;ch++)s->lane[ch].last=s->lane[ch].ic1=s->lane[ch].ic2=0;
    s->filter=(int32_t)(wet|((uint32_t)(running+50)<<9));
}

static uint32_t gr_lr_begin(track_t *t,voice_t *v,uint32_t vi)
{
    if(vi>=GR_POLY) return 0;
    gr_part_t *P=gr_of(t);gr_lr_voice_t *s=&P->lr[vi];
    uint32_t want=(uint32_t)t->p[P_GTYPE]==0u && gr_test_spread[gr_part(t)]>0u && gr_test_spread[gr_part(t)]<=100u;
    if(!s->on && want) {
        gr_lr_lane_t lane={v->s[5],v->s[2],v->s[3],v->s[4]};
        s->lane[0]=s->lane[1]=lane;s->mix=256;s->on=1;
    }
    s->reserved=0;
    if(s->on)for(uint32_t g=0;g<GR_NG;g++)s->reserved|=P->g[g].owner==vi+1u && P->pan[g]!=0;
    return s->on;
}
static int gr_run_lr(gr_part_t *P,gr_grain_t *g,int32_t *mono,uint32_t n)
{
    int32_t sample[CTL]={0};int pan=clamp(P->pan[g-P->g],-64,64);
    int alive=gr_run(P,g,sample,n);
    for(uint32_t i=0;i<n;i++){
        int x=sample[i],q=x*(pan<0?-pan:pan),a=q<0?-((-q)>>6):q>>6;
        mono[i]+=x;P->lr_work[0][i]+=x-(pan>0?a:0);P->lr_work[1][i]+=x-(pan<0?a:0);
    }
    return alive;
}
static int32_t gr_lr_add(int32_t old,int64_t value)
{
    int64_t sum=(int64_t)old+value;
    return sum>INT32_MAX?INT32_MAX:sum<INT32_MIN?INT32_MIN:(int32_t)sum;
}
static void gr_lr_finish(track_t *t,voice_t *v,int32_t *out,const int32_t *mono,uint32_t n,const vmod_t *m,int cutoff)
{
    gr_part_t *P=gr_of(t);P->lr_block=1;uint32_t vi=(uint32_t)(v-t->v);gr_lr_voice_t *s=&P->lr[vi];
    gr_lr_filter_t state={{s->lane[0],s->lane[1],{v->s[5],v->s[2],v->s[3],v->s[4]}},v->s[6],v->s[7]};
    for(uint32_t i=0;i<n;i++)P->lr_work[2][i]=mono[i];
    memset(P->lr_out,0,sizeof P->lr_out);
    gr_lr_filter(&state,P->lr_work,P->lr_out,n,m,gr_tone_coef(t->p[P_E7]),cutoff,t->p[P_GRES],3);
    s->lane[0]=state.lane[0];s->lane[1]=state.lane[1];
    v->s[5]=state.lane[2].tone_y;v->s[2]=state.lane[2].last;v->s[3]=state.lane[2].ic1;v->s[4]=state.lane[2].ic2;
    v->s[6]=state.tone_a;v->s[7]=state.filter;
    uint32_t want=(uint32_t)t->p[P_GTYPE]==0u && gr_test_spread[gr_part(t)]>0u && gr_test_spread[gr_part(t)]<=100u,placed=s->reserved;
    for(uint32_t g=0;g<GR_NG;g++)placed|=P->g[g].owner==vi+1u && P->pan[g]!=0;
    /* Existing V-SPRD uses side0 voices for its first group. */
    uint32_t group=t->p[P_SPRD]>0 && (trk_vmode(t)==V_POLY||trk_vmode(t)==V_UNISON) && !v->side?0u:1u;
    for(uint32_t i=0;i<n;i++) {
        int64_t mid=((int64_t)P->lr_out[0][i]+P->lr_out[1][i])/2;
        int64_t side=((int64_t)P->lr_out[0][i]-P->lr_out[1][i])/2;
        int64_t shadow=P->lr_out[2][i];
        out[i]=gr_lr_add(out[i],shadow+(mid-shadow)*s->mix/256);
        P->stereo_side[group][i]=gr_lr_add(P->stereo_side[group][i],side*s->mix/256);
        if(want){if(s->mix<256)s->mix++;}
        else if(!placed && s->mix)s->mix--;
    }
    if(!want&&!placed&&!s->mix) memset(s,0,sizeof *s);
}

/* Test control reset only; audio-owned histories are drained/retired by their
 * render core. Called at the existing track/default/new-project boundary. */
static void gr_lr_test_reset(const track_t *t)
{
    uintptr_t address=(uintptr_t)t,base=(uintptr_t)trk;
    if(address>=base && address<base+sizeof trk && (address-base)%sizeof(track_t)==0)
        gr_test_spread[(address-base)/sizeof(track_t)]=0;
}
