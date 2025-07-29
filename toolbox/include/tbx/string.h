#pragma once
#include <cctype>
#include <string>

namespace Tbx {

inline std::string toLower(std::string str)
{
    for (char& c : str)
        c = static_cast<char>(std::tolower(static_cast<char>(c)));
    return str;
}

inline std::string toUpper(std::string str)
{
    for (char& c : str)
        c = static_cast<char>(std::toupper(static_cast<char>(c)));
    return str;
}

}