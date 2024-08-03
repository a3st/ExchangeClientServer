#include "wallet.hpp"
#include "precompiled.hpp"

namespace exchange::packets
{
    WalletListPacket::WalletListPacket(std::vector<WalletInfo>& wallet_infos)
        : Packet(core::RequestMessageType::WalletList), m_wallet_infos(&wallet_infos)
    {
    }

    auto WalletListPacket::accept(nlohmann::json const& payload) -> void
    {
        auto error_code = static_cast<core::ErrorCode>(payload["error_code"].get<uint16_t>());
        if (error_code != core::ErrorCode::Success)
        {
            return;
        }

        *m_wallet_infos = payload["wallets"];
    }

    auto WalletListPacket::send(nlohmann::json& payload) -> void
    {
    }
} // namespace exchange::packets