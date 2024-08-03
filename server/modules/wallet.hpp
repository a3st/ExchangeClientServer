#pragma once

#include "core/json.hpp"
#include <SQLiteCpp/SQLiteCpp.h>

namespace exchange::modules
{
    enum class WalletTransactionType : uint32_t
    {
        Withdraw,
        Deposit
    };

    struct WalletInfo
    {
        uint64_t id;
        std::string currency;
        float amount;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(exchange::modules::WalletInfo, id, currency, amount)

    class Wallet
    {
      public:
        Wallet(SQLite::Database& database, std::optional<std::filesystem::path> const log_path);

        ~Wallet();

        auto create_wallet(uint64_t const user_id, std::string_view const currency, uint64_t& wallet_id) -> bool;

        auto make_transaction(uint64_t const wallet_id, float const amount,
                              WalletTransactionType const transaction_type, std::string_view const description) -> bool;

        auto wallets(uint64_t const user_id) -> std::optional<std::vector<WalletInfo>>;

      private:
        SQLite::Database* m_database;
    };
} // namespace exchange::modules