#include "elf/elf_reader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {

const sys_sight::ElfSymbol* find_symbol(const sys_sight::ElfInfo& elf, const std::string& name) {
    const auto found = std::find_if(elf.symbols.begin(), elf.symbols.end(),
                                    [&](const auto& symbol) { return symbol.name == name; });
    return found == elf.symbols.end() ? nullptr : &*found;
}

const sys_sight::ElfSection* find_section(const sys_sight::ElfInfo& elf, const std::string& name) {
    const auto found = std::find_if(elf.sections.begin(), elf.sections.end(),
                                    [&](const auto& section) { return section.name == name; });
    return found == elf.sections.end() ? nullptr : &*found;
}

std::filesystem::path write_temp(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

}  // namespace

TEST(ElfReaderTest, ReadsSectionsAndGlobalsOfExample) {
    const auto elf = sys_sight::read_elf(EXAMPLE_BINARY);

    EXPECT_FALSE(elf.pie);
    for (const auto* name : {".text", ".rodata", ".data", ".bss"}) {
        EXPECT_NE(find_section(elf, name), nullptr) << name;
    }

    const struct {
        const char* name;
        const char* section;
    } expected[] = {{"g_rodata", ".rodata"}, {"g_data", ".data"}, {"g_bss", ".bss"}};
    for (const auto& [name, section] : expected) {
        const auto* symbol = find_symbol(elf, name);
        ASSERT_NE(symbol, nullptr) << name;
        EXPECT_EQ(symbol->section, section) << name;
        EXPECT_EQ(symbol->size, 4U) << name;

        const auto* containing = find_section(elf, section);
        ASSERT_NE(containing, nullptr) << section;
        EXPECT_GE(symbol->address, containing->start) << name;
        EXPECT_LT(symbol->address, containing->end) << name;
    }
}

TEST(ElfReaderTest, RejectsFilesThatAreNotElf) {
    const auto path = write_temp("sys_sight_not_elf.txt", "this is not an ELF file at all");
    EXPECT_THROW((void)sys_sight::read_elf(path.string()), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(ElfReaderTest, RejectsTruncatedElf) {
    std::ifstream input(EXAMPLE_BINARY, std::ios::binary);
    const std::string head(std::istreambuf_iterator<char>(input), {});
    const auto path = write_temp("sys_sight_truncated_elf", head.substr(0, 100));
    EXPECT_THROW((void)sys_sight::read_elf(path.string()), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(ElfReaderTest, MissingFileThrowsSystemError) {
    EXPECT_THROW((void)sys_sight::read_elf("/nonexistent/sys_sight_missing"), std::system_error);
}
