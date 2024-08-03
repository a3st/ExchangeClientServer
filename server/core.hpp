#pragma once

#include "modules/exchange.hpp"
#include "modules/login.hpp"
#include "modules/wallet.hpp"
#include "session.hpp"
#include <botan/srp6.h>

namespace exchange
{
    class Core
    {
      public:
        Core(std::optional<std::filesystem::path> const log_path);

        ~Core();

        auto start() -> void;

        auto on_session_connected(uint64_t const session_id) -> void;

        auto on_session_closed(uint64_t const session_id) -> void;

        auto on_message(uint64_t const session_id, std::span<uint8_t const> const buffer) -> Response;

      private:
        struct SRP6Session
        {
            Botan::SRP6_Server_Session srp6;
            std::string secret;
            std::string B;
        };

        std::unordered_map<uint64_t, SRP6Session> m_srp6_sessions;

        SQLite::Database m_database;

        modules::LoginSystem m_login_system;
        modules::Wallet m_wallet;
        modules::Exchange m_exchange;
    };
} // namespace exchange