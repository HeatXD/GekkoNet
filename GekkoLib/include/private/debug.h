#pragma once

#include <source_location>
#include <string_view>

namespace Gekko {
    void Assert(bool condition, std::string_view message = "", std::source_location location = std::source_location::current());
}
