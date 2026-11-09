// Search over gradvecs_1 gates of the form  w1 * term_i + w2 * term_j >= c  (any two of the 8 lattice terms, which
// all have the scan's row-list-and-lag structure). For random permutations, histograms per gate of: all positions,
// and the island weight exp(sum beta_k * term_k) of positions whose 8-term score passes the final cut.
//   gatesearch THREADS PERMS FINAL > table      (one line per gate: i j w1 w2 then per threshold bin)
#include "../../src/kernel_0A.h"
#include <algorithm>
#include <array>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
constexpr int NB = 800; constexpr float LO = -40.f, BW = 0.05f;
struct Gate { int i, j; float w1, w2; };
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long perms = atol(argv[2]); float FIN = atof(argv[3]);
    const float beta[8] = {1.46f, 1.19f, 1.19f, 1.46f, 1.53f, 1.23f, 1.23f, 1.53f};   // fitted per term group
    std::vector<Gate> gates;
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++)
        for (float w : {0.5f, 1.0f, 2.0f}) gates.push_back({i, j, 1.0f, w});
    const int G = gates.size();
    std::vector<double> cnt(G * NB, 0), mass(G * NB, 0), maxsum(G, 0);
    std::mutex mu;
    std::vector<std::thread> ts;
    for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
        std::mt19937_64 rng(555 + th);
        std::vector<double> c(G * NB, 0), m(G * NB, 0);
        for (long it = th; it < perms; it += threads) {
            uint8_t p[256]; for (int i = 0; i < 256; i++) p[i] = i;
            for (int i = 0; i < 256; i++) { int j = i + rng() % (256 - i); std::swap(p[i], p[j]); }
            int ny = rng() % 256;
            float conv[2][256][4];
            for (int y = 0; y < 2; y++) for (int r = 0; r < 256; r++) for (int x = 1; x < 5; x++) {
                float s = 0; for (int dz = 0; dz < 6; dz++) s += host_kernel_0A[y][x][dz][p[(r + dz) & 255] & 15]; conv[y][r][x - 1] = s; }
            int idx[2][262];
            for (int n = 0; n < 262; n++) for (int y = 0; y < 2; y++) idx[y][n] = p[(p[n & 255] + ny + y) & 255];
            for (int nx = 0; nx < 256; nx++) for (int nz = 0; nz < 256; nz++) {
                float t[8];
                for (int y = 0; y < 2; y++) for (int i = 1; i <= 4; i++) t[y * 4 + i - 1] = conv[y][(idx[y][nx + i] + nz) & 255][i - 1];
                float S8 = (t[1] + t[2]) + t[0] + t[3] + t[4] + t[5] + t[6] + t[7];
                double wgt = 0;
                if (S8 > FIN) { float e = 0; for (int k = 0; k < 8; k++) e += beta[k] * t[k]; wgt = std::exp((double)e + 20.0); }
                for (int g = 0; g < G; g++) {
                    float v = gates[g].w1 * t[gates[g].i] + gates[g].w2 * t[gates[g].j];
                    int b = (int)((v - LO) / BW); if (b < 0) b = 0; if (b >= NB) b = NB - 1;
                    c[g * NB + b] += 1; if (wgt > 0) m[g * NB + b] += wgt;
                }
            }
        }
        std::lock_guard l(mu);
        for (int k = 0; k < G * NB; k++) { cnt[k] += c[k]; mass[k] += m[k]; }
    });
    for (auto &t : ts) t.join();
    for (int g = 0; g < G; g++) {
        printf("%d %d %.2f %.2f", gates[g].i, gates[g].j, gates[g].w1, gates[g].w2);
        for (int b = 0; b < NB; b++) printf(" %.0f:%.6g", cnt[g * NB + b], mass[g * NB + b]);
        printf("\n");
    }
    fprintf(stderr, "perms %ld\n", perms);
}
