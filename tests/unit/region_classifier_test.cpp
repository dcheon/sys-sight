#include "output/jsonl_writer.h"
#include "state/proc_maps.h"
#include "state/region_classifier.h"

#include <gtest/gtest.h>

#include <sstream>

namespace {

constexpr const char* kMaps =
    "00400000-00401000 r--p 00000000 08:01 100 /home/u/my prog\n"
    "00401000-00402000 r-xp 00001000 08:01 100 /home/u/my prog\n"
    "00402000-00403000 r--p 00002000 08:01 100 /home/u/my prog\n"
    "00404000-00405000 rw-p 00003000 08:01 100 /home/u/my prog\n"
    "00405000-00406000 rw-p 00000000 00:00 0 \n"
    "01b9a000-01bbb000 rw-p 00000000 00:00 0 [heap]\n"
    "7f0000000000-7f0000100000 rw-p 00000000 00:00 0 \n"
    "7f0000200000-7f0000225000 r--p 00000000 08:01 200 /usr/lib/libc.so.6\n"
    "7ffc00000000-7ffc00021000 rw-p 00000000 00:00 0 [stack]\n"
    "7ffc00100000-7ffc00104000 r--p 00000000 00:00 0 [vvar]\n"
    "7ffc00104000-7ffc00106000 r-xp 00000000 00:00 0 [vdso]\n";

}  // namespace

TEST(ProcMapsTest, ParsesPathnameOnly) {
    std::istringstream input(kMaps);
    const auto entries = sys_sight::parse_proc_maps(input);

    ASSERT_EQ(entries.size(), 11U);
    EXPECT_EQ(entries[0].start, 0x400000U);
    EXPECT_EQ(entries[0].end, 0x401000U);
    EXPECT_EQ(entries[0].permissions, "r--p");
    EXPECT_EQ(entries[0].name, "/home/u/my prog");  // spaces kept, offset/dev/inode dropped
    EXPECT_EQ(entries[4].name, "");
    EXPECT_EQ(entries[5].name, "[heap]");
}

TEST(ProcMapsTest, RejectsMalformedLine) {
    std::istringstream input("not a maps line\n");
    EXPECT_THROW((void)sys_sight::parse_proc_maps(input), std::runtime_error);
}

TEST(RegionClassifierTest, ClassifiesEachRegionKind) {
    std::istringstream input(kMaps);
    const auto regions =
        sys_sight::classify_regions(sys_sight::parse_proc_maps(input), "/home/u/my prog");

    using sys_sight::RegionKind;
    const RegionKind expected[] = {
        RegionKind::Rodata, RegionKind::Text,  RegionKind::Rodata, RegionKind::Data,
        RegionKind::Data,   // anonymous pages right after .data are .bss
        RegionKind::Heap,   RegionKind::Mmap,  RegionKind::Mmap,   RegionKind::Stack,
        RegionKind::Kernel, RegionKind::Kernel,
    };
    ASSERT_EQ(regions.size(), std::size(expected));
    for (std::size_t index = 0; index < regions.size(); ++index) {
        EXPECT_EQ(regions[index].kind, expected[index]) << "region " << index;
    }
}

TEST(RegionClassifierTest, UnknownExecutableFallsBackToMmap) {
    std::istringstream input(kMaps);
    const auto regions = sys_sight::classify_regions(sys_sight::parse_proc_maps(input), "");

    EXPECT_EQ(regions[1].kind, sys_sight::RegionKind::Mmap);
    EXPECT_EQ(regions[5].kind, sys_sight::RegionKind::Heap);
}

TEST(JsonOutputTest, EscapesStringsAndFormatsAddresses) {
    std::ostringstream output;
    sys_sight::write_json_string(output, "a\"b\\c\n");

    EXPECT_EQ(output.str(), "\"a\\\"b\\\\c\\u000a\"");
    EXPECT_EQ(sys_sight::hex_address(0x7ffc00000000ULL), "0x7ffc00000000");
}
