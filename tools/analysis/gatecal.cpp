// Calibrates a phase gate on real seeds (the six offsets of a seed are correlated, so independent draws mislead):
// pass rate of upstream main's linear gate, the fitted quadratic threshold with the same pass rate, and the share
// of islands each keeps under the fitted island model exp(-257.3 d0^2 - 47.4 d1^2 - 7.5 d2^2).
//   gatecal THREADS N
#include "cpipe.hpp"
#include <cmath>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
using namespace cp;
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long n = atol(argv[2]);
    const int NT = 401; const double t0 = 0.8, dt = 0.004;              // quadratic thresholds 0.8 .. 2.4
    std::vector<double> qpass(NT), qw(NT); double lpass = 0, lw = 0, wall = 0; std::mutex mu;
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; t++) ts.emplace_back([&, t] {
        std::mt19937_64 rng(98765 + t * 7919);
        std::vector<double> qp(NT), qww(NT); double lp = 0, lww = 0, wa = 0;
        for (long i = t; i < n; i += threads) {
            Seed s{rng()}; float d[6];
            for (int side = 0; side < 2; side++)
                for (int o = 0; o < 3; o++) {
                    const auto &salt = standalone::octave_salt[-9 + o + 10];
                    Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
                    float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
                    d[o * 2 + side] = f - 0.5f;
                }
            float lin = 0.35f * std::fabs(d[0]); lin += 0.35f * std::fabs(d[1]); lin += 0.11f * std::fabs(d[2]);
            lin += 0.11f * std::fabs(d[3]); lin += 0.04f * std::fabs(d[4]); lin += 0.04f * std::fabs(d[5]);
            float q = 257.3f * d[0] * d[0]; q += 257.3f * d[1] * d[1]; q += 47.4f * d[2] * d[2];
            q += 47.4f * d[3] * d[3]; q += 7.5f * d[4] * d[4]; q += 7.5f * d[5] * d[5];
            double w = std::exp(-(double)q); wa += w;
            if (lin < 0.038f) { lp++; lww += w; }
            int k = (int)std::ceil((q - t0) / dt); if (k < 0) k = 0;          // q < t0 + j*dt for all j >= k
            if (k < NT) { qp[k]++; qww[k] += w; }
        }
        std::lock_guard l(mu); lpass += lp; lw += lww; wall += wa;
        for (int j = 0; j < NT; j++) { qpass[j] += qp[j]; qw[j] += qww[j]; }
    });
    for (auto &t : ts) t.join();
    for (int j = 1; j < NT; j++) { qpass[j] += qpass[j - 1]; qw[j] += qw[j - 1]; }
    printf("linear 0.35/0.11/0.04 < 0.038: passes 1 in %.0f, keeps %.4f of islands\n", n / lpass, lw / wall);
    for (double a : {0.25, 0.31, 0.37}) { double bestr = 0, bt = 0; for (int j = 0; j < NT; j++) { double F = qpass[j] / lpass, r = (qw[j] / lw) / (a + (1 - a) * F); if (r > bestr) { bestr = r; bt = t0 + j * dt; } } printf("fixed share %.2f: best quadratic threshold %.3f, islands per time x%.3f vs linear\n", a, bt, bestr); }
    int best = 0; for (int j = 0; j < NT; j++) if (std::fabs(qpass[j] - lpass) < std::fabs(qpass[best] - lpass)) best = j;
    for (int j = best - 3; j <= best + 3; j++)
        printf("quadratic < %.3f: passes 1 in %.0f, keeps %.4f of islands, islands vs linear x%.3f\n", t0 + j * dt, n / qpass[j], qw[j] / wall, qw[j] / lw);
}
