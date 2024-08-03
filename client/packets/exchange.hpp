#pragma once

#include "packet.hpp"

namespace exchange::packets
{
    class MakeRequestPacket : public Packet
    {
      public:
        MakeRequestPacket(std::string_view const currency, float const amount,
                          float const price, uint32_t const request_type, bool& successful);

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;

      private:
        std::string_view m_currency;
        float m_amount;
        float m_price;
        uint32_t m_request_type;

        bool* m_successful;
    };
} // namespace exchange::packets