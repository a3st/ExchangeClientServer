#pragma once

#include "core/common.hpp"
#include "core/json.hpp"

namespace exchange
{
    class Packet
    {
      public:
        Packet(core::RequestMessageType const message_type);

        auto process(boost::asio::ip::tcp::socket& socket) -> bool;

      protected:
        virtual auto accept(nlohmann::json const& payload) -> void = 0;

        virtual auto send(nlohmann::json& payload) -> void = 0;

      private:
        core::RequestMessageType m_message_type;
    };
} // namespace exchange