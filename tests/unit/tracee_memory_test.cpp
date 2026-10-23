#include "tracer/tracee_memory.h"

#include <gtest/gtest.h>

#include <unistd.h>

#include <cstdint>

TEST(TraceeMemoryTest, ReadsIntegersOfSupportedSizes) {
    volatile std::uint32_t value = 0x11223344U;
    const auto address = reinterpret_cast<sys_sight::Address>(&value);

    EXPECT_EQ(sys_sight::read_tracee_integer(getpid(), address, 4), 0x11223344U);
    EXPECT_EQ(sys_sight::read_tracee_integer(getpid(), address, 1), 0x44U);  // little-endian
    EXPECT_EQ(sys_sight::read_tracee_integer(getpid(), address, 2), 0x3344U);
}

TEST(TraceeMemoryTest, RejectsUnsupportedSizesAndUnreadableAddresses) {
    std::uint64_t value = 0;
    const auto address = reinterpret_cast<sys_sight::Address>(&value);

    EXPECT_FALSE(sys_sight::read_tracee_integer(getpid(), address, 3).has_value());
    EXPECT_FALSE(sys_sight::read_tracee_integer(getpid(), address, 16).has_value());
    EXPECT_FALSE(sys_sight::read_tracee_integer(getpid(), 0, 4).has_value());
}
