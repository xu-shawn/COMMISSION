// Arithmetic of gradvecs_1's gate as a join (TUNE_GV1_JOIN, kernel_v5), on random permutations:
//   gv1join PERMS [DIAG]
// lists of A rows and B rows that can pass (a fifth of the rows each), pairs that pass and their columns, steps
// of the join and drains of the survivor queue when a lane queues one column of a lag at a time (successors go
// back into the queue) or all of them at once, per-warp shares with the pair grid dealt by A rows (DIAG 0) or by
// diagonals (DIAG 1), and the hit-loop passes and drains of `kernel` for the same hits.
#include "../../src/kernel_0A.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
int main(int argc, char **argv) {
    int N = argc > 1 ? atoi(argv[1]) : 2000; float T = -5.0f; int diag = argc > 2 ? atoi(argv[2]) : 0;
    std::mt19937_64 rng(99);
    double sHits = 0, sDr = 0, sDrLanes = 0, sSteps = 0, sDr2 = 0, sDr2Lanes = 0, sMaxDr = 0, sMaxDr2 = 0, sOldDr = 0, sOldPass = 0;
    for (int it = 0; it < N; it++) {
        uint8_t p[256]; for (int i = 0; i < 256; i++) p[i] = i;
        for (int i = 0; i < 256; i++) { int j = i + rng() % (256 - i); std::swap(p[i], p[j]); }
        int ny = rng() % 256;
        static float A[256], B[256];
        for (int r = 0; r < 256; r++) { float sa = 0, sb = 0; for (int dz = 0; dz < 6; dz++) { sa += host_kernel_0A[0][2][dz][p[(r + dz) & 255] & 15]; sb += host_kernel_0A[0][3][dz][p[(r + dz) & 255] & 15]; } A[r] = sa; B[r] = sb; }
        int i0[262]; for (int n = 0; n < 262; n++) i0[n] = p[(p[n & 255] + ny) & 255];
        float maxA = -1e9, maxB = -1e9; for (int t = 0; t < 256; t++) { maxA = std::max(maxA, A[t]); maxB = std::max(maxB, B[t]); }
        std::vector<int> la, lb; for (int t = 0; t < 256; t++) { if (A[t] >= T - maxB - 1e-3f) la.push_back(t); if (B[t] >= T - maxA - 1e-3f) lb.push_back(t); }
        int lagcnt[256] = {0}; for (int nx = 0; nx < 256; nx++) lagcnt[(i0[nx + 3] - i0[nx + 2]) & 255]++;
        int na = la.size(), nb = lb.size();
        double mxd = 0, mxd2 = 0;
        for (int w = 0; w < 8; w++) {
            std::vector<int> q;   // remaining chain length of each queued entry (1 = last column)
            int q2 = 0; double dr = 0, dr2 = 0;
            auto drain = [&](int act) { dr++; sDrLanes += act; std::vector<int> re; for (int i = 0; i < act; i++) { int c = q.back(); q.pop_back(); if (c > 1) re.push_back(c - 1); } for (int c : re) q.push_back(c); };
            for (int c = w; c < na; c += 8) for (int j0 = 0; j0 < nb; j0 += 32) {
                sSteps++;
                for (int l = 0; l < 32 && j0 + l < nb; l++) {
                    int cc = diag ? (c + l) % na : c;
                    if (A[la[cc]] + B[lb[j0 + l]] >= T) { int h = lagcnt[(lb[j0 + l] - la[cc]) & 255]; if (h) { q.push_back(h); q2 += h; sHits += h; } }
                }
                while ((int)q.size() >= 32) drain(32);
                while (q2 >= 32) { q2 -= 32; dr2++; sDr2Lanes += 32; }
            }
            while (!q.empty()) drain(std::min<int>(q.size(), 32));
            if (q2) { dr2++; sDr2Lanes += q2; }
            sDr += dr; sDr2 += dr2; mxd = std::max(mxd, dr); mxd2 = std::max(mxd2, dr2);
        }
        sMaxDr += mxd; sMaxDr2 += mxd2;
        // `kernel`: lanes are columns, hits per column; passes of the hit loop per warp and drains
        for (int w = 0; w < 8; w++) { int tot = 0; for (int base = 0; base < na; base += 32) { int mx = 0; for (int l = 0; l < 32; l++) { int nx = 32 * w + l; int d = (i0[nx + 3] - i0[nx + 2]) & 255; int h = 0; for (int c = base; c < std::min(na, base + 32); c++) if (A[la[c]] + B[(la[c] + d) & 255] >= T) h++; mx = std::max(mx, h); tot += h; } sOldPass += mx; } sOldDr += (tot + 31) / 32; }
    }
    printf("%s: hits %.0f  steps %.1f | chain queue: drains %.1f (%.1f lanes each), busiest warp %.1f | push all columns: drains %.1f (%.1f lanes), busiest warp %.1f | kernel: hit-loop passes %.1f drains %.1f\n", diag ? "diagonals" : "rows", sHits / N, sSteps / N, sDr / N, sDrLanes / sDr, sMaxDr / N, sDr2 / N, sDr2Lanes / sDr2, sMaxDr2 / N, sOldPass / N, sOldDr / N);
}
