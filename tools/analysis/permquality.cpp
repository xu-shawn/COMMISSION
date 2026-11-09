// Per random permutation (octave 0A): gradvecs_1 outputs (gate >= -5, final > -15), their island weight under the
// fitted model, and setup-level summaries available before the scan: rows in the A list, rows in the B list,
// max of the gate columns, and the mean of the top 8 values of each gate column.
//   permquality THREADS PERMS > table
#include "../../src/kernel_0A.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long perms = atol(argv[2]);
    const float beta[8] = {1.46f, 1.19f, 1.19f, 1.46f, 1.53f, 1.23f, 1.23f, 1.53f};
    std::mutex mu; std::vector<std::thread> ts;
    for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
        std::mt19937_64 rng(9090 + th); std::vector<std::array<double, 9>> out;
        for (long it = th; it < perms; it += threads) {
            uint8_t p[256]; for (int i = 0; i < 256; i++) p[i] = i;
            for (int i = 0; i < 256; i++) { int j = i + rng() % (256 - i); std::swap(p[i], p[j]); }
            int ny = rng() % 256;
            float conv[2][256][4];
            for (int y = 0; y < 2; y++) for (int r = 0; r < 256; r++) for (int x = 1; x < 5; x++) {
                float s = 0; for (int dz = 0; dz < 6; dz++) s += host_kernel_0A[y][x][dz][p[(r + dz) & 255] & 15]; conv[y][r][x - 1] = s; }
            int idx[2][262];
            for (int n = 0; n < 262; n++) for (int y = 0; y < 2; y++) idx[y][n] = p[(p[n & 255] + ny + y) & 255];
            float A[256], B[256]; for (int r = 0; r < 256; r++) { A[r] = conv[0][r][1]; B[r] = conv[0][r][2]; }
            float maxA = *std::max_element(A, A + 256), maxB = *std::max_element(B, B + 256);
            int na = 0, nb = 0; for (int r = 0; r < 256; r++) { na += A[r] >= -5 - maxB; nb += B[r] >= -5 - maxA; }
            float sa[256], sb[256]; std::copy(A, A + 256, sa); std::copy(B, B + 256, sb);
            std::sort(sa, sa + 256, std::greater<float>()); std::sort(sb, sb + 256, std::greater<float>());
            double topA = 0, topB = 0; for (int k = 0; k < 8; k++) { topA += sa[k]; topB += sb[k]; }
            int outs = 0; double w = 0;
            for (int nx = 0; nx < 256; nx++) for (int nz = 0; nz < 256; nz++) {
                float t[8];
                for (int y = 0; y < 2; y++) for (int i = 1; i <= 4; i++) t[y * 4 + i - 1] = conv[y][(idx[y][nx + i] + nz) & 255][i - 1];
                if (!(t[1] + t[2] >= -5.0f)) continue;
                float S8 = (t[1] + t[2]) + t[0] + t[3] + t[4] + t[5] + t[6] + t[7];
                if (!(S8 > -15.0f)) continue;
                outs++; float e = 0; for (int k = 0; k < 8; k++) e += beta[k] * t[k]; w += std::exp((double)e + 20.0);
            }
            out.push_back({(double)outs, w, (double)na, (double)nb, maxA, maxB, topA / 8, topB / 8, 0});
        }
        std::lock_guard l(mu);
        for (auto &a : out) printf("%.0f %.6g %.0f %.0f %.4f %.4f %.4f %.4f\n", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    });
    for (auto &t : ts) t.join();
}
