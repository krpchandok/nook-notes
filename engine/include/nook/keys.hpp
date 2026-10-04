#pragma once
#include <string>

namespace nook {

inline std::string prefix_successor(const std::string& prefix) {
    std::string end = prefix;
    while (!end.empty() && static_cast<unsigned char>(end.back()) == 0xFF) {
        end.pop_back();
    }
    if (end.empty()) return "";
    end.back() = static_cast<char>(static_cast<unsigned char>(end.back()) + 1);
    return end;
}



}