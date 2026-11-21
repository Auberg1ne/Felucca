/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent behavioral checks of the GRAIN bipolar filter. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static double response(int cut, int res, double hz)
{
    voice_t v = {0}; v.s[7] = -1;
    vmod_t m = {0}; m.amp0 = m.amp1 = 32767;
    double input_energy = 0.0, output_energy = 0.0;
    const unsigned count = 44100;

    for (unsigned i = 0; i < count; i++) {
        int32_t x = (int32_t)lrint(12000.0 *
                      sin(2.0 * 3.14159265358979323846 * hz * i / FS));
        int32_t y = 0;
        gr_filter(&v, &x, &y, 1, &m, cut, res); /* actual production selector */
        int32_t dry = gr_output(0, x, &m, 0);

        if (i >= count / 2) {
            input_energy += (double)dry * dry;
            output_energy += (double)y * y;
        }
    }

    return sqrt(output_energy / input_energy);
}

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"response line %u: %s\n",__LINE__,#x); return 1; } } while(0)
/* Independent linear SVF transfer: HP tends to unity at Nyquist; an
 * accidentally cascaded LP would fail the high-frequency checks below. */
static double hp_reference(int cut, int res, double hz)
{
    int q=(40<<8)+cut*(60<<8)/50,i=q>>8;
    int gq=SVF_G[i];if(i<127)gq+=((SVF_G[i+1]-gq)*(q&255))>>8;
    double g=gq/4096.0,w=tan(3.14159265358979323846*hz/FS);
    double k=(8192-res*7600/127)/4096.0;
    return w*w/hypot(g*g-w*w,k*g*w);
}
int main(void)
{
    const int cuts[]={-50,-25,-1,0,1,25,50};
    puts("cutoff | 100 Hz | 1000 Hz | 5000 Hz | 20000 Hz | 21000 Hz");
    for(unsigned i=0;i<sizeof cuts/sizeof cuts[0];i++)
        printf("%+6d | %.5f | %.5f | %.5f | %.5f | %.5f\n",cuts[i],
            response(cuts[i],0,100),response(cuts[i],0,1000),
            response(cuts[i],0,5000),response(cuts[i],0,20000),response(cuts[i],0,21000));
    double lp=2,hp=2;
    for(int mag=1;mag<=50;mag++){
        double treble=response(-mag,0,5000),bass=response(mag,0,100);
        CHECK(treble<=lp+0.001);CHECK(bass<=hp+0.001);lp=treble;hp=bass;
        CHECK(response(-mag,0,100)>response(-mag,0,20000)); /* LP polarity */
        CHECK(response(mag,0,21000)>bass); /* HP polarity */
        for(int res=0;res<=127;res+=127)for(int hz=20000;hz<=21000;hz+=1000){
            double actual=response(mag,res,hz),expected=hp_reference(mag,res,hz);
            CHECK(fabs(actual-expected)<0.025); /* HP alone, no extra LP rolloff */
            CHECK(hz!=21000 || actual>0.90);
        }
    }
    CHECK(response(-1,0,1000)>0.90 && response(1,0,1000)>0.90); /* nearly open */
    CHECK(response(-50,0,1000)<0.10 && response(50,0,100)<0.002); /* closed */
    for(int res=0;res<=127;res+=127)for(int hz=100;hz<=20100;hz+=5000)
        CHECK(response(0,res,hz)==1.0);
    printf("PASS: %u checks; negative LP lowers cutoff, positive HP raises cutoff; HP treble and exact bypass\n",checks);
    return 0;
}
