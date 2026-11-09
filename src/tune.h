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
// Size-aware thresholds (docs/record-tuning.md). gradvecs_1's z0 gate looks at the bottom layer of the
// central lattice nodes only and drops ~40% of large islands; -5 keeps most of them. Its final score, raised
// from -16 to -15, drops a tenth of 9-10M islands but only ~7% of those above 11M (their octave-0 cores are
// stronger), and cuts the positions handed to filter_2_01a by 2.5x.
#  ifndef TUNE_GV1_Z0_GATE
#    define TUNE_GV1_Z0_GATE -5.0f
#  endif
#  ifndef TUNE_GV1_FINAL
#    define TUNE_GV1_FINAL -15.0f
#  endif
// filter_2_01b replaced by the joint octave 0-1 test (both stacks), which keeps every known island above
// 10.5M and passes about a fortieth of what the B-only test did
#  ifndef TUNE_JOINT_B
#    define TUNE_JOINT_B 24
#  endif
// seed hashing overlapped with the later kernels, and the hashing's zero-state guard skipped (same results)
#  ifndef TUNE_RAW_FORK
#    define TUNE_RAW_FORK 1
#  endif
#  ifndef TUNE_OVERLAP
#    define TUNE_OVERLAP 1
#  endif
#  ifndef TUNE_GV1_BPS
#    define TUNE_GV1_BPS 2
#  endif
#  ifndef TUNE_A_BPS
#    define TUNE_A_BPS 3
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

// ---- additions (see docs/record-tuning.md) ----------------------------------------------------------------

// Skip the all-zero-state guard of XrsrRandom(lo, hi) in the seed hashing (probability 2^-128). Same results.
#ifndef TUNE_RAW_FORK
#define TUNE_RAW_FORK 0
#endif

// Hash batch i+1 on a low-priority stream while batch i runs through the later kernels on a high-priority one
// (seed hashing is bound by the integer ALU pipe, gradvecs_1 by shared-memory loads). Same results.
#ifndef TUNE_OVERLAP
#define TUNE_OVERLAP 0
#endif

// Grid caps, in blocks per SM, for gradvecs_1 and filter_2_01a (0 = the fixed grids used before). With
// TUNE_OVERLAP they leave room on each SM for the hashing blocks.
#ifndef TUNE_GV1_BPS
#define TUNE_GV1_BPS 0
#endif
#ifndef TUNE_A_BPS
#define TUNE_A_BPS 0
#endif

// filter_2_01b replaced by a joint test on octaves 0-1 of both stacks: the minimum number of the 256 samples
// where 0A + 0B + 1A + 1B < TUNE_JOINT_LEVEL (0 = the B-only test).
#ifndef TUNE_JOINT_B
#define TUNE_JOINT_B 0
#endif
#ifndef TUNE_JOINT_LEVEL
#define TUNE_JOINT_LEVEL -1.05
#endif

// gradvecs_1 scanned with candidates on the lanes and columns in the loop (kernel_v2): same candidates,
// conflict-free table reads.
#ifndef TUNE_GV1_V2
#define TUNE_GV1_V2 0
#endif

// timing experiments on kernel_v2 only (wrong results): 1 = setup without scan, 2 = scan without queue/score
#ifndef TUNE_GV1_EXP
#define TUNE_GV1_EXP 0
#endif

// The joint test reuses filter_2_01a's octave 0A + 1A sums instead of recomputing them at every image.
#ifndef TUNE_JOINT_REUSE_A
#define TUNE_JOINT_REUSE_A 0
#endif

// mix64 multipliers of the seed hashing passed as kernel arguments (Pascal: avoids MOV32I per seed). Same results.
#ifndef TUNE_HASH_KARG
#define TUNE_HASH_KARG 0
#endif

// gradvecs_1 (kernel): packed lattice indices and an early exit after the bottom-layer terms when scoring
// gate survivors. Same candidates.
#ifndef TUNE_GV1_DRAIN2
#define TUNE_GV1_DRAIN2 0
#endif

// Joint test pre-filter (needs TUNE_JOINT_REUSE_A): minimum count over 64 samples (4 rows) before the full test.
#ifndef TUNE_JOINT_PRE
#define TUNE_JOINT_PRE 0
#endif
