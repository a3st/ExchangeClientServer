#pragma once

#include "core/common.hpp"

namespace exchange
{
    class Client
    {
      public:
        Client(std::string_view const address, uint32_t const port);

        auto run() -> void;

      private:
        boost::asio::io_context m_io_context;
        boost::asio::ip::tcp::socket m_socket;
    };
} // namespace exchange