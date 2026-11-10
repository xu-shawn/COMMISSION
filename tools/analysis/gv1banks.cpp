// Shared-memory wavefront and instruction arithmetic of the gradvecs_1 gate scan, on random permutations.
// For each kernel layout it replays which shared-memory words the 32 lanes of a warp read in one LDS and counts
// the wavefronts that load needs (the largest number of DISTINCT words that fall in one of the 32 banks).
//   gv1banks PERMS [GATE]          wavefront and hit-loop arithmetic of each layout
//   gv1banks verify [PERMS]        TUNE_GV1_LEAN's scan replayed against the reference gate: outputs must match
// Layouts:  v1 = `kernel` (lanes are columns, candidates broadcast, B = conv_z0[row][2], row stride 5 floats)
//           v2 = `kernel_v2` (lanes are candidates in ascending t, columns in the loop, B = bdup[t + d])
//           v1 with K copies = `kernel` with TUNE_GV1_LEAN = K (B stored K-fold, a lane reads the copy given by
//                its rank among the warp's lanes with the same lag residue mod 32 / K)
//           v5 = `kernel_v2` with TUNE_GV1_V2 == 2: B stored 8-fold (row stride 8 floats), lane l reads copy l / 4
//                and holds a candidate with t % 4 == l % 4: bank = 8 * ((t + d) % 4) + l / 4, distinct for all lanes
#include "../../src/kernel_0A.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

static int wavefronts(const std::vector<int> &words) {   // word addresses (in 4-byte units) read by the lanes
    int best = 0;
    for (int b = 0; b < 32; b++) {
        std::vector<int> w;
        for (int x : words) if (((x % 32) + 32) % 32 == b) w.push_back(x);
        std::sort(w.begin(), w.end()); w.erase(std::unique(w.begin(), w.end()), w.end());
        best = std::max(best, (int)w.size());
    }
    return best;
}

// TUNE_GV1_LEAN replayed on the host: candidate rows carry (row offset, lowest passing B minus a margin), the scan
// runs in fours over a list padded with never-passing entries, each lane reads its own copy of B, and survivors
// are tested against the exact gate when scored. Returns the number of permutations whose outputs differ from
// the reference (gate a + b >= T, then the 8-term score > F).
static long verify_lean(int N, float T, float F, int K) {
    std::mt19937_64 rng(777);
    long bad = 0, outs = 0, extra = 0;
    for (int it = 0; it < N; it++) {
        uint8_t p[256]; for (int i = 0; i < 256; i++) p[i] = i;
        for (int i = 0; i < 256; i++) { int j = i + rng() % (256 - i); std::swap(p[i], p[j]); }
        int ny = rng() % 256;
        static float conv[2][256][4];
        for (int y = 0; y < 2; y++) for (int r = 0; r < 256; r++) for (int x = 1; x < 5; x++) {
            float s = 0; for (int dz = 0; dz < 6; dz++) s += host_kernel_0A[y][x][dz][p[(r + dz) & 255] & 15]; conv[y][r][x - 1] = s; }
        int i0[262], i1[262];
        for (int n = 0; n < 262; n++) { i0[n] = p[(p[n & 255] + ny) & 255]; i1[n] = p[(p[n & 255] + ny + 1) & 255]; }
        float maxB = -1e9; for (int t = 0; t < 256; t++) maxB = std::max(maxB, conv[0][t][2]);
        auto score = [&](int nx, int nz, float &p0) {
            p0 = conv[0][(i0[nx + 2] + nz) & 255][1] + conv[0][(i0[nx + 3] + nz) & 255][2];
            return p0 + conv[0][(i0[nx + 1] + nz) & 255][0] + conv[0][(i0[nx + 4] + nz) & 255][3]
                 + conv[1][(i1[nx + 1] + nz) & 255][0] + conv[1][(i1[nx + 2] + nz) & 255][1]
                 + conv[1][(i1[nx + 3] + nz) & 255][2] + conv[1][(i1[nx + 4] + nz) & 255][3];
        };
        std::vector<int> ref, got;
        for (int nx = 0; nx < 256; nx++) for (int nz = 0; nz < 256; nz++) { float p0; float sc = score(nx, nz, p0); if (p0 >= T && sc > F) ref.push_back(nx * 256 + nz); }
        // the kernel
        static float bk[256 * 16]; for (int r = 0; r < 256; r++) for (int c = 0; c < K; c++) bk[r * K + c] = conv[0][r][2];   // row stride K floats
        struct Pk { uint32_t off; float thr; }; std::vector<Pk> pk; std::vector<int> ct;
        for (int t = 0; t < 256; t++) { float a = conv[0][t][1]; if (a >= T - maxB - 1.0e-3f) { ct.push_back(t); pk.push_back({uint32_t(t) * K * 4, (T - a) - 1.0e-4f}); } }
        const int nc = ct.size(); for (int q = 0; q < 3; q++) pk.push_back({0u, 3.0e38f});
        for (int w = 0; w < 8; w++) {
            int cnt[32] = {0};
            for (int lane = 0; lane < 32; lane++) {
                int nx = 32 * w + lane; uint32_t d = (i0[nx + 3] - i0[nx + 2]) & 255;
                uint32_t cls = d & (32 / K - 1); uint32_t copy = cnt[cls]++ & (K - 1);
                uint32_t bk_lane = d * K * 4 + copy * 4;
                for (int base = 0; base < nc; base += 32) {
                    int n_this = std::min(32, nc - base); uint32_t hit = 0;
                    for (int j = 0; j < n_this; j += 4) { uint32_t nib = 0;
                        for (int q = 0; q < 4; q++) { Pk e = pk[base + j + q]; uint32_t off = (e.off + bk_lane) & (256 * K * 4 - 1);
                            float b = *(const float *)((const char *)bk + off); if (b >= e.thr) nib |= 1u << q; }
                        hit |= nib << j; }
                    while (hit) { int j = __builtin_ctz(hit); hit &= hit - 1; int t = ct[base + j]; int nz = (t - i0[nx + 2]) & 255;
                        float p0; float sc = score(nx, nz, p0); extra += !(p0 >= T);
                        if (sc > F && p0 >= T) got.push_back(nx * 256 + nz); }
                }
            }
        }
        std::sort(ref.begin(), ref.end()); std::sort(got.begin(), got.end());
        outs += ref.size(); if (ref != got) bad++;
    }
    printf("verify K=%d gate %.1f final %.1f: %d permutations, %ld outputs, %ld permutations differ, %ld extra survivors of the margin\n", K, T, F, N, outs, bad, extra);
    return bad;
}

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "verify") {
        int n = argc > 2 ? atoi(argv[2]) : 2000; long bad = 0;
        for (int K : {4, 8, 16}) { bad += verify_lean(n, -5.0f, -15.0f, K); bad += verify_lean(n / 4, -4.0f, -16.0047f, K); }
        return bad != 0;
    }
    int N = argc > 1 ? atoi(argv[1]) : 2000;
    float T = argc > 2 ? atof(argv[2]) : -5.0f;
    std::mt19937_64 rng(4242);
    double s_nc = 0, s_ncB = 0, s_ncmin = 0, s_hits = 0, s_final = 0;
    double v1_loads = 0, v1_wf = 0, v1_iters = 0, v1_steps = 0;
    double v2_loads = 0, v2_wf = 0, v2_iters = 0, v2_steps = 0;
    double v5_loads = 0, v5_wf = 0, v5_iters = 0, v5_steps = 0, v5_chunks = 0, v5_spill = 0;
    double v5b_chunks = 0, v5b_steps = 0;   // capacity 16 per parity (16-fold copy)
    double dr_v1_conv = 0, dr_v1_n = 0;
    long hist_chunks[8] = {0};
    const int kset[4] = {2, 4, 8, 16}; double vk_loads[4] = {0}, vk_wf[4] = {0};
    for (int it = 0; it < N; it++) {
        uint8_t p[256]; for (int i = 0; i < 256; i++) p[i] = i;
        for (int i = 0; i < 256; i++) { int j = i + rng() % (256 - i); std::swap(p[i], p[j]); }
        int ny = rng() % 256;
        static float conv[2][256][4];
        for (int y = 0; y < 2; y++) for (int r = 0; r < 256; r++) for (int x = 1; x < 5; x++) {
            float s = 0; for (int dz = 0; dz < 6; dz++) s += host_kernel_0A[y][x][dz][p[(r + dz) & 255] & 15]; conv[y][r][x - 1] = s; }
        int i0[262], i1[262];
        for (int n = 0; n < 262; n++) { i0[n] = p[(p[n & 255] + ny) & 255]; i1[n] = p[(p[n & 255] + ny + 1) & 255]; }
        float A[256], B[256], maxA = -1e9, maxB = -1e9;
        for (int t = 0; t < 256; t++) { A[t] = conv[0][t][1]; B[t] = conv[0][t][2]; maxA = std::max(maxA, A[t]); maxB = std::max(maxB, B[t]); }
        std::vector<int> cand;
        for (int t = 0; t < 256; t++) if (A[t] >= T - maxB - 1e-3f) cand.push_back(t);
        int ncB = 0; for (int t = 0; t < 256; t++) ncB += B[t] >= T - maxA - 1e-3f;
        int nc = cand.size();
        s_nc += nc; s_ncB += ncB; s_ncmin += std::min(nc, ncB);
        int lag[256]; for (int nx = 0; nx < 256; nx++) lag[nx] = (i0[nx + 3] - i0[nx + 2]) & 255;
        auto pass = [&](int t, int nx) { return A[t] + B[(t + lag[nx]) & 255] >= T; };
        // hits and finals
        for (int nx = 0; nx < 256; nx++) for (int t : cand) if (pass(t, nx)) {
            s_hits++;
            int nz = (t - i0[nx + 2]) & 255; float sc = 0;
            for (int x = 1; x <= 4; x++) { sc += conv[0][(i0[nx + x] + nz) & 255][x - 1]; sc += conv[1][(i1[nx + x] + nz) & 255][x - 1]; }
            if (sc > -15.0f) s_final++;
        }
        // ---- v1: warp w holds columns 32w..32w+31; per candidate one B load at word 5*((t+d)&255)+2
        for (int w = 0; w < 8; w++) {
            for (int base = 0; base < nc; base += 32) {
                int n_this = std::min(32, nc - base); int maxpop = 0;
                std::vector<int> pop(32, 0);
                for (int j = 0; j < n_this; j++) {
                    int t = cand[base + j]; std::vector<int> words;
                    for (int l = 0; l < 32; l++) { int nx = 32 * w + l; words.push_back(5 * ((t + lag[nx]) & 255) + 2); pop[l] += pass(t, nx); }
                    v1_loads++; v1_wf += wavefronts(words);
                }
                for (int l = 0; l < 32; l++) maxpop = std::max(maxpop, pop[l]);
                v1_iters += maxpop + 1; v1_steps++;
            }
        }
        // ---- v1k: v1 with B stored k-fold (row stride k floats); lane l reads copy c_l = its rank among the
        //      warp's lanes with the same d % (32 / k), so lanes of one class use different banks while they fit
        for (int ki = 0; ki < 4; ki++) {
            const int k = kset[ki], ncls = 32 / k;
            for (int w = 0; w < 8; w++) {
                int cnt[32] = {0}, copy[32];
                for (int l = 0; l < 32; l++) { int r = lag[32 * w + l] % ncls; copy[l] = cnt[r]++ % k; }
                for (int j = 0; j < nc; j++) {
                    int t = cand[j]; std::vector<int> words;
                    for (int l = 0; l < 32; l++) words.push_back(k * ((t + lag[32 * w + l]) & 255) + copy[l]);
                    vk_loads[ki]++; vk_wf[ki] += wavefronts(words);
                }
            }
        }
        // ---- v2: chunk c holds candidates 32c..32c+31 (ascending t) on its lanes, invalid lanes read t = 0
        {
            int chunks = (nc + 31) / 32;
            for (int c = 0; c < chunks; c++) for (int g = 0; g < 8; g++) {
                std::vector<int> pop(32, 0);
                for (int q = 0; q < 32; q++) {
                    int nx = 32 * g + q; std::vector<int> words;
                    for (int l = 0; l < 32; l++) { int ci = 32 * c + l; int t = ci < nc ? cand[ci] : 0; words.push_back(t + lag[nx]); if (ci < nc) pop[l] += pass(t, nx); }
                    v2_loads++; v2_wf += wavefronts(words);
                }
                int maxpop = 0; for (int l = 0; l < 32; l++) maxpop = std::max(maxpop, pop[l]);
                v2_iters += maxpop + 1; v2_steps++;
            }
        }
        // ---- v5: lane l of a chunk takes a candidate with t % 4 == l % 4, reading copy l / 4 of row t + d
        {
            int cls[4] = {0, 0, 0, 0}; for (int t : cand) cls[t & 3]++;
            int need = 1; for (int r = 0; r < 4; r++) need = std::max(need, (cls[r] + 7) / 8);
            if (nc == 0) need = 0;
            v5_chunks += need; hist_chunks[std::min(need, 7)]++;
            int par[2] = {0, 0}; for (int t : cand) par[t & 1]++;
            int needb = std::max((par[0] + 15) / 16, (par[1] + 15) / 16);
            v5b_chunks += needb; v5b_steps += needb * 8;
            // placement: rank k within its class -> chunk k / 8, lane 4 * (k % 8) + class
            std::vector<std::vector<int>> slot(need, std::vector<int>(32, -1));
            int k[4] = {0, 0, 0, 0};
            for (int t : cand) { int r = t & 3; int kk = k[r]++; slot[kk / 8][4 * (kk % 8) + r] = t; }
            for (int c = 0; c < need; c++) for (int g = 0; g < 8; g++) {
                std::vector<int> pop(32, 0);
                for (int q = 0; q < 32; q++) {
                    int nx = 32 * g + q; std::vector<int> words;
                    for (int l = 0; l < 32; l++) { int t = slot[c][l]; int tt = t < 0 ? (l & 3) : t; words.push_back(8 * (tt + lag[nx]) + l / 4); if (t >= 0) pop[l] += pass(t, nx); }
                    v5_loads++; v5_wf += wavefronts(words);
                }
                int maxpop = 0; for (int l = 0; l < 32; l++) maxpop = std::max(maxpop, pop[l]);
                v5_iters += maxpop + 1; v5_steps++;
            }
        }
    }
    printf("gate %.1f, %d permutations\n", T, N);
    printf("per seed: list A %.1f  list B %.1f  min(A,B) %.1f  gate hits %.0f  final (> -15) %.2f\n", s_nc / N, s_ncB / N, s_ncmin / N, s_hits / N, s_final / N);
    printf("layout  B loads/seed  wavefronts/load  B wavefronts/seed  hit-loop passes/seed  (32x32 steps/seed)\n");
    printf("v1      %8.0f      %5.2f           %8.0f          %6.1f              %5.1f\n", v1_loads / N, v1_wf / v1_loads, v1_wf / N, v1_iters / N, v1_steps / N);
    printf("v2      %8.0f      %5.2f           %8.0f          %6.1f              %5.1f\n", v2_loads / N, v2_wf / v2_loads, v2_wf / N, v2_iters / N, v2_steps / N);
    printf("v5      %8.0f      %5.2f           %8.0f          %6.1f              %5.1f\n", v5_loads / N, v5_wf / v5_loads, v5_wf / N, v5_iters / N, v5_steps / N);
    for (int ki = 0; ki < 4; ki++) printf("v1 with %2d B copies: %5.2f wavefronts/load, %6.0f B wavefronts/seed, table %d B (256 rows)\n", kset[ki], vk_wf[ki] / vk_loads[ki], vk_wf[ki] / N, 256 * kset[ki] * 4);
    printf("v5 chunks/seed %.2f (dense would be %.2f); 16-fold copy with parity classes: %.2f chunks\n", v5_chunks / N, v2_steps / N / 8, v5b_chunks / N);
    printf("v5 chunk histogram:"); for (int i = 0; i < 8; i++) printf(" %d:%ld", i, hist_chunks[i]); printf("\n");
}
