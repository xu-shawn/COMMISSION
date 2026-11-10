
// ---------------------------------------------------------------------------------------------------------
// appended by simt/build.sh: runs filter_seeds, init_seeds, gradvecs_1 and filter_2_01a on one small batch and
// prints their outputs in a canonical order
#include <algorithm>
#include <tuple>
int main(int argc, char **argv) {
  const uint64_t start_t = argc > 1 ? strtoull(argv[1], 0, 0) : 0x0123456789abcdefull;
  init_grad_dot_table(); init_conv_kernels();
#if TUNE_JOINT_REUSE_A
  { float *a_sums; cudaMalloc(&a_sums, sizeof(float) * 256 * a_sums_cap); device_a_sums = a_sums; }
#endif
  BufferLens lens{};
  DeviceBuffer buffer_seeds(sizeof(uint64_t) * KernelSeed1::threads_per_run);
  DeviceBuffer buffer_results(sizeof(KernelSeed1::Result) * 4096);
  DeviceBuffer buffer_1(1u << 22), buffer_2(1u << 22);
  OutputBuffer<uint64_t> out_seeds((uint64_t *)buffer_seeds.data, &lens.results_len_filter_seeds, 4096);
  KernelFilterSeeds::run(start_t, out_seeds, nullptr);
  const uint32_t ns = lens.results_len_filter_seeds;
  std::vector<uint64_t> seeds(out_seeds.data, out_seeds.data + ns);
  { auto s = seeds; std::sort(s.begin(), s.end()); printf("SEEDS %u\n", ns); for (auto x : s) printf("seed %lld\n", (long long)x); }
  KernelSeed1::Result *results = (KernelSeed1::Result *)buffer_results.data;
  cuda_launch(KernelSeed1::kernel, (ns + 31) / 32, KernelSeed1::threads_per_block, 0, nullptr, InputBuffer<uint64_t>(out_seeds), results);
  OutputBuffer<SeedPos> out_g1(buffer_2, &lens.results_len_filter_gradvecs_1);
  KernelFilterGradVecs1::run(out_seeds, out_g1, results, nullptr);
  const uint32_t ng = lens.results_len_filter_gradvecs_1;
  { std::vector<std::tuple<long long, int, int>> v; for (uint32_t i = 0; i < ng; i++) v.emplace_back((long long)seeds[out_g1.data[i].seed_index], out_g1.data[i].x, out_g1.data[i].z);
    std::sort(v.begin(), v.end()); printf("GV1 %u\n", ng); for (auto &t : v) printf("gv1 %lld %d %d\n", std::get<0>(t), std::get<1>(t), std::get<2>(t)); }
  OutputBuffer<SeedPos> out_a(buffer_1, &lens.results_len_filter_2_0a);
  KernelFilter2_0A::run(out_g1, out_a, results, nullptr);
  const uint32_t na = lens.results_len_filter_2_0a;
  { std::vector<std::tuple<long long, int, int, uint32_t>> v;
    for (uint32_t i = 0; i < na; i++) {
      uint32_t h = 0;
#if TUNE_JOINT_REUSE_A
      for (int k = 0; k < 256; k++) { uint32_t u; memcpy(&u, device_a_sums + (size_t)i * 256 + k, 4); h = h * 16777619u ^ u; }
#endif
      v.emplace_back((long long)seeds[out_a.data[i].seed_index], out_a.data[i].x, out_a.data[i].z, h);
#if TUNE_JOINT_REUSE_A
      if (getenv("DUMP_SUMS")) { fprintf(stderr, "S %lld %d %d", (long long)seeds[out_a.data[i].seed_index], out_a.data[i].x, out_a.data[i].z);
        for (int k = 0; k < 256; k++) fprintf(stderr, " %.9g", device_a_sums[(size_t)i * 256 + k]); fprintf(stderr, "\n"); }
#endif
    }
    std::sort(v.begin(), v.end()); printf("A %u\n", na); for (auto &t : v) printf("a %lld %d %d %08x\n", std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t)); }
#if TUNE_A2_MIN && TUNE_JOINT_B
  if (getenv("LATE")) {
    { float *a_sums2; cudaMalloc(&a_sums2, sizeof(float) * 256 * a_sums_cap); device_a_sums2 = a_sums2; }
    DeviceBuffer flags(sizeof(uint32_t) * 4096);
    cuda_launch(KernelSeed1::late_kernel<1>, 8, KernelSeed1::threads_per_block, 0, nullptr, InputBuffer<uint64_t>(out_seeds), InputBuffer<SeedPos>(out_a), results, (uint32_t *)flags.data);
    DeviceBuffer buffer_a2(1u << 22), buffer_g2(1u << 24), buffer_j(1u << 22);
    OutputBuffer<SeedPos> out_a2(buffer_a2, &lens.results_len_a2);
    cuda_launch(KernelFilter2_0B::a2_kernel, 8, 256, 0, nullptr, InputBuffer<SeedPos>(out_a), out_a2, results);
    printf("A2 %u\n", lens.results_len_a2);
    OutputBuffer<SeedPos> out_g2(buffer_g2, &lens.results_len_filter_gradvecs_2);
    cuda_launch(KernelFilterGradVecs2::kernel, 16, KernelFilterGradVecs2::block_dim_x, 0, nullptr, InputBuffer<SeedPos>(out_a2), out_g2, results);
    printf("GV2 %u\n", lens.results_len_filter_gradvecs_2);
    OutputBuffer<SeedPos> out_j(buffer_j, &lens.results_len_filter_2_0b);
    cuda_launch(KernelFilter2_0B::joint_kernel, 16, 256, 0, nullptr, InputBuffer<SeedPos>(out_g2), out_j, results);
    const uint32_t nj = lens.results_len_filter_2_0b;
    std::vector<std::tuple<long long, int, int>> v;
    for (uint32_t i = 0; i < nj; i++) v.emplace_back((long long)seeds[out_j.data[i].seed_index], out_j.data[i].x, out_j.data[i].z);
    std::sort(v.begin(), v.end()); printf("JOINT %u\n", nj); for (auto &t : v) printf("j %lld %d %d\n", std::get<0>(t), std::get<1>(t), std::get<2>(t));
  }
#endif
  return 0;
}
