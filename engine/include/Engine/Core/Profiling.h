#pragma once
#include <string>

namespace Core {

struct ProfileTask {
    std::string name;
    uint64_t start, end;
};

}
