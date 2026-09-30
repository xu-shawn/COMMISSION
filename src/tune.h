#pragma once
// Search tunables.
//
// Every default below reproduces the behaviour of the code this file was added to, so a plain `make`
// searches exactly as before except for the hashing order (TUNE_CHAIN_HASH, which changes the order in
// which seeds are visited but not which seeds pass).
//
//   make PRESET=record                      thresholds tuned for islands of 11 million blocks and more
//   make TUNE="-DTUNE_MAX_SCORE=0.046f"     override single values (wins over a preset)
//
// How these values were measured is written up in docs/search-economics.md.

#if defined(TUNE_PRESET_RECORD) && TUNE_PRESET_RECORD
// Island frequency falls off as exp(-350 d0^2 - 55 d1^2 - 4 d2^2) in the distance d of each octave's
// vertical offset from the middle of its lattice layer, for islands of record size.  A level set of that
// density keeps more islands per surviving seed than the weighted sum of |d| used before.
#  ifndef TUNE_SCORE_POW
#    define TUNE_SCORE_POW 2
#  endif
#  ifndef TUNE_W0
#    define TUNE_W0 350.0f
#  endif
#  ifndef TUNE_W1
#    define TUNE_W1 55.0f
#  endif
#  ifndef TUNE_W2
#    define TUNE_W2 4.0f
#  endif
#  ifndef TUNE_MAX_SCORE
#    define TUNE_MAX_SCORE 1.5f
#  endif
// fewer survivors per seed, so the fixed cost of the late kernels is spread over four times the seeds
#  ifndef TUNE_LOG2_BATCH
#    define TUNE_LOG2_BATCH 30
#  endif
// hand the CPU only candidates whose estimated area is at least 10.9 million blocks
#  ifndef TUNE_MIN_2B
#    define TUNE_MIN_2B 140
#  endif
#  ifndef TUNE_MIN_2C
#    define TUNE_MIN_2C 420
#  endif
#  ifndef TUNE_MIN_2D
#    define TUNE_MIN_2D 1700
#  endif
#endif

// ---- filter_seeds ---------------------------------------------------------------------------------
// score = combine over octaves 0..2 of both stacks of  W_i * pen(|frac(yo) - 0.5|)  <  TUNE_MAX_SCORE
//   pen(d) = d (TUNE_SCORE_POW 1) or d*d (TUNE_SCORE_POW 2)
//   combine = sum (TUNE_SCORE_MAX 0) or maximum (TUNE_SCORE_MAX 1, a box)
#ifndef TUNE_MAX_SCORE
#define TUNE_MAX_SCORE 0.054f
#endif
#ifndef TUNE_W0
#define TUNE_W0 0.35f
#endif
#ifndef TUNE_W1
#define TUNE_W1 0.11f
#endif
#ifndef TUNE_W2
#define TUNE_W2 0.04f
#endif
#ifndef TUNE_SCORE_POW
#define TUNE_SCORE_POW 1
#endif
#ifndef TUNE_SCORE_MAX
#define TUNE_SCORE_MAX 0
#endif

// Hashing order.
//   0: consecutive seeds.
//   1: walk the chain t_k = t_0 + k*GOLDEN, seed_k = t_k ^ SILVER.  The generator state of a seed is
//      (mix64(t), mix64(t + GOLDEN)), so along the chain the second word of one seed is the first word of
//      the next, and each seed costs one mix64 instead of two.  GOLDEN is odd, so the chain visits every
//      64-bit seed exactly once; position 0 is the --start seed itself.
#ifndef TUNE_CHAIN_HASH
#define TUNE_CHAIN_HASH 1
#endif

// log2 of the seeds per GPU iteration, and of the survivor capacity per iteration.
// Survivors per iteration = pass rate * 2^TUNE_LOG2_BATCH must stay below the capacity.
#ifndef TUNE_LOG2_BATCH
#define TUNE_LOG2_BATCH 28
#endif
#ifndef TUNE_LOG2_SURVIVORS
#define TUNE_LOG2_SURVIVORS 19
#endif

// ---- filter_gradvecs_1: gate on the lower-layer centre columns, bound on the 8-term score ------------
#ifndef TUNE_GV1_Z0_GATE
#define TUNE_GV1_Z0_GATE -4.0f
#endif
#ifndef TUNE_GV1_FINAL
#define TUNE_GV1_FINAL -16.0047f
#endif

// ---- filter_gradvecs_2 -------------------------------------------------------------------------------
#ifndef TUNE_GV2_GATE
#define TUNE_GV2_GATE -13.5f
#endif
#ifndef TUNE_GV2_FINAL
#define TUNE_GV2_FINAL -20.0f
#endif

// Scan the images of stack B column by column (1) or image by image as before (0).  Same candidates.
#ifndef TUNE_GV2_BY_COLUMN
#define TUNE_GV2_BY_COLUMN 1
#endif

// ---- two-octave area tests: minimum samples (of 256) below -0.55 -------------------------------------
#ifndef TUNE_A_MIN
#define TUNE_A_MIN 27
#endif
#ifndef TUNE_B_MIN
#define TUNE_B_MIN 20
#endif

// ---- cascade: minimum samples below -1.05 of 256 / 1024 / 4096 / 16384 -------------------------------
// The last one times 6400 (102400 for large biomes) is the smallest estimated area passed to the CPU.
#ifndef TUNE_MIN_2A
#define TUNE_MIN_2A 24
#endif
#ifndef TUNE_MIN_2B
#define TUNE_MIN_2B 110
#endif
#ifndef TUNE_MIN_2C
#define TUNE_MIN_2C 340
#endif
#ifndef TUNE_MIN_2D
#define TUNE_MIN_2D 1540
#endif

// Write every candidate the GPU passes to the CPU to candidates-dev<N>.txt as "seed x z count", where
// count is the final stage's sample count (estimated area = count * 6400).  Off by default.
#ifndef TUNE_LOG_CANDIDATES
#define TUNE_LOG_CANDIDATES 0
#endif
