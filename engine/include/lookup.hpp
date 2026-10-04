#include <string>

struct Lookup {
    enum class State { Found, Deleted, Absent };
    State state;
    std::string value = "";   // only meaningful when Found
};