// Host execution of the kernels in gpu.cu: every CUDA thread of a block is an OS thread, and the warp- and
// block-level primitives (__ballot_sync, __shfl_*_sync, __syncwarp, __syncthreads) are rendezvous points, so the
// lanes of a warp step through them together as on the device. One block runs at a time (shared memory is
// function-static). For checking results, not speed.
#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#define __global__
#define __device__
#define __host__
#define __forceinline__ inline
#define __launch_bounds__(...)
#define __restrict__ __restrict
struct dim3_ { unsigned x = 1, y = 1, z = 1; };
inline thread_local dim3_ threadIdx, blockIdx;
inline dim3_ blockDim, gridDim;
struct uint4 { unsigned x, y, z, w; };
struct float4 { float x, y, z, w; };
inline float4 make_float4(float a, float b, float c, float d) { return {a, b, c, d}; }
template <class T> T min(T a, T b) { return a < b ? a : b; }
template <class T> T max(T a, T b) { return a < b ? b : a; }

namespace simt {
struct Group {
  std::mutex m; std::condition_variable cv; int n = 0, active = 0; uint64_t gen = 0; std::vector<uint32_t> in, out;
  void init(int k) { active = k; n = 0; in.assign(k, 0); out.assign(k, 0); }
};
inline Group warps[64], block;
template <class F> auto collective(Group &g, int idx, uint32_t v, F &&f) {
  std::unique_lock<std::mutex> lk(g.m);
  g.in[idx] = v;
  if (++g.n == g.active) { g.out = g.in; g.n = 0; g.gen++; g.cv.notify_all(); }
  else { const uint64_t my = g.gen; g.cv.wait(lk, [&] { return g.gen != my; }); }
  return f(g.out);
}
inline Group &warp() { return warps[threadIdx.x >> 5]; }
inline int lane() { return threadIdx.x & 31; }
template <class T> uint32_t bits(T v) { static_assert(sizeof(T) == 4); uint32_t u; std::memcpy(&u, &v, 4); return u; }
template <class T> T unbits(uint32_t u) { T v; std::memcpy(&v, &u, 4); return v; }
}
inline unsigned __ballot_sync(unsigned, bool p) {
  return simt::collective(simt::warp(), simt::lane(), p, [](const std::vector<uint32_t> &o) { unsigned m = 0; for (size_t i = 0; i < o.size(); i++) m |= (o[i] ? 1u : 0u) << i; return m; });
}
template <class T> T __shfl_sync(unsigned, T v, int src) { return simt::collective(simt::warp(), simt::lane(), simt::bits(v), [&](const std::vector<uint32_t> &o) { return simt::unbits<T>(o[src]); }); }
template <class T> T __shfl_down_sync(unsigned, T v, unsigned d) { const unsigned l = simt::lane(); return simt::collective(simt::warp(), l, simt::bits(v), [&](const std::vector<uint32_t> &o) { return simt::unbits<T>(o[l + d < 32 ? l + d : l]); }); }
template <class T> T __shfl_up_sync(unsigned, T v, unsigned d) { const unsigned l = simt::lane(); return simt::collective(simt::warp(), l, simt::bits(v), [&](const std::vector<uint32_t> &o) { return simt::unbits<T>(o[l >= d ? l - d : l]); }); }
template <class T> T __shfl_xor_sync(unsigned, T v, unsigned d) { const unsigned l = simt::lane(); return simt::collective(simt::warp(), l, simt::bits(v), [&](const std::vector<uint32_t> &o) { return simt::unbits<T>(o[l ^ d]); }); }
template <class T> T __reduce_add_sync(unsigned, T v) { return simt::collective(simt::warp(), simt::lane(), simt::bits(v), [&](const std::vector<uint32_t> &o) { T s = 0; for (auto x : o) s += simt::unbits<T>(x); return s; }); }
inline void __syncwarp(unsigned = ~0u) { simt::collective(simt::warp(), simt::lane(), 0, [](const std::vector<uint32_t> &) { return 0; }); }
inline void __syncthreads() { simt::collective(simt::block, threadIdx.x, 0, [](const std::vector<uint32_t> &) { return 0; }); }
template <class T, class U> T atomicAdd(T *p, U v) { return __atomic_fetch_add(p, (T)v, __ATOMIC_SEQ_CST); }
template <class T, class U, class V> T atomicCAS(T *p, U cmp, V v) { T e = (T)cmp; __atomic_compare_exchange_n(p, &e, (T)v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return e; }
template <class T, class U> T atomicExch(T *p, U v) { return __atomic_exchange_n(p, (T)v, __ATOMIC_SEQ_CST); }
inline int __float2int_rd(float f) { return (int)std::floor(f); }
inline int __popc(unsigned v) { return __builtin_popcount(v); }
inline int __ffs(int v) { return __builtin_ffs(v); }
inline float __uint_as_float(unsigned u) { float f; std::memcpy(&f, &u, 4); return f; }
inline unsigned __float_as_uint(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }

// host API: memory is ordinary memory, everything else does nothing
typedef int cudaError_t; constexpr int cudaSuccess = 0;
typedef struct cudaStream_s *cudaStream_t; typedef struct cudaEvent_s *cudaEvent_t;
enum { cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost, cudaStreamNonBlocking, cudaDevAttrMultiProcessorCount, cudaFuncAttributePreferredSharedMemoryCarveout };
inline const char *cudaGetErrorString(cudaError_t) { return ""; }
template <class T> cudaError_t cudaMalloc(T **p, size_t n) { *p = (T *)std::calloc(1, n); return 0; }
inline cudaError_t cudaFree(void *p) { std::free(p); return 0; }
inline cudaError_t cudaMemcpy(void *d, const void *s, size_t n, int) { std::memcpy(d, s, n); return 0; }
template <class T> cudaError_t cudaGetSymbolAddress(void **p, T &sym) { *p = (void *)&sym; return 0; }
template <class T> cudaError_t cudaMemcpyToSymbol(T &sym, const void *s, size_t n) { std::memcpy((void *)&sym, s, n); return 0; }
inline cudaError_t cudaGetDevice(int *d) { *d = 0; return 0; }
inline cudaError_t cudaDeviceGetAttribute(int *v, int, int) { *v = 2; return 0; }   // two "SMs"
template <class... A> cudaError_t cuda_any(A &&...) { return 0; }
#define cudaMemcpyAsync cuda_any
#define cudaMemsetAsync cuda_any
#define cudaGetLastError cuda_any
#define cudaSetDevice cuda_any
#define cudaDeviceGetStreamPriorityRange cuda_any
#define cudaFuncSetAttribute cuda_any
#define cudaStreamCreate cuda_any
#define cudaStreamCreateWithPriority cuda_any
#define cudaStreamDestroy cuda_any
#define cudaStreamSynchronize cuda_any
#define cudaStreamWaitEvent cuda_any
#define cudaEventCreate cuda_any
#define cudaEventDestroy cuda_any
#define cudaEventRecord cuda_any
#define cudaEventSynchronize cuda_any
#define cudaEventElapsedTime cuda_any

template <class K, class... A> void cuda_launch(K kernel, unsigned grid, unsigned block, size_t, cudaStream_t, A... args) {
  gridDim.x = grid; blockDim.x = block;
  for (unsigned b = 0; b < grid; b++) {
    simt::block.init(block);
    for (unsigned w = 0; w * 32 < block; w++) simt::warps[w].init(block - 32 * w < 32 ? block - 32 * w : 32);
    std::vector<std::thread> ts;
    for (unsigned t = 0; t < block; t++) ts.emplace_back([=] { threadIdx.x = t; blockIdx.x = b; kernel(args...); });
    for (auto &t : ts) t.join();
  }
}
