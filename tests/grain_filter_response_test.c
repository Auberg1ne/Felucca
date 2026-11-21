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

int main(void)
{
    puts("cutoff | 100 Hz | 1000 Hz | 5000 Hz");
    for (int cut = -50; cut <= 50; cut += 10)
        printf("%+6d | %6.4f | %7.4f | %7.4f\n",
               cut, response(cut, 0, 100),
               response(cut, 0, 1000), response(cut, 0, 5000));

    double hp25bass = response(25, 0, 100);
    double hp50bass = response(50, 0, 100);
    double hp25treble = response(25, 0, 5000);
    double lp25treble = response(-25, 0, 5000);
    double lp50treble = response(-50, 0, 5000);
    double lp25bass = response(-25, 0, 100);

    /* Only output polarity reverses: the original frequency curves stay.
     * Thus increasing magnitude opens these new LP/HP selections. */
    if (!(hp50bass > hp25bass && hp25bass < 0.5 &&
          hp25treble > hp25bass &&
          lp50treble > lp25treble && lp25treble < 0.5 &&
          lp25bass > lp25treble)) {
        fprintf(stderr, "FAIL: negative LP / positive HP with unchanged frequency curves\n");
        return 1;
    }

    for (int res = 0; res <= 127; res += 127) {
        if (fabs(response(0, res, 100) - 1.0) > 0.000001 ||
            fabs(response(0, res, 5000) - 1.0) > 0.000001) {
            fprintf(stderr, "FAIL: zero-cutoff bypass\n");
            return 1;
        }
    }

    puts("PASS: bipolar frequency response and exact bypass");
    return 0;
}
