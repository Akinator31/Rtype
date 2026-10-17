#include "KeyGen.hpp"
#include "rtype/log/Logger.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char **argv)
{
    if (const auto problem = rtype::log::configureFromEnvironment()) {
        rtype::log::Logger("keygen").warning("{}", *problem);
    }
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    return rtype::keygen::run(arguments, std::cout);
}
