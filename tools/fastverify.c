// Quick estimate of the island at each candidate, to decide which candidates get the exact (slow) check.
//
// Mushroom fields is the only biome selected by continentalness alone (c < -1.05, docs), so this floods the
// region c < -1.05 on a grid of `step` blocks, sampling only the continentalness noise and the coordinate
// shift, and reports the largest connected region seeded from a coarse lattice around the candidate. It is
// about 100x cheaper than verify_largest, which evaluates every climate parameter at 1:4.
//
// build:  gcc -O3 -fwrapv -I src -I cubiomes tools/fastverify.c cubiomes/{noise,biomenoise,biomes,generator,layers,finders}.c -lm -o fastverify
// use:    ./fastverify [large_biomes=0] [step blocks=16] < candidates > estimates
// input:  "seed x z [count]"; output: "seed x z count  centre_x centre_z approx_area"
#include "../cubiomes/generator.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Generator g;

static double cont(int qx, int qz) {   // biome (1:4) coordinates
    double px = qx, pz = qz;
    px += sampleDoublePerlin(&g.bn.climate[NP_SHIFT], qx, 0, qz) * 4.0;
    pz += sampleDoublePerlin(&g.bn.climate[NP_SHIFT], qz, qx, 0) * 4.0;
    return sampleDoublePerlin(&g.bn.climate[NP_CONTINENTALNESS], px, 0, pz);
}

int main(int argc, char **argv) {
    int large = argc > 1 ? atoi(argv[1]) : 0;
    int step = (argc > 2 ? atoi(argv[2]) : 16) * (large ? 4 : 1);
    int half = 12800 * (large ? 4 : 1);          // window half-width, blocks
    int n = 2 * half / step + 1;                 // grid points per side
    uint8_t *state = malloc((size_t)n * n);      // 0 unknown, 1 inside, 2 outside, 3 inside and counted
    int *stack = malloc(sizeof(int) * (size_t)n * n);
    setupGenerator(&g, MC_NEWEST, large ? LARGE_BIOMES : 0);
    char line[256];
    while (fgets(line, sizeof line, stdin)) {
        int64_t seed; int32_t x, z; long count = 0;
        if (sscanf(line, "%" SCNd64 " %d %d %ld", &seed, &x, &z, &count) < 3) continue;
        applySeed(&g, DIM_OVERWORLD, (uint64_t)seed);
        memset(state, 0, (size_t)n * n);
        int x0 = x - half, z0 = z - half;
        #define AT(i, j) state[(size_t)(i) * n + (j)]
        #define TEST(i, j) (AT(i, j) ? AT(i, j) : (AT(i, j) = cont((x0 + (i) * step) >> 2, (z0 + (j) * step) >> 2) < -1.05 ? 1 : 2))
        long best = 0; double bx = x, bz = z;
        int seedstep = 512 * (large ? 4 : 1) / step;     // seed points every 512 blocks within +-4096
        int c = n / 2, r = 4096 * (large ? 4 : 1) / step;
        for (int si = c - r; si <= c + r; si += seedstep) for (int sj = c - r; sj <= c + r; sj += seedstep) {
            if (TEST(si, sj) != 1) continue;
            long cells = 0; double sx = 0, sz = 0; int sp = 0;
            AT(si, sj) = 3; stack[sp++] = si * n + sj;
            while (sp) {
                int v = stack[--sp], i = v / n, j = v % n;
                cells++; sx += i; sz += j;
                const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int a = i + di[k], b = j + dj[k];
                    if (a < 0 || b < 0 || a >= n || b >= n) continue;
                    if (TEST(a, b) == 1) { AT(a, b) = 3; stack[sp++] = a * n + b; }
                }
            }
            if (cells > best) { best = cells; bx = x0 + sx / cells * step; bz = z0 + sz / cells * step; }
        }
        printf("%" PRId64 " %d %d %ld %d %d %ld\n", seed, x, z, count, (int)bx, (int)bz, best * step * step);
        fflush(stdout);
    }
    return 0;
}
