#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

namespace exchange::modules
{
    class LoginSystem
    {
      public:
        LoginSystem(SQLite::Database& database, std::optional<std::filesystem::path> const log_path);

        ~LoginSystem();

        auto initialize_session(uint64_t const session_id) -> void;

        auto exists(std::string_view const user_name) -> bool;

        auto register_account(std::string_view const user_name, std::string_view const v, uint64_t& user_id) -> bool;

        auto login_account(std::string_view const user_name, std::string_view const v,
                           uint64_t const session_id) -> bool;

        auto login_session(uint64_t const session_id) -> void;

        auto logout_session(uint64_t const session_id) -> void;

        auto close_session(uint64_t const session_id) -> void;

        auto auth_session(uint64_t const session_id) const -> bool;

        auto user_id(uint64_t const session_id) const -> uint64_t;

      private:
        SQLite::Database* m_database;
        std::unordered_map<uint64_t, std::pair<bool, uint64_t>> m_auth_sessions;
    };
} // namespace exchange::modules