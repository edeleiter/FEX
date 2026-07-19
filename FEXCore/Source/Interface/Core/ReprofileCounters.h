// SPDX-License-Identifier: MIT
// THROWAWAY re-profile instrumentation (plan: find the next leaf at 13.3 FPS after block-linking Phase 1).
// REVERT BEFORE SHIP. All counters live in libFEXCore (single binary) so the periodic dump sees a consistent
// set. Output uses raw POSIX open/write to /tmp/fex_reprof-<pid>.log — the SAME channel FEX's perf-map
// (JitSymbols.cpp) uses, which is CONFIRMED working from the mingw PE side on macOS (fprintf(stderr) is not).
#pragma once
#include <FEXCore/Utils/SHMStats.h> // GetCycleCounter (CNTVCT_EL0)
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>

namespace FEXCore::Reprofile {
inline std::atomic<uint64_t> g_findblock {0}; // C++ FindBlock (L1-miss slow path) = non-EC re-dispatch volume (indirect branches)
inline std::atomic<uint64_t> g_compile {0};   // CompileBlock (true compiles) = recompile-bound check
inline std::atomic<uint64_t> g_oor_skip {0};  // Phase-1 out-of-range link skip = Phase-2 sizing
inline std::atomic<uint64_t> g_last_cyc {0};
inline int g_fd = -1;
inline std::once_flag g_once;

inline void Dump(uint64_t n) {
  std::call_once(g_once, [] {
    char path[64];
    snprintf(path, sizeof(path), "/tmp/fex_reprof-%d.log", getpid());
    g_fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0644);
  });
  if (g_fd < 0) {
    return;
  }
  uint64_t now = FEXCore::SHMStats::GetCycleCounter();
  uint64_t prev = g_last_cyc.exchange(now, std::memory_order_relaxed);
  char buf[192];
  int len = snprintf(buf, sizeof(buf), "REPROF cyc=%llu dcyc=%llu findblock=%llu compile=%llu oor_skip=%llu\n", (unsigned long long)now,
                     (unsigned long long)(now - prev), (unsigned long long)n, (unsigned long long)g_compile.load(std::memory_order_relaxed),
                     (unsigned long long)g_oor_skip.load(std::memory_order_relaxed));
  if (len > 0) {
    [[maybe_unused]] auto r = write(g_fd, buf, len);
  }
}

// Called from FindBlock's C++ slow path. Dumps once on the first call (confirms plumbing + that FindBlock is
// reached) and every 2^16 calls thereafter, so /tmp/fex_reprof-<pid>.log yields per-interval rates.
inline void OnFindBlock() {
  uint64_t n = g_findblock.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n == 1 || (n & ((1ull << 16) - 1)) == 0) {
    Dump(n);
  }
}
} // namespace FEXCore::Reprofile
