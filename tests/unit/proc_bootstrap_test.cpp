#include "state/proc_bootstrap.h"

#include <gtest/gtest.h>

#include <unistd.h>

TEST(ProcBootstrapTest, SeedsCurrentProcessDescriptorsAndMappings) {
    sys_sight::ProcessState state;
    const auto result = sys_sight::ProcBootstrap::seed(getpid(), state);

    EXPECT_GT(result.file_descriptors, 0U);
    EXPECT_GT(result.memory_regions, 0U);
    EXPECT_TRUE(state.file_descriptors().contains(STDIN_FILENO));
    EXPECT_TRUE(state.file_descriptors().contains(STDOUT_FILENO));
    EXPECT_TRUE(state.file_descriptors().contains(STDERR_FILENO));
    EXPECT_FALSE(state.memory_map().regions().empty());
}
