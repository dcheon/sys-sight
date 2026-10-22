#include "state/fd_table.h"

#include <gtest/gtest.h>

TEST(FileDescriptorTableTest, TracksOpenDuplicateAndClose) {
    sys_sight::FileDescriptorTable table;
    table.open(3, "/tmp/example");
    table.duplicate(3, 7);

    EXPECT_EQ(table.size(), 2U);
    EXPECT_EQ(table.path(7), "/tmp/example");

    table.close(3);
    EXPECT_FALSE(table.contains(3));
    EXPECT_TRUE(table.contains(7));
}
