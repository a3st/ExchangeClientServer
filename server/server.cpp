#include "server.hpp"
#include "precompiled.hpp"

namespace exchange
{
    Server::Server(uint32_t const port, std::filesystem::path const& log_path)
        : m_acceptor(m_io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
          m_session_index(0), m_core(log_path)
    {
        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                std::filesystem::path(log_path).make_preferred().string()),
        };
        auto logger = std::make_shared<spdlog::logger>("server", sinks.begin(), sinks.end());
        logger->set_level(spdlog::get_level());
        spdlog::register_logger(logger);

        spdlog::get("server")->log(spdlog::level::info, "Server starting...");
    }

    auto Server::run() -> void
    {
        spdlog::get("server")->log(spdlog::level::info, "Server is running! ::{}", m_acceptor.local_endpoint().port());

        m_core.start();

        this->accept_connection();
        m_io_context.run();
    }

    auto Server::accept_connection() -> void
    {
        m_acceptor.async_accept([this](boost::system::error_code const& error, boost::asio::ip::tcp::socket&& socket) {
            if (!error)
            {
                auto remote_endpoint = socket.remote_endpoint();

                auto session = std::make_shared<Session>(std::move(socket), m_session_index);
                session->on_connected = [remote_endpoint, this](uint64_t const session_id) {
                    spdlog::get("server")->log(spdlog::level::trace, "Client {}:{} is connected",
                                               remote_endpoint.address().to_string(), remote_endpoint.port());
                    this->m_core.on_session_connected(session_id);
                };
                session->on_closed = [remote_endpoint, this](uint64_t const session_id) {
                    spdlog::get("server")->log(spdlog::level::trace, "Client {}:{} is disconnected",
                                               remote_endpoint.address().to_string(), remote_endpoint.port());
                    this->m_core.on_session_closed(session_id);
                    this->close_connection(session_id);
                };
                session->on_message = [this](uint64_t const session_id,
                                             std::span<uint8_t const> const buffer) -> Response {
                    return this->m_core.on_message(session_id, buffer);
                };
                session->start();

                m_sessions[m_session_index++] = std::move(session);

                this->accept_connection();
            }
        });
    }

    auto Server::close_connection(uint64_t const session_id) -> void
    {
        m_sessions.erase(session_id);
    }
} // namespace exchange