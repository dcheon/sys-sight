#include "state/memory_oracle.h"

#include <gtest/gtest.h>

namespace {

using sys_sight::MemoryRegion;
using sys_sight::ProcMapsEntry;

constexpr int kRead = 1;
constexpr int kWrite = 2;
constexpr int kExec = 4;

}  // namespace

TEST(MemoryOracleTest, IdenticalMapsMatchEvenWithDifferentNamesAndSplits) {
    const std::vector<MemoryRegion> reducer = {{0x1000, 0x3000, kRead | kWrite, "<mmap>"}};
    const std::vector<ProcMapsEntry> proc = {{0x1000, 0x2000, "rw-p", "/a"}, {0x2000, 0x3000, "rw-s", ""}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    EXPECT_TRUE(result.matches());
    EXPECT_EQ(result.stack_growth_bytes, 0U);
}

TEST(MemoryOracleTest, ReportsRegionMissingFromReducer) {
    const std::vector<MemoryRegion> reducer = {{0x1000, 0x2000, kRead, ""}};
    const std::vector<ProcMapsEntry> proc = {{0x1000, 0x2000, "r--p", ""}, {0x4000, 0x6000, "rw-p", "[heap]"}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    ASSERT_EQ(result.mismatches.size(), 1U);
    EXPECT_EQ(result.mismatches[0].start, 0x4000U);
    EXPECT_EQ(result.mismatches[0].end, 0x6000U);
    EXPECT_EQ(result.mismatches[0].reducer_protection, -1);
    EXPECT_EQ(result.mismatches[0].proc_protection, kRead | kWrite);
}

TEST(MemoryOracleTest, ReportsRegionOnlyInReducer) {
    const std::vector<MemoryRegion> reducer = {{0x1000, 0x2000, kRead, ""}, {0x8000, 0x9000, kRead, ""}};
    const std::vector<ProcMapsEntry> proc = {{0x1000, 0x2000, "r--p", ""}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    ASSERT_EQ(result.mismatches.size(), 1U);
    EXPECT_EQ(result.mismatches[0].reducer_protection, kRead);
    EXPECT_EQ(result.mismatches[0].proc_protection, -1);
}

TEST(MemoryOracleTest, ReportsOnlyTheSubrangeWhoseProtectionDiffers) {
    const std::vector<MemoryRegion> reducer = {{0x1000, 0x5000, kRead | kWrite, ""}};
    const std::vector<ProcMapsEntry> proc = {
        {0x1000, 0x2000, "rw-p", ""}, {0x2000, 0x4000, "r--p", ""}, {0x4000, 0x5000, "rw-p", ""}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    ASSERT_EQ(result.mismatches.size(), 1U);
    EXPECT_EQ(result.mismatches[0].start, 0x2000U);
    EXPECT_EQ(result.mismatches[0].end, 0x4000U);
    EXPECT_EQ(result.mismatches[0].reducer_protection, kRead | kWrite);
    EXPECT_EQ(result.mismatches[0].proc_protection, kRead);
}

TEST(MemoryOracleTest, StackGrowthIsReportedNotCountedAsMismatch) {
    const std::vector<MemoryRegion> reducer = {{0x7000, 0x8000, kRead | kWrite, "[stack]"}};
    const std::vector<ProcMapsEntry> proc = {{0x5000, 0x8000, "rw-p", "[stack]"}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    EXPECT_TRUE(result.matches());
    EXPECT_EQ(result.stack_growth_bytes, 0x2000U);
}

TEST(MemoryOracleTest, StackThatShrinksOrChangesProtectionIsAMismatch) {
    const std::vector<MemoryRegion> reducer = {{0x5000, 0x8000, kRead | kWrite | kExec, "[stack]"}};
    const std::vector<ProcMapsEntry> proc = {{0x5000, 0x8000, "rw-p", "[stack]"}};

    const auto result = sys_sight::compare_memory_maps(reducer, proc);

    ASSERT_EQ(result.mismatches.size(), 1U);
    EXPECT_EQ(result.mismatches[0].reducer_protection, kRead | kWrite | kExec);
}

TEST(MemoryOracleTest, EmptyMapsMatch) {
    EXPECT_TRUE(sys_sight::compare_memory_maps({}, {}).matches());
}
