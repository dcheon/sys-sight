#include "state/stack_walker.h"

#include <algorithm>

namespace sys_sight {
namespace {

bool in_text(const std::vector<AddressRange>& text, Address address) {
    return std::any_of(text.begin(), text.end(), [address](const AddressRange& range) {
        return address >= range.start && address < range.end;
    });
}

const ElfFunction* function_at(const std::vector<ElfFunction>& functions, Address address) {
    for (const auto& function : functions) {
        if (address >= function.address && address - function.address < function.size) {
            return &function;
        }
    }
    return nullptr;
}

// `push %rbp; mov %rsp,%rbp`, optionally after `endbr64`. `code` holds the
// function's first eight bytes, little-endian.
bool sets_up_frame_pointer(std::uint64_t code) {
    constexpr std::uint32_t kPushMov = 0xe5894855;     // 55 48 89 e5
    constexpr std::uint32_t kPushMovAlt = 0xec8b4855;  // 55 48 8b ec
    constexpr std::uint32_t kEndbr64 = 0xfa1e0ff3;     // f3 0f 1e fa
    const auto first = static_cast<std::uint32_t>(code);
    const auto second = static_cast<std::uint32_t>(code >> 32);
    const bool direct = first == kPushMov || first == kPushMovAlt;
    return direct || (first == kEndbr64 && (second == kPushMov || second == kPushMovAlt));
}

}  // namespace

const char* to_string(StackWalkEnd end) noexcept {
    switch (end) {
        case StackWalkEnd::LeftExecutable: return "left_executable";
        case StackWalkEnd::BrokenChain: return "broken_chain";
        case StackWalkEnd::NoFramePointer: return "no_frame_pointer";
        case StackWalkEnd::DepthLimit: return "depth_limit";
    }
    return "broken_chain";
}

StackWalk walk_stack(Address rsp, Address rbp, const MemoryReader& read,
                     const std::vector<ElfFunction>& functions,
                     const std::vector<AddressRange>& text, std::size_t max_depth) {
    StackWalk walk;
    // Return address of the call being made: it lies in the function that owns the frame.
    auto return_into = read(rsp);
    if (!return_into) {
        walk.end = StackWalkEnd::BrokenChain;
        return walk;
    }

    Address bottom = rsp + 8;
    Address frame_pointer = rbp;
    while (walk.frames.size() < max_depth) {
        if (!in_text(text, *return_into)) {
            walk.end = StackWalkEnd::LeftExecutable;
            return walk;
        }
        const auto* owner = function_at(functions, *return_into);
        if (owner != nullptr) {
            const auto code = read(owner->address);
            if (code && !sets_up_frame_pointer(*code)) {
                walk.end = StackWalkEnd::NoFramePointer;
                walk.failed_function = owner->name;
                return walk;
            }
        }
        const Address top = frame_pointer + 16;
        if (frame_pointer < bottom || top < frame_pointer) {
            walk.end = StackWalkEnd::BrokenChain;
            return walk;
        }
        const auto saved = read(frame_pointer);
        const auto caller_return = read(frame_pointer + 8);
        if (!saved || !caller_return) {
            walk.end = StackWalkEnd::BrokenChain;
            return walk;
        }

        walk.frames.push_back({owner == nullptr ? std::string{} : owner->name, bottom, top,
                               frame_pointer, *saved, *caller_return});
        if (!in_text(text, *caller_return)) {
            walk.end = StackWalkEnd::LeftExecutable;
            return walk;
        }
        if (*saved <= frame_pointer) {
            walk.end = StackWalkEnd::BrokenChain;
            return walk;
        }
        bottom = top;
        frame_pointer = *saved;
        return_into = caller_return;
    }
    walk.end = StackWalkEnd::DepthLimit;
    return walk;
}

}  // namespace sys_sight
