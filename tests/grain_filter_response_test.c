/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent behavioral checks of the GRAIN bipolar filter. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static double response(int cut, int res, double hz)
{
    tsvf_t c;
    int32_t ic1 = 0, ic2 = 0;
    double input_energy = 0.0, output_energy = 0.0;
    const unsigned count = 44100;

    if (cut) {
        int q = cut < 0
            ? 40 * 256 + (-cut) * (60 * 256) / 50
            : 46 * 256 + (50 - cut) * (54 * 256) / 50;
        tsvf_coef(&c, q, res);
    }

    for (unsigned i = 0; i < count; i++) {
        int32_t x = (int32_t)lrint(12000.0 *
                      sin(2.0 * 3.14159265358979323846 * hz * i / FS));
        int32_t y = x;

        if (cut) {
            int32_t old = ic1;
            int32_t lp = tsvf_lp(&c, x, &ic1, &ic2);
            int32_t bp = (ic1 + old) >> 1;
            int32_t k = 8192 - res * 7600 / 127;
            int32_t hp = x - ((k * bp) >> 12) - lp;
            y = cut < 0 ? hp : lp;
        }

        if (i >= count / 2) {
            input_energy += (double)x * x;
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

    double hp25bass = response(-25, 0, 100);
    double hp50bass = response(-50, 0, 100);
    double hp50treble = response(-50, 0, 5000);
    double lp25treble = response(25, 0, 5000);
    double lp50treble = response(50, 0, 5000);
    double lp50bass = response(50, 0, 100);

    printf("HP bass: -25=%.4f -50=%.4f; HP treble -50=%.4f\n",
           hp25bass, hp50bass, hp50treble);
    printf("LP treble: +25=%.4f +50=%.4f; LP bass +50=%.4f\n",
           lp25treble, lp50treble, lp50bass);

    if (!(hp50bass < hp25bass && hp50bass < 0.5 &&
          hp50treble > hp50bass &&
          lp50treble < lp25treble && lp50treble < 0.5 &&
          lp50bass > lp50treble)) {
        fprintf(stderr, "FAIL: bipolar frequency-response expectations\n");
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
