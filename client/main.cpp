#include "client.hpp"
#include "precompiled.hpp"
#include <argh.h>

auto main(int32_t argc, char** argv) -> int32_t
{
    argh::parser command_line(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

    uint32_t port;
    if (!(command_line({"-p", "--port"}) >> port))
    {
        port = 5555;
    }

    std::string address;
    if (!(command_line({"-c", "--connect"}) >> address))
    {
        address = "127.0.0.1";
    }

    try
    {
        exchange::Client client(address, port);
        client.run();
        return EXIT_SUCCESS;
    }
    catch (std::exception e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}