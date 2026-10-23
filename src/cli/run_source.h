#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace sys_sight {

// Compiles a C source with the flags sys-sight needs, traces the result, and
// removes the build products. Returns the exit code of the traced program, or
// 1 if the source could not be built.
int run_source(const std::string& source, const std::vector<std::string>& arguments,
               std::ostream& output);

}  // namespace sys_sight
