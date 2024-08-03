#pragma once

#include "core/common.hpp"
#include "core/json.hpp"

namespace exchange
{
    class Response
    {
        friend class Session;

      public:
        Response(core::RequestMessageType const message_type, std::optional<nlohmann::json> payload);

      private:
        core::RequestMessageType m_message_type;
        nlohmann::json m_payload;
    };

    class Session
    {
      public:
        Session(boost::asio::ip::tcp::socket&& socket, uint64_t const sessionID);

        Session(Session const& other) = delete;

        Session(Session&& other);

        auto operator=(Session const& other) -> Session& = delete;

        auto operator=(Session&& other) -> Session&;

        auto start() -> void;

        uint64_t session_id() const;

        std::function<void(uint64_t const)> on_connected;

        std::function<void(uint64_t const)> on_closed;

        std::function<Response(uint64_t const, std::span<uint8_t const> const)> on_message;

      private:
        uint64_t m_session_id;
        boost::asio::ip::tcp::socket m_socket;
        std::vector<uint8_t> m_read_buffer;
        std::vector<uint8_t> m_write_buffer;

        auto read_socket() -> void;

        auto write_socket() -> void;
    };
} // namespace exchange