#pragma once

#include "core.hpp"
#include "session.hpp"

namespace exchange
{
    class Server
    {
      public:
        Server(uint32_t const port, std::filesystem::path const& log_path);

        auto run() -> void;

      private:
        boost::asio::io_context m_io_context;
        boost::asio::ip::tcp::acceptor m_acceptor;

        std::unordered_map<uint64_t, std::shared_ptr<Session>> m_sessions;
        uint64_t m_session_index;

        Core m_core;

        auto accept_connection() -> void;

        auto close_connection(uint64_t const sessionID) -> void;
    };
} // namespace exchange