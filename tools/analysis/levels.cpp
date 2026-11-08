// How much of a large island is owed to each octave level?
//   levels THREADS K < "seed x z" lines
// For L = 1..6: keep continentalness octaves 0..L-1 of both sides as they are and replace octaves L..8 of both
// sides by K random realisations (fresh permutations and offsets; y-phases taken from random seeds that pass
// COMMISSION's record phase gate, so their joint distribution is the real one). Measures the island each time
// (8-block grid, +-5600 blocks, largest 4-connected region of C < -1.05 meeting the original island).
// Output per island and L: mean and sd of the area, and the share of realisations reaching 9M / 10M / 11M.
#include "noise.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
using namespace standalone;

constexpr int R = 700, W = 2 * R + 1, G = 2;

static long flood_largest(const std::vector<float> &f, const std::vector<uint8_t> &orig, std::vector<int> &lab, std::vector<int> &stack) {
    std::fill(lab.begin(), lab.end(), 0); int nl = 0; long best = 0;
    for (size_t s = 0; s < f.size(); s++) {
        if (!orig[s] || lab[s] || !(f[s] < -1.05f)) continue;
        lab[s] = ++nl; stack.push_back(int(s)); long n = 0;
        while (!stack.empty()) {
            int c = stack.back(); stack.pop_back(); n++;
            int i = c % W, j = c / W;
            const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int ii = i + di[k], jj = j + dj[k];
                if (ii < 0 || jj < 0 || ii >= W || jj >= W) continue;
                size_t d = size_t(jj) * W + ii;
                if (!lab[d] && f[d] < -1.05f) { lab[d] = nl; stack.push_back(int(d)); }
            }
        }
        best = std::max(best, n);
    }
    return best;
}

// y-phase fractions of all 9 octaves, both sides, for random seeds passing the record phase gate
static std::vector<std::array<double, 18>> gate_phases(int n) {
    std::vector<std::array<double, 18>> out;
    for (uint64_t k = 0; int(out.size()) < n; k++) {
        uint64_t seed = mix(k * 0x9e3779b97f4a7c15ULL + 4242);
        Random world(seed); uint64_t a = world.next(), b = world.next();
        const auto &cf = configs[Continentalness]; Random c(a ^ cf.a, b ^ cf.b);
        std::array<double, 18> fy; double sc = 0;
        for (int side = 0; side < 2; side++) {
            uint64_t fa = c.next(), fb = c.next();
            for (int o = 0; o < 9; o++) {
                const auto &salt = octave_salt[cf.first + o + 10];
                Random r(fa ^ salt[0], fb ^ salt[1]); r.next();
                double y = r.uniform() * 256.; fy[side * 9 + o] = y - std::floor(y);
            }
            const double w[3] = {350, 55, 4};
            for (int o = 0; o < 3; o++) { double d = std::abs(fy[side * 9 + o] - .5); sc += w[o] * d * d; }
        }
        if (sc < 1.5) out.push_back(fy);
    }
    return out;
}

int main(int argc, char **argv) {
    int threads = argc > 1 ? std::stoi(argv[1]) : 8, K = argc > 2 ? std::stoi(argv[2]) : 16;
    auto phases = gate_phases(4096);
    std::vector<std::pair<uint64_t, std::pair<int, int>>> in;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; int x, z;
        if (is >> sd >> x >> z) in.push_back({uint64_t(sd), {x, z}});
    }
    printf("seed,L,orig_M,mean_M,sd_M,p9,p10,p11\n");
    std::mutex mu;
    for (auto &isl : in) {
        Sampler s(isl.first);
        const auto &C = s.get(Continentalness), &Sh = s.get(Shift);
        int q0x = isl.second.first >> 2, q0z = isl.second.second >> 2;
        const size_t N = size_t(W) * W;
        std::vector<double> px(N), pz(N);
        std::vector<float> cum[10];                          // cum[L] = sum of octaves 0..L-1, both sides
        for (auto &c : cum) c.assign(N, 0.f);
        {
            std::vector<std::thread> pool; std::atomic<int> row(0);
            for (int t = 0; t < threads; t++) pool.emplace_back([&] {
                for (int j; (j = row++) < W;)
                    for (int i = 0; i < W; i++) {
                        size_t k = size_t(j) * W + i; int qx = q0x + (i - R) * G, qz = q0z + (j - R) * G;
                        px[k] = qx + 4. * Sh.sample(qx, 0, qz); pz[k] = qz + 4. * Sh.sample(qz, qx, 0);
                        double acc = 0;
                        for (int o = 0; o < 9; o++) {
                            for (int side = 0; side < 2; side++) {
                                double sc = side ? 337. / 331. : 1.; const auto &p = C.octave[side][o];
                                acc += p.weight * p.sample(px[k] * sc * p.frequency, 0, pz[k] * sc * p.frequency) * C.factor;
                            }
                            cum[o + 1][k] = float(acc);
                        }
                    }
            });
            for (auto &t : pool) t.join();
        }
        std::vector<uint8_t> all(N, 0);
        for (int j = R - 375; j <= R + 375; j++) for (int i = R - 375; i <= R + 375; i++) all[size_t(j) * W + i] = 1;
        std::vector<int> lab(N), stack;
        long orig_n = flood_largest(cum[9], all, lab, stack);
        std::vector<long> cnt; for (int l : lab) { if (l >= int(cnt.size())) cnt.resize(l + 1); cnt[l]++; }
        int best_l = 0; for (int l = 1; l < int(cnt.size()); l++) if (cnt[l] == orig_n) best_l = l;
        std::vector<uint8_t> orig(N); for (size_t k = 0; k < N; k++) orig[k] = lab[k] == best_l;
        const double cell = G * G * 16 / 1e6;
        struct Job { int L, k; };
        std::vector<Job> jobs;
        for (int L = 1; L <= 6; L++) for (int k = 0; k < K; k++) jobs.push_back({L, k});
        std::vector<double> area(jobs.size());
        std::atomic<size_t> next(0);
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; t++) pool.emplace_back([&] {
            std::vector<float> f(N); std::vector<int> lab2(N), st2;
            for (size_t ji; (ji = next++) < jobs.size();) {
                auto [L, k] = jobs[ji];
                Random r(isl.first * 1000003 + L * 7919 + k * 104729);
                const auto &ph = phases[r.bounded(uint32_t(phases.size()))];
                Perlin p[2][9];
                for (int side = 0; side < 2; side++) for (int o = L; o < 9; o++) {
                    const auto &p0 = C.octave[side][o];
                    p[side][o].init(Random(r.next(), r.next()), p0.frequency, p0.weight);
                    double fy = ph[side * 9 + o]; p[side][o].oy = p[side][o].iy + fy; p[side][o].fy = fy; p[side][o].vy = fade(fy);
                }
                for (size_t c = 0; c < N; c++) {
                    double v = cum[L][c];
                    for (int side = 0; side < 2; side++) {
                        double sc = side ? 337. / 331. : 1.;
                        for (int o = L; o < 9; o++) { const auto &q = p[side][o]; v += q.weight * q.sample(px[c] * sc * q.frequency, 0, pz[c] * sc * q.frequency) * C.factor; }
                    }
                    f[c] = float(v);
                }
                area[ji] = flood_largest(f, orig, lab2, st2) * cell;
            }
        });
        for (auto &t : pool) t.join();
        for (int L = 1; L <= 6; L++) {
            double s1 = 0, s2 = 0; int p9 = 0, p10 = 0, p11 = 0;
            for (size_t j = 0; j < jobs.size(); j++) if (jobs[j].L == L) { double a = area[j]; s1 += a; s2 += a * a; p9 += a >= 9; p10 += a >= 10; p11 += a >= 11; }
            double m = s1 / K;
            printf("%lld,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", (long long)isl.first, L, orig_n * cell, m, std::sqrt(std::max(0.0, s2 / K - m * m)),
                   double(p9) / K, double(p10) / K, double(p11) / K);
        }
        fflush(stdout);
    }
}
