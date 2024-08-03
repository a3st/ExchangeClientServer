#include "session.hpp"
#include "core/json.hpp"
#include "precompiled.hpp"

namespace exchange
{
    Response::Response(core::RequestMessageType const message_type, std::optional<nlohmann::json> payload)
        : m_message_type(message_type), m_payload(payload.value_or(nlohmann::json()))
    {
    }

    Session::Session(boost::asio::ip::tcp::socket&& socket, uint64_t const m_session_id)
        : m_socket(std::move(socket)), m_write_buffer(2048), m_read_buffer(2048), m_session_id(m_session_id)
    {
    }

    Session::Session(Session&& other)
        : m_socket(std::move(other.m_socket)), m_write_buffer(std::move(other.m_write_buffer)),
          m_read_buffer(std::move(other.m_read_buffer)), m_session_id(other.m_session_id)
    {
    }

    auto Session::operator=(Session&& other) -> Session&
    {
        m_socket = std::move(other.m_socket);
        m_write_buffer = std::move(other.m_write_buffer);
        m_read_buffer = std::move(other.m_read_buffer);
        m_session_id = other.m_session_id;
        return *this;
    }

    uint64_t Session::session_id() const
    {
        return m_session_id;
    }

    auto Session::start() -> void
    {
        if (on_connected)
        {
            on_connected(m_session_id);
        }

        this->read_socket();
    }

    auto Session::read_socket() -> void
    {
        m_socket.async_read_some(boost::asio::buffer(m_read_buffer),
                                 [this](boost::system::error_code const& error, size_t const size) -> void {
                                     if (!error)
                                     {
                                         if (on_message)
                                         {
                                             Response response = this->on_message(
                                                 m_session_id, std::span<uint8_t>(m_read_buffer.data(), size));

                                             nlohmann::json packet;
                                             packet["type"] = static_cast<uint16_t>(response.m_message_type);
                                             packet["payload"] = response.m_payload;

                                             std::string const request = packet.dump();
                                             m_write_buffer.assign(request.begin(), request.end());
                                         }

                                         this->write_socket();
                                     }
                                     else
                                     {
                                         this->on_closed(m_session_id);
                                     }
                                 });
    }

    auto Session::write_socket() -> void
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(m_write_buffer),
                                 [this](boost::system::error_code const& error, size_t const size) -> void {
                                     if (!error)
                                     {
                                         this->read_socket();
                                     }
                                     else
                                     {
                                         this->on_closed(m_session_id);
                                     }
                                 });
    }
} // namespace exchange