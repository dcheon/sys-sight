#include "state/memory_map.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

using sys_sight::VirtualMemoryMap;

constexpr int kRead = 1;
constexpr int kWrite = 2;

// "start-end:protection" for each region, to compare whole maps at once.
std::vector<std::string> describe(const VirtualMemoryMap& map) {
    std::vector<std::string> result;
    for (const auto& region : map.regions()) {
        result.push_back(std::to_string(region.start) + "-" + std::to_string(region.end) + ":" +
                         std::to_string(region.protection));
    }
    return result;
}

std::string range(sys_sight::Address start, sys_sight::Address end, int protection) {
    return std::to_string(start) + "-" + std::to_string(end) + ":" + std::to_string(protection);
}

}  // namespace

TEST(VirtualMemoryMapTest, RoundsLengthsUpToWholePages) {
    VirtualMemoryMap map;
    map.map(0x10000, 4097, kRead, "");
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x12000, kRead)}));

    map.unmap(0x11000, 1);  // one byte still removes the whole page
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead)}));
}

TEST(VirtualMemoryMapTest, NewMappingReplacesWhatItOverlaps) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x4000, kRead | kWrite, "");
    map.map(0x11000, 0x1000, kRead, "");  // MAP_FIXED over the second page

    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead | kWrite),
                                                      range(0x11000, 0x12000, kRead),
                                                      range(0x12000, 0x14000, kRead | kWrite)}));
}

TEST(VirtualMemoryMapTest, ProtectSplitsAndMergesBack) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x3000, kRead | kWrite, "");
    map.protect(0x11000, 0x1000, kRead);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead | kWrite),
                                                      range(0x11000, 0x12000, kRead),
                                                      range(0x12000, 0x13000, kRead | kWrite)}));

    map.protect(0x11000, 0x1000, kRead | kWrite);  // restoring merges the pieces
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x13000, kRead | kWrite)}));
}

TEST(VirtualMemoryMapTest, ProtectIgnoresUnmappedParts) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x1000, kRead | kWrite, "");
    map.protect(0x10000, 0x3000, kRead);  // only the first page exists
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead)}));
    map.protect(0x20000, 0, kRead);       // empty range: nothing happens
    EXPECT_EQ(map.regions().size(), 1U);
}

TEST(VirtualMemoryMapTest, RemapGrowsInPlaceAndShrinks) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x1000, kRead | kWrite, "");
    map.remap(0x10000, 0x1000, 0x10000, 0x4000, false);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x14000, kRead | kWrite)}));

    map.remap(0x10000, 0x4000, 0x10000, 0x2000, false);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x12000, kRead | kWrite)}));
}

TEST(VirtualMemoryMapTest, RemapMovesAndKeepsProtection) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x1000, kRead, "");
    map.remap(0x10000, 0x1000, 0x50000, 0x3000, false);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x50000, 0x53000, kRead)}));
}

TEST(VirtualMemoryMapTest, RemapWithDontUnmapKeepsTheOldRange) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x1000, kRead, "");
    map.remap(0x10000, 0x1000, 0x50000, 0x1000, true);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead),
                                                      range(0x50000, 0x51000, kRead)}));
}

TEST(VirtualMemoryMapTest, RemapOfUnknownRangeChangesNothing) {
    VirtualMemoryMap map;
    map.map(0x10000, 0x1000, kRead, "");
    map.remap(0x90000, 0x1000, 0xa0000, 0x1000, false);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x10000, 0x11000, kRead)}));
}

TEST(VirtualMemoryMapTest, BreakQueryThenGrowAndShrinkTheHeap) {
    VirtualMemoryMap map;
    map.set_break(0, 0x20000);  // query: the heap starts here, with no pages yet
    EXPECT_TRUE(map.regions().empty());
    EXPECT_EQ(map.program_break(), 0x20000U);

    map.set_break(0x23001, 0x23001);  // grows by 3 pages plus one byte -> 4 pages
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x20000, 0x24000, kRead | kWrite)}));

    map.set_break(0x21000, 0x21000);
    EXPECT_EQ(describe(map), (std::vector<std::string>{range(0x20000, 0x21000, kRead | kWrite)}));

    map.set_break(0x10000, 0x10000);  // never shrinks below the heap start
    EXPECT_TRUE(map.regions().empty());
}

TEST(VirtualMemoryMapTest, HeapIsNotTrackedWhenTheFirstBreakCallIsNotAQuery) {
    VirtualMemoryMap map;
    map.set_break(0x30000, 0x30000);
    EXPECT_TRUE(map.regions().empty());
    EXPECT_EQ(map.program_break(), 0x30000U);
}

TEST(VirtualMemoryMapTest, RejectsEmptyOrOverflowingRanges) {
    VirtualMemoryMap map;
    EXPECT_THROW(map.map(0x1000, 0, kRead), std::invalid_argument);
    EXPECT_THROW(map.unmap(0x1000, 0), std::invalid_argument);
    EXPECT_THROW(map.map(UINT64_MAX - 0x100, 0x1000, kRead), std::invalid_argument);
}

TEST(VirtualMemoryMapTest, SplitsRegionWhenUnmappingMiddle) {
    sys_sight::VirtualMemoryMap map;
    map.map(0x1000, 0x3000, 3, "heap");
    map.unmap(0x2000, 0x1000);

    ASSERT_EQ(map.regions().size(), 2U);
    EXPECT_EQ(map.regions()[0].start, 0x1000U);
    EXPECT_EQ(map.regions()[0].end, 0x2000U);
    EXPECT_EQ(map.regions()[1].start, 0x3000U);
    EXPECT_EQ(map.regions()[1].end, 0x4000U);
}
