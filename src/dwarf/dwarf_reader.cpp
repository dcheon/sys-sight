#include "dwarf/dwarf_reader.h"

#include <dwarf.h>
#include <elfutils/libdw.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <utility>

namespace sys_sight {
namespace {

constexpr int kMaxTypeDepth = 12;

std::string attr_string(Dwarf_Die* die, unsigned attribute) {
    Dwarf_Attribute value;
    if (dwarf_attr_integrate(die, attribute, &value) == nullptr) {
        return {};
    }
    const char* text = dwarf_formstring(&value);
    return text == nullptr ? std::string{} : std::string(text);
}

std::optional<std::uint64_t> attr_unsigned(Dwarf_Die* die, unsigned attribute) {
    Dwarf_Attribute value;
    Dwarf_Word number = 0;
    if (dwarf_attr_integrate(die, attribute, &value) == nullptr ||
        dwarf_formudata(&value, &number) != 0) {
        return std::nullopt;
    }
    return number;
}

// The DIE a DW_AT_type refers to; false for `void` (no attribute).
bool type_of(Dwarf_Die* die, Dwarf_Die* out) {
    Dwarf_Attribute attribute;
    return dwarf_attr_integrate(die, DW_AT_type, &attribute) != nullptr &&
           dwarf_formref_die(&attribute, out) != nullptr;
}

bool is_alias(int tag) {
    return tag == DW_TAG_typedef || tag == DW_TAG_const_type || tag == DW_TAG_volatile_type ||
           tag == DW_TAG_restrict_type || tag == DW_TAG_atomic_type;
}

// Follows typedefs and qualifiers down to the type that has a shape. False for void.
bool strip_aliases(Dwarf_Die* die) {
    for (int guard = 0; guard < 64 && is_alias(dwarf_tag(die)); ++guard) {
        Dwarf_Die next;
        if (!type_of(die, &next)) {
            return false;
        }
        *die = next;
    }
    return !is_alias(dwarf_tag(die));
}

// Array dimensions of an array DIE, outermost first. A dimension of 0 is unknown.
std::vector<std::uint64_t> array_dimensions(Dwarf_Die* array) {
    std::vector<std::uint64_t> dimensions;
    Dwarf_Die child;
    if (dwarf_child(array, &child) != 0) {
        return dimensions;
    }
    do {
        const int tag = dwarf_tag(&child);
        if (tag != DW_TAG_subrange_type && tag != DW_TAG_enumeration_type) {
            continue;
        }
        if (const auto count = attr_unsigned(&child, DW_AT_count)) {
            dimensions.push_back(*count);
        } else if (const auto upper = attr_unsigned(&child, DW_AT_upper_bound)) {
            dimensions.push_back(*upper + 1);
        } else {
            dimensions.push_back(0);
        }
    } while (dwarf_siblingof(&child, &child) == 0);
    return dimensions;
}

std::string dimension_suffix(const std::vector<std::uint64_t>& dimensions, std::size_t from) {
    std::string suffix;
    for (std::size_t index = from; index < dimensions.size(); ++index) {
        suffix += "[" + (dimensions[index] == 0 ? std::string{} : std::to_string(dimensions[index])) +
                  "]";
    }
    return suffix;
}

std::string type_name(Dwarf_Die* die, int depth = 0) {
    if (depth > kMaxTypeDepth) {
        return "?";
    }
    const int tag = dwarf_tag(die);
    Dwarf_Die inner;
    const bool has_inner = type_of(die, &inner);
    const std::string name = attr_string(die, DW_AT_name);
    switch (tag) {
        case DW_TAG_base_type:
        case DW_TAG_typedef: return name.empty() ? "?" : name;
        case DW_TAG_structure_type: return name.empty() ? "struct" : "struct " + name;
        case DW_TAG_union_type: return name.empty() ? "union" : "union " + name;
        case DW_TAG_enumeration_type: return name.empty() ? "enum" : "enum " + name;
        case DW_TAG_subroutine_type: return "function";
        case DW_TAG_pointer_type: {
            const std::string target = has_inner ? type_name(&inner, depth + 1) : "void";
            return target.back() == '*' ? target + "*" : target + " *";
        }
        case DW_TAG_const_type:
        case DW_TAG_volatile_type:
        case DW_TAG_restrict_type:
        case DW_TAG_atomic_type: {
            const char* qualifier = tag == DW_TAG_const_type      ? "const "
                                    : tag == DW_TAG_volatile_type ? "volatile "
                                    : tag == DW_TAG_restrict_type ? "restrict "
                                                                  : "_Atomic ";
            return qualifier + (has_inner ? type_name(&inner, depth + 1) : std::string("void"));
        }
        case DW_TAG_array_type:
            return (has_inner ? type_name(&inner, depth + 1) : std::string("void")) +
                   dimension_suffix(array_dimensions(die), 0);
        default: return "?";
    }
}

DwarfTypePtr build_type(Dwarf_Die* die, int depth);

DwarfTypePtr build_array(Dwarf_Die* element_die, bool has_element,
                         const std::vector<std::uint64_t>& dimensions, std::size_t index,
                         const std::string& base_name, int depth) {
    auto array = std::make_shared<DwarfType>();
    array->name = base_name + dimension_suffix(dimensions, index);
    array->count = dimensions[index];
    if (index + 1 < dimensions.size()) {
        array->element =
            build_array(element_die, has_element, dimensions, index + 1, base_name, depth);
    } else if (has_element) {
        array->element = build_type(element_die, depth + 1);
    }
    if (array->count == 0 || array->element == nullptr || array->element->size == 0) {
        return array;  // kind stays Unknown: a flexible or variable-length array
    }
    array->kind = TypeKind::Array;
    array->size = array->count * array->element->size;
    return array;
}

void read_enumerators(Dwarf_Die* enumeration, DwarfType& type) {
    Dwarf_Die child;
    if (dwarf_child(enumeration, &child) != 0) {
        return;
    }
    do {
        if (dwarf_tag(&child) != DW_TAG_enumerator) {
            continue;
        }
        Dwarf_Attribute value;
        Dwarf_Sword number = 0;
        if (dwarf_attr(&child, DW_AT_const_value, &value) != nullptr &&
            dwarf_formsdata(&value, &number) == 0) {
            type.enumerators.push_back({attr_string(&child, DW_AT_name), number});
        }
    } while (dwarf_siblingof(&child, &child) == 0);
}

void read_members(Dwarf_Die* aggregate, DwarfType& type, int depth) {
    Dwarf_Die child;
    if (dwarf_child(aggregate, &child) != 0) {
        return;
    }
    do {
        if (dwarf_tag(&child) != DW_TAG_member) {
            continue;
        }
        DwarfMember member;
        member.name = attr_string(&child, DW_AT_name);
        member.offset = attr_unsigned(&child, DW_AT_data_member_location).value_or(0);
        member.bit_size = static_cast<unsigned>(attr_unsigned(&child, DW_AT_bit_size).value_or(0));
        if (member.bit_size != 0) {
            // DWARF 4 and later give the bit offset from the start of the struct.
            if (const auto bits = attr_unsigned(&child, DW_AT_data_bit_offset)) {
                member.offset = *bits / 8;
                member.bit_offset = static_cast<unsigned>(*bits % 8);
            }
        }
        Dwarf_Die member_type;
        if (type_of(&child, &member_type)) {
            member.type = build_type(&member_type, depth + 1);
            type.members.push_back(std::move(member));
        }
    } while (dwarf_siblingof(&child, &child) == 0);
}

DwarfTypePtr build_type(Dwarf_Die* die, int depth) {
    auto type = std::make_shared<DwarfType>();
    type->name = type_name(die);
    if (depth > kMaxTypeDepth) {
        return type;
    }
    Dwarf_Die shape = *die;
    if (!strip_aliases(&shape)) {
        return type;  // void
    }
    const int tag = dwarf_tag(&shape);
    const auto byte_size = attr_unsigned(&shape, DW_AT_byte_size);
    switch (tag) {
        case DW_TAG_base_type:
            type->size = byte_size.value_or(0);
            switch (attr_unsigned(&shape, DW_AT_encoding).value_or(0)) {
                case DW_ATE_boolean: type->kind = TypeKind::Bool; break;
                case DW_ATE_float: type->kind = TypeKind::Float; break;
                case DW_ATE_signed_char:
                    type->kind = TypeKind::Char;
                    type->is_signed = true;
                    break;
                case DW_ATE_unsigned_char: type->kind = TypeKind::Char; break;
                case DW_ATE_signed:
                    type->kind = TypeKind::Signed;
                    type->is_signed = true;
                    break;
                case DW_ATE_unsigned:
                case DW_ATE_UTF: type->kind = TypeKind::Unsigned; break;
                default: break;
            }
            break;
        case DW_TAG_pointer_type: {
            type->kind = TypeKind::Pointer;
            type->size = byte_size.value_or(8);
            Dwarf_Die target;
            type->target = type_of(&shape, &target) ? type_name(&target) : "void";
            break;
        }
        case DW_TAG_enumeration_type:
            type->kind = TypeKind::Enum;
            type->size = byte_size.value_or(4);
            read_enumerators(&shape, *type);
            break;
        case DW_TAG_structure_type:
        case DW_TAG_union_type:
            type->kind = tag == DW_TAG_structure_type ? TypeKind::Struct : TypeKind::Union;
            type->size = byte_size.value_or(0);
            read_members(&shape, *type, depth);
            break;
        case DW_TAG_array_type: {
            const auto dimensions = array_dimensions(&shape);
            if (dimensions.empty()) {
                break;
            }
            Dwarf_Die element;
            const bool has_element = type_of(&shape, &element);
            const std::string base = has_element ? type_name(&element) : "void";
            auto array = std::make_shared<DwarfType>(
                *build_array(&element, has_element, dimensions, 0, base, depth));
            array->name = type->name;  // keep the typedef name if the program used one
            return array;
        }
        default: break;
    }
    if (type->size == 0) {
        type->kind = TypeKind::Unknown;
    }
    return type;
}

// How far the CFA (the frame base gcc uses) is above rbp. In code with frame
// pointers it is 16. Empty if the function uses any other frame base.
std::optional<std::int64_t> cfa_above_rbp(Dwarf_Die* function) {
    Dwarf_Attribute attribute;
    Dwarf_Op* operations = nullptr;
    std::size_t count = 0;
    if (dwarf_attr(function, DW_AT_frame_base, &attribute) == nullptr ||
        dwarf_getlocation(&attribute, &operations, &count) != 0 || count != 1) {
        return std::nullopt;
    }
    if (operations[0].atom == DW_OP_call_frame_cfa) {
        return 16;
    }
    if (operations[0].atom == DW_OP_breg6) {  // rbp + n
        return static_cast<std::int64_t>(operations[0].number);
    }
    return std::nullopt;
}

void collect_variables(Dwarf_Die* scope, std::optional<std::int64_t> frame_base, Address frame_pointer,
                       std::vector<LocalVariable>& result) {
    Dwarf_Die child;
    if (dwarf_child(scope, &child) != 0) {
        return;
    }
    do {
        const int tag = dwarf_tag(&child);
        if (tag != DW_TAG_variable && tag != DW_TAG_formal_parameter) {
            continue;
        }
        Dwarf_Attribute location;
        Dwarf_Op* operations = nullptr;
        std::size_t count = 0;
        if (dwarf_attr_integrate(&child, DW_AT_location, &location) == nullptr ||
            dwarf_getlocation(&location, &operations, &count) != 0 || count != 1) {
            continue;
        }
        Address address = 0;
        if (operations[0].atom == DW_OP_fbreg && frame_base) {
            address = frame_pointer +
                      static_cast<Address>(static_cast<std::int64_t>(operations[0].number) +
                                           *frame_base);
        } else if (operations[0].atom == DW_OP_addr) {
            address = operations[0].number;  // a static local
        } else {
            continue;
        }
        Dwarf_Die type_die;
        std::string name = attr_string(&child, DW_AT_name);
        if (name.empty() || !type_of(&child, &type_die)) {
            continue;
        }
        result.push_back({std::move(name), tag == DW_TAG_formal_parameter, address,
                          build_type(&type_die, 0)});
    } while (dwarf_siblingof(&child, &child) == 0);
}

}  // namespace

std::optional<DwarfReader> DwarfReader::open(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return std::nullopt;
    }
    Dwarf* dwarf = dwarf_begin(fd, DWARF_C_READ);
    if (dwarf == nullptr) {
        ::close(fd);
        return std::nullopt;
    }
    return DwarfReader(dwarf, fd);
}

DwarfReader::DwarfReader(DwarfReader&& other) noexcept : dwarf_(other.dwarf_), fd_(other.fd_) {
    other.dwarf_ = nullptr;
    other.fd_ = -1;
}

DwarfReader::~DwarfReader() {
    if (dwarf_ != nullptr) {
        dwarf_end(dwarf_);
        ::close(fd_);
    }
}

std::vector<LocalVariable> DwarfReader::locals_at(Address pc, Address frame_pointer) const {
    std::vector<LocalVariable> result;
    // A return address is one past a call; the call itself is inside the scope.
    const Dwarf_Addr lookup = pc - 1;
    Dwarf_Die unit;
    if (dwarf_addrdie(dwarf_, lookup, &unit) == nullptr) {
        return result;
    }
    Dwarf_Die* scopes = nullptr;
    const int scope_count = dwarf_getscopes(&unit, lookup, &scopes);
    // Scopes run innermost first, up to the function: list the function's own
    // variables first, then those of the nested blocks that contain pc.
    for (int function = 0; function < scope_count; ++function) {
        if (dwarf_tag(&scopes[function]) != DW_TAG_subprogram) {
            continue;
        }
        const auto frame_base = cfa_above_rbp(&scopes[function]);
        for (int scope = function; scope >= 0; --scope) {
            collect_variables(&scopes[scope], frame_base, frame_pointer, result);
        }
        break;
    }
    std::free(scopes);
    return result;
}

}  // namespace sys_sight
