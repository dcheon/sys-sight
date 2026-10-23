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

namespace {

sys_sight::SyscallEvent exit_event(std::int64_t number, sys_sight::SyscallArguments arguments,
                                   std::int64_t result) {
    return {1, 10, 10, number, false, arguments, result, 1, ""};
}

}  // namespace

TEST(SyscallDecoderTest, MmapKeepsOnlyReadWriteExecuteBits) {
    const auto event = exit_event(SYS_mmap, {0, 4096, 0x1000003, 34, 0, 0}, 0x70000000);  // PROT_GROWSDOWN set
    const auto state_event = sys_sight::state_event_from_syscall(event);
    ASSERT_TRUE(state_event.has_value());
    EXPECT_EQ(std::get<sys_sight::MmapEvent>(*state_event).protection, 3);
}

TEST(SyscallDecoderTest, DecodesMprotect) {
    const auto state_event =
        sys_sight::state_event_from_syscall(exit_event(SYS_mprotect, {0x7000, 0x2000, 5, 0, 0, 0}, 0));
    ASSERT_TRUE(state_event.has_value());
    const auto& event = std::get<sys_sight::MprotectEvent>(*state_event);
    EXPECT_EQ(event.start, 0x7000U);
    EXPECT_EQ(event.length, 0x2000U);
    EXPECT_EQ(event.protection, 5);
}

TEST(SyscallDecoderTest, DecodesMremapAndDontUnmap) {
    const auto plain = sys_sight::state_event_from_syscall(
        exit_event(SYS_mremap, {0x7000, 0x1000, 0x3000, 1, 0, 0}, 0x9000));
    ASSERT_TRUE(plain.has_value());
    const auto& moved = std::get<sys_sight::MremapEvent>(*plain);
    EXPECT_EQ(moved.old_start, 0x7000U);
    EXPECT_EQ(moved.old_length, 0x1000U);
    EXPECT_EQ(moved.new_start, 0x9000U);  // the return value
    EXPECT_EQ(moved.new_length, 0x3000U);
    EXPECT_FALSE(moved.keep_old);

    const auto kept = sys_sight::state_event_from_syscall(
        exit_event(SYS_mremap, {0x7000, 0x1000, 0x1000, 4, 0, 0}, 0x9000));
    EXPECT_TRUE(std::get<sys_sight::MremapEvent>(*kept).keep_old);
}

TEST(SyscallDecoderTest, DecodesBrkRequestAndResult) {
    const auto state_event =
        sys_sight::state_event_from_syscall(exit_event(SYS_brk, {0x24000, 0, 0, 0, 0, 0}, 0x24000));
    ASSERT_TRUE(state_event.has_value());
    const auto& event = std::get<sys_sight::BrkEvent>(*state_event);
    EXPECT_EQ(event.requested, 0x24000U);
    EXPECT_EQ(event.current_break, 0x24000U);
}

TEST(SyscallDecoderTest, IgnoresEntryAndFailedEvents) {
    sys_sight::SyscallEvent event{
        1, 10, 10, SYS_close, true, {3, 0, 0, 0, 0, 0}, 0, 1, "close"};
    EXPECT_FALSE(sys_sight::state_event_from_syscall(event).has_value());
    event.entry = false;
    event.return_value = -1;
    EXPECT_FALSE(sys_sight::state_event_from_syscall(event).has_value());
}
