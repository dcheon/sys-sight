#include "state/memory_oracle.h"

#include <algorithm>

namespace sys_sight {
namespace {

struct Run {
    Address start;
    Address end;
    int protection;
};

// Sorted runs with touching ranges of equal protection merged.
std::vector<Run> merge(std::vector<Run> runs) {
    std::sort(runs.begin(), runs.end(), [](const Run& a, const Run& b) { return a.start < b.start; });
    std::vector<Run> merged;
    for (const auto& run : runs) {
        if (!merged.empty() && merged.back().end == run.start &&
            merged.back().protection == run.protection) {
            merged.back().end = run.end;
        } else {
            merged.push_back(run);
        }
    }
    return merged;
}

int protection_at(const std::vector<Run>& runs, Address address) {
    const auto next = std::upper_bound(runs.begin(), runs.end(), address,
                                       [](Address value, const Run& run) { return value < run.start; });
    if (next == runs.begin()) {
        return -1;
    }
    const auto& candidate = *(next - 1);
    return address < candidate.end ? candidate.protection : -1;
}

}  // namespace

OracleResult compare_memory_maps(const std::vector<MemoryRegion>& reducer,
                                 const std::vector<ProcMapsEntry>& proc) {
    OracleResult result;

    std::vector<Run> proc_runs;
    proc_runs.reserve(proc.size());
    for (const auto& entry : proc) {
        proc_runs.push_back({entry.start, entry.end, protection_from_permissions(entry.permissions)});
    }

    std::vector<Run> reducer_runs;
    reducer_runs.reserve(reducer.size());
    for (const auto& region : reducer) {
        reducer_runs.push_back({region.start, region.end, region.protection});
    }

    // The kernel grows [stack] downward on page faults, with no system call.
    for (const auto& entry : proc) {
        if (entry.name != "[stack]") {
            continue;
        }
        const int protection = protection_from_permissions(entry.permissions);
        for (auto& run : reducer_runs) {
            if (run.end == entry.end && run.start > entry.start && run.protection == protection) {
                result.stack_growth_bytes = run.start - entry.start;
                run.start = entry.start;
            }
        }
    }

    const auto reducer_merged = merge(std::move(reducer_runs));
    const auto proc_merged = merge(std::move(proc_runs));

    std::vector<Address> boundaries;
    for (const auto* runs : {&reducer_merged, &proc_merged}) {
        for (const auto& run : *runs) {
            boundaries.push_back(run.start);
            boundaries.push_back(run.end);
        }
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
        const Address start = boundaries[index];
        const Address end = boundaries[index + 1];
        const int reducer_protection = protection_at(reducer_merged, start);
        const int proc_protection = protection_at(proc_merged, start);
        if (reducer_protection == proc_protection) {
            continue;
        }
        if (!result.mismatches.empty()) {
            auto& last = result.mismatches.back();
            if (last.end == start && last.reducer_protection == reducer_protection &&
                last.proc_protection == proc_protection) {
                last.end = end;
                continue;
            }
        }
        result.mismatches.push_back({start, end, reducer_protection, proc_protection});
    }
    return result;
}

}  // namespace sys_sight
