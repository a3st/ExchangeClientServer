#pragma once

#include "packet.hpp"

namespace exchange::packets
{
    struct WalletInfo
    {
        uint64_t id;
        std::string currency;
        float amount;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(exchange::packets::WalletInfo, id, currency, amount)

    class WalletListPacket : public Packet
    {
      public:
        WalletListPacket(std::vector<WalletInfo>& wallet_infos);

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;

      private:
        std::vector<WalletInfo>* m_wallet_infos;
    };
} // namespace exchange::packets