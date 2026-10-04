#pragma once
#include <string>

namespace nook {
enum class State { Found, Deleted, Absent };

struct Lookup {
    State state;
    std::string value = "";   // only meaningful when Found
};  
}