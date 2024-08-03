#include "exchange.hpp"
#include "precompiled.hpp"

namespace exchange::packets
{
    MakeRequestPacket::MakeRequestPacket(std::string_view const currency, float const amount,
                                         float const price, uint32_t const request_type, bool& successful)
        : Packet(core::RequestMessageType::MakeRequest), m_currency(currency), m_amount(amount),
          m_price(price), m_request_type(request_type), m_successful(&successful)
    {
    }

    auto MakeRequestPacket::accept(nlohmann::json const& payload) -> void
    {
        auto error_code = static_cast<core::ErrorCode>(payload["error_code"].get<uint16_t>());
        if (error_code != core::ErrorCode::Success)
        {
            *m_successful = false;
            return;
        }

        *m_successful = true;
    }

    auto MakeRequestPacket::send(nlohmann::json& payload) -> void
    {
        payload["currency"] = m_currency;
        payload["amount"] = m_amount;
        payload["price"] = m_price;
        payload["request_type"] = m_request_type;
    }
} // namespace exchange::packets