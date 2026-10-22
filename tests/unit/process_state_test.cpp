#include "state/process_state.h"

#include <gtest/gtest.h>

TEST(ProcessStateTest, AppliesEventsToIndependentStateModels) {
    sys_sight::ProcessState state;
    state.apply(sys_sight::OpenEvent{4, "/dev/null"});
    state.apply(sys_sight::MmapEvent{0x4000, 0x1000, 5, "anonymous"});

    EXPECT_TRUE(state.file_descriptors().contains(4));
    EXPECT_EQ(state.memory_map().mapped_bytes(), 0x1000U);
}
