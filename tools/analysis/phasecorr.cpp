// Are the y-phases of continentalness octaves 3..8 independent of those of octaves 0..2?
// Draw random seeds, keep those passing COMMISSION's record-preset phase gate
// (350 d0^2 + 55 d1^2 + 4 d2^2 summed over both sides < 1.5), and histogram d_o for every octave.
//   phasecorr N
#include "noise.hpp"
#include <cstdio>
#include <string>
using namespace standalone;

static void phases(uint64_t seed, double d[2][9]) {
    Random world(seed); uint64_t a = world.next(), b = world.next();
    const auto &cf = configs[Continentalness]; Random c(a ^ cf.a, b ^ cf.b);
    for (int side = 0; side < 2; side++) {
        uint64_t fa = c.next(), fb = c.next();
        for (int o = 0; o < 9; o++) {
            const auto &salt = octave_salt[cf.first + o + 10];
            Random r(fa ^ salt[0], fb ^ salt[1]); r.next();
            double y = r.uniform() * 256.; d[side][o] = std::abs(y - std::floor(y) - .5);
        }
    }
}
int main(int argc, char **argv) {
    uint64_t N = std::stoull(argv[1]);
    long pass = 0; double sum[9] = {}; long lo[9] = {}, hi[9] = {};
    long all_lo[9] = {}, all_hi[9] = {}; double all_sum[9] = {}; long all = 0;
    for (uint64_t k = 0; k < N; k++) {
        uint64_t seed = mix(k * 0x9e3779b97f4a7c15ULL + 12345);
        double d[2][9]; phases(seed, d);
        for (int s = 0; s < 2; s++) for (int o = 0; o < 9; o++) { all_sum[o] += d[s][o]; all_lo[o] += d[s][o] < .1; all_hi[o] += d[s][o] > .4; }
        all += 2;
        double sc = 0;
        for (int s = 0; s < 2; s++) sc += 350 * d[s][0] * d[s][0] + 55 * d[s][1] * d[s][1] + 4 * d[s][2] * d[s][2];
        if (sc >= 1.5) continue;
        pass++;
        for (int s = 0; s < 2; s++) for (int o = 0; o < 9; o++) { sum[o] += d[s][o]; lo[o] += d[s][o] < .1; hi[o] += d[s][o] > .4; }
    }
    printf("%llu seeds, %ld pass (1 in %.0f)\n", (unsigned long long)N, pass, double(N) / pass);
    printf("oct  all: mean d  d<.1  d>.4 | gate-passing: mean d  d<.1  d>.4\n");
    for (int o = 0; o < 9; o++)
        printf("%d    %.3f  %.2f  %.2f | %.3f  %.2f  %.2f\n", o, all_sum[o] / all, double(all_lo[o]) / all, double(all_hi[o]) / all,
               sum[o] / (2 * pass), double(lo[o]) / (2 * pass), double(hi[o]) / (2 * pass));
}
