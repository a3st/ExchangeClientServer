#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

namespace exchange::modules
{
    enum class RequestType : uint32_t
    {
        Buy,
        Sell
    };

    class Wallet;

    class Exchange
    {
      public:
        Exchange(SQLite::Database& database, std::optional<std::filesystem::path> const log_path);

        ~Exchange();

        auto make_request(uint64_t const user_id, std::string_view const currency, float const amount,
                          float const price, RequestType const request_type) -> bool;

        auto remove_request(uint64_t const request_id) -> bool;

        auto process_requests(Wallet& wallet) -> void;

      private:
        SQLite::Database* m_database;

        struct RequestSideInfo
        {
            uint64_t request_id;
            uint64_t user_id;
            float amount;
            float price;
            std::string currency;
        };

        auto request_step(Wallet& wallet, SQLite::Transaction& transaction, RequestSideInfo const& buyer_info,
                          RequestSideInfo const& seller_info) -> void;
    };
} // namespace exchange::modules