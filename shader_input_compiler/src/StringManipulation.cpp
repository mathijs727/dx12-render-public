#include "StringManipulation.h"
#include <algorithm> // for transform
#include <cctype> // for tolower, toupper, isupper
#include <memory> // for _Simple_types
#include <utility> // for begin, end

std::string title(std::string str)
{
    if (str.empty())
        return "";
    str[0] = (char)std::toupper(str[0]);
    return str;
}

std::string notTitle(std::string str)
{
    if (str.empty())
        return str;

    str[0] = static_cast<char>(std::tolower(str[0]));
    return str;
}

std::string strToUpper(std::string str)
{
    std::transform(std::begin(str), std::end(str), std::begin(str), [](auto c) { return static_cast<char>(std::toupper(c)); });
    return str;
}

std::string snakeCase(std::string str)
{
    if (str.empty())
        return str;

    str[0] = static_cast<char>(std::tolower(str[0]));
    for (auto iter = std::begin(str); iter != std::end(str); iter++) {
        if (std::isupper(*iter)) {
            *iter = static_cast<char>(std::tolower(*iter));
            iter = str.insert(iter, '_');
        }
    }
    return str;
}
