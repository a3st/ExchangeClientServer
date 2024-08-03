#include "precompiled.hpp"
#include "server.hpp"
#include <argh.h>

auto main(int32_t argc, char** argv) -> int32_t
{
    argh::parser command_line(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

    uint32_t port;
    if (!(command_line({"-p", "--port"}) >> port))
    {
        port = 5555;
    }

    std::string log_path;
    if (!(command_line({"-l", "--log"}) >> log_path))
    {
        log_path = "server.log";
    }

    if (command_line[{"-t", "--trace"}])
    {
        spdlog::set_level(spdlog::level::trace);
    }
    else
    {
#ifndef NDEBUG
        spdlog::set_level(spdlog::level::debug);
#else
        spdlog::set_level(spdlog::level::info);
#endif
    }

    try
    {
        exchange::Server server(port, std::filesystem::path(log_path).make_preferred());
        server.run();
        return EXIT_SUCCESS;
    }
    catch (std::exception e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}