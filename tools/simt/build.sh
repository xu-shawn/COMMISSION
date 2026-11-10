#!/bin/bash
# Host-executable build of src/gpu.cu's kernels (simt.h: one OS thread per CUDA thread, warp and block primitives
# as rendezvous points), with test_main.inc appended: it runs filter_seeds, init_seeds, gradvecs_1 and
# filter_2_01a on one small batch and prints their outputs in a canonical order; with LATE=1 in the environment
# also the late init, the full-A screen, gradvecs_2 and the joint test. Two builds that must agree are compared
# with cmp on their outputs (the "a" lines carry a hash of the stored stack-A sums; DUMP_SUMS=1 prints the sums to
# stderr for comparisons with a tolerance).
#
#   tools/simt/build.sh <tree> <out> [-D flags]     e.g.  build.sh . /tmp/base -DTUNE_LOG2_BATCH=19 -DTUNE_A_MIN=10
#   /tmp/base [chain start] > base.out
#
# Small batches only: TUNE_LOG2_BATCH=19 takes about 20 s. Loosen TUNE_A_MIN (and TUNE_A2_MIN, TUNE_JOINT_B with
# LATE=1) so that the later stages have outputs to compare.
set -e
T=$1; O=$2; shift 2
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -r -- "$W"' EXIT
# kernel<<<a, b, c, d>>>(args) -> cuda_launch(kernel, a, b, c, d, args); shared memory -> function statics
perl -0pe 's/([A-Za-z_:0-9]+(?:<[^<>;]*>)?)\s*<<<([^;]*?)>>>\s*\(/cuda_launch($1, $2, /g; s/__shared__\s+alignas\((\d+)\)/alignas($1) static/g; s/__shared__/static/g' "$T/src/gpu.cu" > "$W/gpu.cpp"
cat "$D/test_main.inc" >> "$W/gpu.cpp"
g++ -std=c++20 -O1 -pthread -include "$D/simt.h" -I "$T/src" -I "$T/asio/asio/include" -DOMISSION_UNBOUND=1 -DTUNE_PRESET_RECORD=1 \
  -DTUNE_JOINT_REUSE_A=1 -DTUNE_W0=257.3f -DTUNE_W1=47.4f -DTUNE_W2=7.5f -DTUNE_MAX_SCORE=1.6572f -DTUNE_GV1_BPS=3 -DTUNE_A_BPS=3 \
  "$@" -Wno-attributes -x c++ "$W/gpu.cpp" -o "$O"
