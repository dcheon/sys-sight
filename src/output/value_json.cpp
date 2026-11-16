#include "output/value_json.h"

#include "output/jsonl_writer.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace sys_sight {
namespace {

std::uint64_t load(const std::uint8_t* data, std::uint64_t size) {
    std::uint64_t value = 0;
    std::memcpy(&value, data, static_cast<std::size_t>(size));  // little-endian
    return value;
}

std::int64_t sign_extend(std::uint64_t value, unsigned bits) {
    if (bits >= 64) {
        return static_cast<std::int64_t>(value);
    }
    const std::uint64_t sign = std::uint64_t{1} << (bits - 1);
    value &= (sign << 1) - 1;
    return static_cast<std::int64_t>((value ^ sign) - sign);
}

bool printable(std::uint64_t code) {
    return code >= 32 && code < 127;
}

void write_integer(std::ostream& output, const char* kind, std::uint64_t raw, unsigned bits,
                   bool is_signed) {
    output << "{\"kind\":\"" << kind << "\",\"value\":\"";
    if (is_signed) {
        output << sign_extend(raw, bits);
    } else {
        output << (bits >= 64 ? raw : raw & ((std::uint64_t{1} << bits) - 1));
    }
    output << '"';
}

void write_bit_field(std::ostream& output, const DwarfMember& member, const std::uint8_t* data,
                     std::uint64_t available) {
    const unsigned span = (member.bit_offset + member.bit_size + 7) / 8;
    if (span > 8 || member.offset + span > available) {
        output << "{\"kind\":\"unknown\"";
    } else {
        const std::uint64_t raw = load(data + member.offset, span) >> member.bit_offset;
        write_integer(output, member.type->is_signed ? "int" : "uint", raw, member.bit_size,
                      member.type->is_signed);
        output << ",\"bits\":" << member.bit_size;
    }
    output << '}';
}

void write_array_text(std::ostream& output, const DwarfType& type, const std::uint8_t* data) {
    std::string text;
    for (std::uint64_t index = 0; index < type.count && data[index] != 0; ++index) {
        text += printable(data[index]) ? static_cast<char>(data[index]) : '.';
    }
    output << ",\"text\":";
    write_json_string(output, text);
}

}  // namespace

void write_value_json(std::ostream& output, const DwarfType& type, const std::uint8_t* data) {
    const unsigned bits = static_cast<unsigned>(type.size * 8);
    switch (type.kind) {
        case TypeKind::Signed:
        case TypeKind::Unsigned:
        case TypeKind::Bool:
            if (type.size > 8) {
                break;
            }
            write_integer(output, type.kind == TypeKind::Bool ? "bool" : "int",
                          load(data, type.size), bits, type.kind == TypeKind::Signed);
            output << '}';
            return;
        case TypeKind::Char: {
            if (type.size > 8) {
                break;
            }
            const std::uint64_t raw = load(data, type.size);
            write_integer(output, "char", raw, bits, type.is_signed);
            if (printable(raw)) {
                output << ",\"text\":";
                write_json_string(output, std::string(1, static_cast<char>(raw)));
            }
            output << '}';
            return;
        }
        case TypeKind::Float: {
            char buffer[40];
            if (type.size == 4) {
                float value;
                std::memcpy(&value, data, sizeof value);
                std::snprintf(buffer, sizeof buffer, "%.9g", static_cast<double>(value));
            } else if (type.size == 8) {
                double value;
                std::memcpy(&value, data, sizeof value);
                std::snprintf(buffer, sizeof buffer, "%.17g", value);
            } else {
                break;
            }
            output << "{\"kind\":\"float\",\"value\":\"" << buffer << "\"}";
            return;
        }
        case TypeKind::Pointer:
            if (type.size != 8) {
                break;
            }
            output << "{\"kind\":\"pointer\",\"value\":\"" << hex_address(load(data, 8))
                   << "\",\"target\":";
            write_json_string(output, type.target);
            output << '}';
            return;
        case TypeKind::Enum: {
            if (type.size > 8) {
                break;
            }
            bool negative = false;
            for (const auto& enumerator : type.enumerators) {
                negative = negative || enumerator.value < 0;
            }
            const std::uint64_t raw = load(data, type.size);
            const std::int64_t value = negative ? sign_extend(raw, bits) : static_cast<std::int64_t>(raw);
            output << "{\"kind\":\"enum\",\"value\":\"" << value << '"';
            for (const auto& enumerator : type.enumerators) {
                if (enumerator.value == value) {
                    output << ",\"name\":";
                    write_json_string(output, enumerator.name);
                    break;
                }
            }
            output << '}';
            return;
        }
        case TypeKind::Struct:
        case TypeKind::Union: {
            output << "{\"kind\":\"" << (type.kind == TypeKind::Struct ? "struct" : "union")
                   << "\",\"fields\":[";
            bool first = true;
            for (const auto& member : type.members) {
                const bool bit_field = member.bit_size != 0;
                if (!bit_field && member.offset + member.type->size > type.size) {
                    continue;
                }
                output << (first ? "" : ",") << "{\"name\":";
                first = false;
                write_json_string(output, member.name);
                output << ",\"offset\":" << member.offset << ",\"type\":";
                write_json_string(output, member.type->name);
                output << ",\"value\":";
                if (bit_field) {
                    write_bit_field(output, member, data, type.size);
                } else {
                    write_value_json(output, *member.type, data + member.offset);
                }
                output << '}';
            }
            output << "]}";
            return;
        }
        case TypeKind::Array: {
            output << "{\"kind\":\"array\",\"count\":" << type.count << ",\"element_type\":";
            write_json_string(output, type.element->name);
            if (type.element->kind == TypeKind::Char && type.element->size == 1) {
                write_array_text(output, type, data);
            }
            output << ",\"elements\":[";
            for (std::uint64_t index = 0; index < type.count; ++index) {
                output << (index == 0 ? "" : ",");
                write_value_json(output, *type.element, data + index * type.element->size);
            }
            output << "]}";
            return;
        }
        case TypeKind::Unknown: break;
    }
    output << "{\"kind\":\"unknown\"}";
}

void write_variables_json(std::ostream& output, const std::vector<FrameVariable>& variables) {
    output << '[';
    bool first = true;
    for (const auto& entry : variables) {
        const auto& variable = entry.variable;
        output << (first ? "" : ",") << "{\"name\":";
        first = false;
        write_json_string(output, variable.name);
        output << ",\"role\":\"" << (variable.parameter ? "parameter" : "local") << "\",\"type\":";
        write_json_string(output, variable.type->name);
        output << ",\"address\":\"" << hex_address(variable.address)
               << "\",\"size\":" << variable.type->size;
        if (entry.bytes) {
            output << ",\"bytes\":\"";
            for (const auto byte : *entry.bytes) {
                char pair[3];
                std::snprintf(pair, sizeof pair, "%02x", byte);
                output << pair;
            }
            output << "\",\"value\":";
            write_value_json(output, *variable.type, entry.bytes->data());
        } else if (variable.type->size > kMaxVariableBytes) {
            output << ",\"truncated\":true";
        } else {
            output << ",\"unreadable\":true";
        }
        output << '}';
    }
    output << ']';
}

}  // namespace sys_sight
