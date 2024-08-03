#include "login.hpp"
#include "precompiled.hpp"

namespace exchange::modules
{
    LoginSystem::LoginSystem(SQLite::Database& database, std::optional<std::filesystem::path> const log_path)
        : m_database(&database)
    {
        std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
        if (log_path)
        {
            sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.value().string()));
        }
        auto logger = std::make_shared<spdlog::logger>("login", sinks.begin(), sinks.end());
        spdlog::initialize_logger(logger);

        if (!database.tableExists("users"))
        {
            try
            {
                SQLite::Statement statement(*m_database,
                                            "CREATE TABLE users (id INTEGER PRIMARY KEY, user_name TEXT, v TEXT)");
                statement.exec();
            }
            catch (SQLite::Exception e)
            {
                spdlog::get("login")->log(spdlog::level::critical, e.what());
                std::exit(EXIT_FAILURE);
            }
        }
    }

    LoginSystem::~LoginSystem()
    {
        spdlog::drop("login");
    }

    auto LoginSystem::exists(std::string_view const user_name) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "SELECT * FROM users WHERE user_name = ?");
            statement.bind(1, std::string(user_name));
            return statement.executeStep();
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("login")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto LoginSystem::login_account(std::string_view const user_name, std::string_view const v,
                                    uint64_t const session_id) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "SELECT id FROM users WHERE user_name = ? and v = ?");
            statement.bind(1, std::string(user_name));
            statement.bind(2, std::string(v));
            if (statement.executeStep())
            {
                m_auth_sessions[session_id].second = statement.getColumn(0).getInt64();
                return true;
            }
            else
            {
                return false;
            }
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("login")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto LoginSystem::register_account(std::string_view const user_name, std::string_view const v,
                                       uint64_t& user_id) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "INSERT INTO users (user_name, v) VALUES (?, ?) RETURNING id");
            statement.bind(1, std::string(user_name));
            statement.bind(2, std::string(v));
            if (statement.executeStep())
            {
                user_id = statement.getColumn(0).getInt64();
            }
            return true;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("login")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto LoginSystem::login_session(uint64_t const session_id) -> void
    {
        m_auth_sessions[session_id].first = true;
    }

    auto LoginSystem::initialize_session(uint64_t const session_id) -> void
    {
        m_auth_sessions[session_id] = {false, std::numeric_limits<uint64_t>::max()};
    }

    auto LoginSystem::logout_session(uint64_t const session_id) -> void
    {
        m_auth_sessions[session_id].first = false;
    }

    auto LoginSystem::close_session(uint64_t const session_id) -> void
    {
        m_auth_sessions.erase(session_id);
    }

    auto LoginSystem::auth_session(uint64_t const session_id) const -> bool
    {
        return m_auth_sessions.at(session_id).first;
    }

    auto LoginSystem::user_id(uint64_t const session_id) const -> uint64_t
    {
        return m_auth_sessions.at(session_id).second;
    }
} // namespace exchange::modules