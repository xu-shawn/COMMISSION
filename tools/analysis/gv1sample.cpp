// gradvecs_1 term groups (P0 = bottom central pair, P1 = top central pair, Ob/Ot = bottom/top outer pairs) of
// every lattice position of random permutations with P0 >= GATE and the 8-term sum > FINAL.
//   gv1sample THREADS PERMS GATE FINAL > sample        (stderr: positions per permutation)
#include "../../src/kernel_0A.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long perms = atol(argv[2]); float G = atof(argv[3]), F = atof(argv[4]);
    std::mutex mu; std::atomic<long> total{0};
    std::vector<std::thread> ts;
    for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
        std::mt19937_64 rng(99991 + th);
        std::vector<std::array<float, 4>> out;
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
                auto C = [&](int y, int i, int col) { return conv[y][(idx[y][nx + i] + nz) & 255][col]; };
                float P0 = C(0, 2, 1) + C(0, 3, 2);
                if (!(P0 >= G)) continue;
                float P1 = C(1, 2, 1) + C(1, 3, 2), Ob = C(0, 1, 0) + C(0, 4, 3), Ot = C(1, 1, 0) + C(1, 4, 3);
                if (P0 + P1 + Ob + Ot > F) out.push_back({P0, P1, Ob, Ot});
            }
        }
        std::lock_guard l(mu);
        total += out.size();
        for (auto &a : out) printf("%.4f %.4f %.4f %.4f\n", a[0], a[1], a[2], a[3]);
    });
    for (auto &t : ts) t.join();
    fprintf(stderr, "positions per permutation: %.3f\n", double(total) / perms);
}
