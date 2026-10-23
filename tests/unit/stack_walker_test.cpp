#include "state/stack_walker.h"

#include <gtest/gtest.h>

#include <map>

namespace {

using sys_sight::Address;

// A fake stack: main (fp 0x1c0) -> outer (fp 0x180) -> inner (fp 0x140), and the
// marker called from inner with the wrapper's rsp at 0x100.
//
//   0x100  return into inner      <- rsp in the syscall wrapper
//   0x140  inner saved fp (0x180)   0x148  return into outer
//   0x180  outer saved fp (0x1c0)   0x188  return into main
//   0x1c0  main saved fp (1)        0x1c8  return into libc (outside the executable)
//
// The functions' first bytes are `push %rbp; mov %rsp,%rbp` (inner, main) and
// `endbr64; push %rbp; mov %rsp,%rbp` (outer).
std::map<Address, std::uint64_t> make_stack() {
    return {{0x7000'0100, 0x401180}, {0x7000'0140, 0x7000'0180}, {0x7000'0148, 0x401200},
            {0x7000'0180, 0x7000'01c0}, {0x7000'0188, 0x401300}, {0x7000'01c0, 1},
            {0x7000'01c8, 0x7f00'0000'1000},
            {0x401150, 0x0000'0000'e589'4855}, {0x4011e0, 0xe589'4855'fa1e'0ff3},
            {0x401280, 0x0000'0000'e589'4855}};
}

const std::vector<sys_sight::ElfFunction> kFunctions = {
    {"inner", 0x401150, 0x50}, {"outer", 0x4011e0, 0x40}, {"main", 0x401280, 0x100}};
const std::vector<sys_sight::AddressRange> kText = {{0x401000, 0x402000}};

sys_sight::MemoryReader reader(const std::map<Address, std::uint64_t>& memory) {
    return [&memory](Address address) -> std::optional<std::uint64_t> {
        const auto found = memory.find(address);
        if (found == memory.end()) {
            return std::nullopt;
        }
        return found->second;
    };
}

}  // namespace

TEST(StackWalkerTest, WalksNestedFramesUntilItLeavesTheExecutable) {
    const auto memory = make_stack();
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText);

    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::LeftExecutable);
    ASSERT_EQ(walk.frames.size(), 3U);

    EXPECT_EQ(walk.frames[0].function, "inner");
    EXPECT_EQ(walk.frames[0].start, 0x7000'0108U);  // rsp + 8
    EXPECT_EQ(walk.frames[0].end, 0x7000'0150U);    // rbp + 16
    EXPECT_EQ(walk.frames[0].frame_pointer, 0x7000'0140U);
    EXPECT_EQ(walk.frames[0].saved_frame_pointer, 0x7000'0180U);
    EXPECT_EQ(walk.frames[0].return_address, 0x401200U);

    EXPECT_EQ(walk.frames[1].function, "outer");
    EXPECT_EQ(walk.frames[1].start, walk.frames[0].end);  // frames are contiguous
    EXPECT_EQ(walk.frames[1].end, 0x7000'0190U);

    EXPECT_EQ(walk.frames[2].function, "main");
    EXPECT_EQ(walk.frames[2].start, walk.frames[1].end);
    EXPECT_EQ(walk.frames[2].end, 0x7000'01d0U);
}

TEST(StackWalkerTest, UnknownFunctionsHaveEmptyNames) {
    const auto memory = make_stack();
    const auto walk = sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), {}, kText);

    ASSERT_EQ(walk.frames.size(), 3U);
    EXPECT_TRUE(walk.frames[0].function.empty());
}

TEST(StackWalkerTest, StopsWhenMarkerWasNotCalledFromTheExecutable) {
    auto memory = make_stack();
    memory[0x7000'0100] = 0x7f00'0000'2000;  // return address inside a shared library
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText);

    EXPECT_TRUE(walk.frames.empty());
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::LeftExecutable);
}

TEST(StackWalkerTest, ReportsBrokenChainWhenFramePointerIsBelowTheStack) {
    const auto memory = make_stack();
    // rbp below rsp + 8: a function built without frame pointers.
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0080, reader(memory), kFunctions, kText);

    EXPECT_TRUE(walk.frames.empty());
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::BrokenChain);
}

TEST(StackWalkerTest, ReportsBrokenChainWhenSavedFramePointerDoesNotGrow) {
    auto memory = make_stack();
    memory[0x7000'0140] = 0x7000'0120;  // saved fp below the current fp
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText);

    ASSERT_EQ(walk.frames.size(), 1U);
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::BrokenChain);
}

TEST(StackWalkerTest, ReportsBrokenChainWhenMemoryIsUnreadable) {
    const std::map<Address, std::uint64_t> memory;
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText);

    EXPECT_TRUE(walk.frames.empty());
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::BrokenChain);
}

TEST(StackWalkerTest, StopsAtFunctionWithoutFramePointerSetup) {
    auto memory = make_stack();
    memory[0x4011e0] = 0xc031'4883'ec48'8348;  // outer starts with `sub $.., %rsp`, no push %rbp
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText);

    ASSERT_EQ(walk.frames.size(), 1U);  // inner is fine; outer cannot be trusted
    EXPECT_EQ(walk.frames[0].function, "inner");
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::NoFramePointer);
    EXPECT_EQ(walk.failed_function, "outer");
}

TEST(StackWalkerTest, HonorsDepthLimit) {
    const auto memory = make_stack();
    const auto walk =
        sys_sight::walk_stack(0x7000'0100, 0x7000'0140, reader(memory), kFunctions, kText, 2);

    EXPECT_EQ(walk.frames.size(), 2U);
    EXPECT_EQ(walk.end, sys_sight::StackWalkEnd::DepthLimit);
}
