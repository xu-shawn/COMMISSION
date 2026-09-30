// Largest connected mushroom island near each candidate, measured exactly.
//
// Uses the same routine and settings as the program's final CPU step (scale 1:4, tolerance 2), but collects
// every region in the window in one flood-fill pass and reports the largest, where the program stops at the
// first region that reaches --size.  That makes it usable on candidates of any size, which the measurement
// loop in docs/search-economics.md needs.
//
// build:  gcc -O3 -fwrapv -I src -I cubiomes tools/verify_largest.c \
//             cubiomes/{noise,biomenoise,biomes,generator,layers,finders}.c -lm -o verify_largest
// use:    ./verify_largest [large_biomes=0] [smallest region, blocks=1000000] < candidates > verified
// input:  "seed x z [count]" per line (the format of candidates-dev<N>.txt)
// output: "seed x z count  centre_x centre_z area  regions"
#include "../src/cubiomes.c"
#include <inttypes.h>

int main(int argc, char **argv) {
    int large = argc > 1 ? atoi(argv[1]) : 0;
    int32_t floor_size = argc > 2 ? atoi(argv[2]) : 1000000;
    Cubiomes *c = cubiomes_create(large);
    int32_t range = 12800 * (large ? 4 : 1);
    char line[256];
    Pos pos[256]; int siz[256];
    while (fgets(line, sizeof line, stdin)) {
        int64_t seed; int32_t x, z; long count = 0;
        if (sscanf(line, "%" SCNd64 " %d %d %ld", &seed, &x, &z, &count) < 3) continue;
        cubiomes_apply_seed(c, (uint64_t)seed);
        Range r = make_range(x, z, range, 4);
        int n = getBiomeCentersOpt(pos, siz, 256, &c->g, r, mushroom_fields, floor_size / 16, 2, 0, NULL);
        int b = -1;
        for (int i = 0; i < n; i++) if (b < 0 || siz[i] > siz[b]) b = i;
        if (b >= 0) printf("%" PRId64 " %d %d %ld %d %d %d %d\n", seed, x, z, count, pos[b].x, pos[b].z, siz[b] * 16, n);
        else        printf("%" PRId64 " %d %d %ld %d %d 0 0\n", seed, x, z, count, x, z);
        fflush(stdout);
    }
    cubiomes_free(c);
    return 0;
}
