// SPDX-License-Identifier: MIT
#pragma once

#include <FEXCore/Utils/IntervalList.h>
#include <thread>
#include <shared_mutex>

/* proton-mac DIAGNOSTIC (LookupCache overcommit-fault storm). fprintf crashes the FEX ARM64EC module
 * (stderr not wired / exception-handler context), so record into a plain global struct with NO I/O and
 * read it post-mortem via lldb on the KEEP=1 hung process: (lldb) p g_fexovc . Answers H1/H2/H3. */
struct FexovcDiag {
  unsigned long long rtcs_count, rtcs_av_count, rtcs_last_code, rtcs_last_fault, rtcs_last_pc;
  unsigned long long hav_count, hav_enclosed, hav_miss, hav_last_fault;
  unsigned long long hav_last_qbase, hav_last_qregion, hav_last_commitsz, hav_last_ret;
  unsigned long long hav_last_gle, hav_last_after_state, hav_last_after_prot;
};
inline FexovcDiag g_fexovc {};

namespace FEX::Windows {
/**
 * @brief Emulates memory overcommit of reserved regions with exceptions
 */
class OvercommitTracker {
private:
  bool IsWine;
  FEXCore::IntervalList<uint64_t> OvercommitIntervals;
  std::shared_mutex OvercommitIntervalsMutex;

public:
  OvercommitTracker(bool IsWine)
    : IsWine {IsWine} {}

  void MarkRange(uint64_t Start, uint64_t Length) {
    std::unique_lock Lock {OvercommitIntervalsMutex};
    OvercommitIntervals.Insert({Start, Start + Length});
  }

  void UnmarkRange(uint64_t Start, uint64_t Length) {
    std::unique_lock Lock {OvercommitIntervalsMutex};
    OvercommitIntervals.Remove({Start, Start + Length});
  }

  bool HandleAccessViolation(uint64_t FaultAddress) {
    std::shared_lock Lock {OvercommitIntervalsMutex};
    auto Query = OvercommitIntervals.Query(FaultAddress);

    /* proton-mac DIAGNOSTIC (H2 Query hit/miss, H3 commit-sticks) -- record only, no I/O. */
    g_fexovc.hav_count++;
    g_fexovc.hav_last_fault = FaultAddress;
    if (Query.Enclosed) g_fexovc.hav_enclosed++; else g_fexovc.hav_miss++;

    if (Query.Enclosed) {
      if (IsWine) {
        MEMORY_BASIC_INFORMATION Info;
        NtQueryVirtualMemory(NtCurrentProcess(), reinterpret_cast<void*>(FaultAddress), MemoryBasicInformation, &Info, sizeof(Info), nullptr);
        const auto CommitSize = reinterpret_cast<SIZE_T>(Info.BaseAddress) + Info.RegionSize - reinterpret_cast<SIZE_T>(Info.AllocationBase);
        void* cret = VirtualAlloc(reinterpret_cast<void*>(Info.AllocationBase), CommitSize, MEM_COMMIT, PAGE_READWRITE);
        {
          unsigned long gle = GetLastError();
          MEMORY_BASIC_INFORMATION After {};
          NtQueryVirtualMemory(NtCurrentProcess(), reinterpret_cast<void*>(FaultAddress), MemoryBasicInformation, &After, sizeof(After), nullptr);
          g_fexovc.hav_last_qbase = reinterpret_cast<unsigned long long>(Info.AllocationBase);
          g_fexovc.hav_last_qregion = static_cast<unsigned long long>(Info.RegionSize);
          g_fexovc.hav_last_commitsz = static_cast<unsigned long long>(CommitSize);
          g_fexovc.hav_last_ret = reinterpret_cast<unsigned long long>(cret);
          g_fexovc.hav_last_gle = gle;
          g_fexovc.hav_last_after_state = After.State;
          g_fexovc.hav_last_after_prot = After.Protect;
        }
      } else {
        static constexpr size_t MaxFaultCommitSize = 1024 * 64;
        const auto AlignedFaultAddress = reinterpret_cast<void*>(FaultAddress & FEXCore::Utils::FEX_PAGE_MASK);
        VirtualAlloc(AlignedFaultAddress, std::min(Query.Size, MaxFaultCommitSize), MEM_COMMIT, PAGE_READWRITE);
      }
      return true;
    }
    return false;
  }
};
} // namespace FEX::Windows
