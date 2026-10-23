#include "elf/elf_reader.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace sys_sight {
namespace {

// Read-only memory mapping of a whole file.
class MappedFile {
public:
    explicit MappedFile(const std::string& path) {
        const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (descriptor == -1) {
            throw std::system_error(errno, std::generic_category(), "failed to open " + path);
        }
        struct stat info {};
        if (fstat(descriptor, &info) == -1) {
            const int failure = errno;
            close(descriptor);
            throw std::system_error(failure, std::generic_category(), "failed to stat " + path);
        }
        if (info.st_size <= 0) {
            close(descriptor);
            throw std::runtime_error("empty ELF file");
        }
        size_ = static_cast<std::uint64_t>(info.st_size);
        data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, descriptor, 0);
        const int failure = errno;
        close(descriptor);
        if (data_ == MAP_FAILED) {
            throw std::system_error(failure, std::generic_category(), "failed to map " + path);
        }
    }
    ~MappedFile() { munmap(data_, size_); }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    // Copies a T out of the file; memcpy avoids misaligned access on bad input.
    template <typename T>
    [[nodiscard]] T read(std::uint64_t offset) const {
        if (offset > size_ || sizeof(T) > size_ - offset) {
            throw std::runtime_error("ELF structure lies outside the file");
        }
        T value;
        std::memcpy(&value, static_cast<const unsigned char*>(data_) + offset, sizeof value);
        return value;
    }

    // NUL-terminated string at `table_offset + name_offset`, inside the table.
    [[nodiscard]] std::string string_at(std::uint64_t table_offset, std::uint64_t table_size,
                                        std::uint64_t name_offset) const {
        if (name_offset >= table_size || table_offset > size_ || table_size > size_ - table_offset) {
            throw std::runtime_error("ELF string lies outside its table");
        }
        const auto* begin = static_cast<const char*>(data_) + table_offset + name_offset;
        const auto limit = table_size - name_offset;
        const auto* end = static_cast<const char*>(std::memchr(begin, '\0', limit));
        return std::string(begin, end == nullptr ? limit : static_cast<std::size_t>(end - begin));
    }

private:
    void* data_ = nullptr;
    std::uint64_t size_ = 0;
};

}  // namespace

ElfInfo read_elf(const std::string& path) {
    const MappedFile file(path);
    const auto header = file.read<Elf64_Ehdr>(0);
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_machine != EM_X86_64) {
        throw std::runtime_error("not a 64-bit x86-64 ELF file");
    }

    ElfInfo info;
    info.pie = header.e_type == ET_DYN;
    if (header.e_shoff == 0 || header.e_shnum == 0) {
        return info;
    }
    if (header.e_shentsize < sizeof(Elf64_Shdr) || header.e_shstrndx >= header.e_shnum) {
        throw std::runtime_error("invalid ELF section header table");
    }

    std::vector<Elf64_Shdr> headers;
    headers.reserve(header.e_shnum);
    for (std::uint64_t index = 0; index < header.e_shnum; ++index) {
        headers.push_back(file.read<Elf64_Shdr>(header.e_shoff + index * header.e_shentsize));
    }

    const auto& names = headers[header.e_shstrndx];
    std::vector<std::string> section_names;
    section_names.reserve(headers.size());
    for (const auto& section : headers) {
        section_names.push_back(file.string_at(names.sh_offset, names.sh_size, section.sh_name));
        // TLS sections (.tbss) overlap other sections' addresses; leave them out.
        if ((section.sh_flags & SHF_ALLOC) != 0 && (section.sh_flags & SHF_TLS) == 0) {
            info.sections.push_back(
                {section_names.back(), section.sh_addr, section.sh_addr + section.sh_size});
        }
    }

    for (const auto& table : headers) {
        if (table.sh_type != SHT_SYMTAB || table.sh_link >= headers.size()) {
            continue;
        }
        const auto& strings = headers[table.sh_link];
        for (std::uint64_t index = 0; index < table.sh_size / sizeof(Elf64_Sym); ++index) {
            const auto symbol = file.read<Elf64_Sym>(table.sh_offset + index * sizeof(Elf64_Sym));
            const auto type = ELF64_ST_TYPE(symbol.st_info);
            if ((type != STT_OBJECT && type != STT_FUNC) || symbol.st_shndx == SHN_UNDEF ||
                symbol.st_shndx >= headers.size()) {
                continue;
            }
            auto name = file.string_at(strings.sh_offset, strings.sh_size, symbol.st_name);
            if (name.empty()) {
                continue;
            }
            if (type == STT_FUNC) {
                if (symbol.st_size > 0) {
                    info.functions.push_back({std::move(name), symbol.st_value, symbol.st_size});
                }
                continue;
            }
            info.symbols.push_back({std::move(name), symbol.st_value, symbol.st_size,
                                    section_names[symbol.st_shndx], std::nullopt});
        }
    }
    return info;
}

}  // namespace sys_sight
