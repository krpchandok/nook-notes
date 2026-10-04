#pragma once
#include <string>
#include <optional>

namespace nook {
    struct Entry {
        std::string key;
        std::optional<std::string> value; // nullopt indicates deletion
    };
}