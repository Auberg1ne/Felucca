/* SPDX-License-Identifier: GPL-3.0-only */
/* Isolated qualification model, never included by firmware. Requires the
 * unchanged mono helpers from ui_test.c. Controls advance once, signal state
 * advances independently in reconstructed L/R, before any re-encoding. */
typedef struct { int32_t tone_y, last, ic1, ic2; } q_lr_lane_t;
typedef struct {
    q_lr_lane_t lane[2];
    int32_t tone_a, filter;
} q_lr_state_t;
_Static_assert(sizeof(q_lr_state_t)==40, "two explicit lanes plus shared controls");
static void q_lr_block(q_lr_state_t *s, const int32_t in[2][CTL], int32_t out[2][CTL],
                       uint32_t n, const vmod_t *m, int32_t tone_target,
                       int32_t cutoff, int32_t resonance)
{
    if (!n || n>CTL) return;
    int32_t signal[2][CTL];
    int bypass=s->tone_a==GR_TONE_UNITY&&tone_target==GR_TONE_UNITY;
    for(uint32_t i=0;i<n;i++) {
        if(!bypass) {
            int32_t d=tone_target-s->tone_a;
            if(d){int32_t step=(d<0?-d:d)>>8;if(!step)step=1;s->tone_a+=d<0?-step:step;}
        }
        for(uint32_t ch=0;ch<2;ch++) {
            int32_t x=clamp(in[ch][i],-65535,65535);
            if(bypass)s->lane[ch].tone_y=x;
            else s->lane[ch].tone_y+=((x-s->lane[ch].tone_y)*s->tone_a)/GR_TONE_UNITY;
            signal[ch][i]=s->tone_a==GR_TONE_UNITY?in[ch][i]:s->lane[ch].tone_y;
        }
    }
    uint32_t state=(uint32_t)s->filter,wet=state&511u;
    int32_t running=(int32_t)(state>>9)-50;
    if(s->filter<0||wet>GR_FILTER_FADE||running < -50||running>50||!running) {
        running=cutoff;wet=s->filter<0&&cutoff?GR_FILTER_FADE:0;
    }
    if(!wet){for(uint32_t ch=0;ch<2;ch++)s->lane[ch].last=s->lane[ch].ic1=s->lane[ch].ic2=0;running=cutoff;}
    if(!running){
        for(uint32_t i=0;i<n;i++)for(uint32_t ch=0;ch<2;ch++)out[ch][i]=gr_output(out[ch][i],clamp(signal[ch][i],-65535,65535),m,i);
        s->filter=0;return;
    }
    int toward=cutoff&&((cutoff<0)==(running<0));if(toward)running=cutoff;
    int32_t cut=running<0?(46<<8)+((50+running)*(54<<8)/50):(40<<8)+(running*(60<<8)/50);
    int32_t k=8192-resonance*7600/127;tsvf_t fc;tsvf_coef(&fc,cut,resonance);
    for(uint32_t i=0;i<n;i++) {
        for(uint32_t ch=0;ch<2;ch++) {
            q_lr_lane_t *v=&s->lane[ch];int32_t x=clamp(signal[ch][i],-65535,65535),old=v->ic1;
            int32_t lp=tsvf_lp(&fc,x,&v->ic1,&v->ic2),bp=(v->ic1+old)>>1;
            int32_t hp=x-((k*bp)>>12)-lp;v->last=running<0?lp:hp;
            int32_t mixed=wet==GR_FILTER_FADE?v->last:x+(int32_t)(((int64_t)v->last-x)*wet/GR_FILTER_FADE);
            out[ch][i]=gr_output(out[ch][i],mixed,m,i);
        }
        if(toward){if(wet<GR_FILTER_FADE)wet++;}else if(wet)wet--;
    }
    if(!wet)for(uint32_t ch=0;ch<2;ch++)s->lane[ch].last=s->lane[ch].ic1=s->lane[ch].ic2=0;
    s->filter=(int32_t)(wet|((uint32_t)(running+50)<<9));
}
