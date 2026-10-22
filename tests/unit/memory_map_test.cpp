#include "state/memory_map.h"

#include <gtest/gtest.h>

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
