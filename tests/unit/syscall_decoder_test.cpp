#include "syscall/syscall_decoder_x86_64.h"

#include <gtest/gtest.h>
#include <sys/syscall.h>

TEST(SyscallDecoderTest, NamesKnownSystemCalls) {
    EXPECT_EQ(sys_sight::syscall_name_x86_64(SYS_openat), "openat");
    EXPECT_EQ(sys_sight::syscall_name_x86_64(SYS_mmap), "mmap");
}

TEST(SyscallDecoderTest, CreatesOnlySuccessfulExitEvents) {
    sys_sight::SyscallEvent event{
        1, 10, 10, SYS_mmap, false, {0, 4096, 3, 34, static_cast<std::uint64_t>(-1), 0},
        0x70000000, 1, "mmap"};
    const auto state_event = sys_sight::state_event_from_syscall(event);
    ASSERT_TRUE(state_event.has_value());
    EXPECT_TRUE(std::holds_alternative<sys_sight::MmapEvent>(*state_event));
}

TEST(SyscallDecoderTest, IgnoresEntryAndFailedEvents) {
    sys_sight::SyscallEvent event{
        1, 10, 10, SYS_close, true, {3, 0, 0, 0, 0, 0}, 0, 1, "close"};
    EXPECT_FALSE(sys_sight::state_event_from_syscall(event).has_value());
    event.entry = false;
    event.return_value = -1;
    EXPECT_FALSE(sys_sight::state_event_from_syscall(event).has_value());
}
