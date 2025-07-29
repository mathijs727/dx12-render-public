#include <catch2/catch_session.hpp>
#include <combaseapi.h>

int main(int argc, char* argv[])
{
    // Initialize the COM runtime (required for texture load/store).
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    return Catch::Session().run(argc, argv);
}