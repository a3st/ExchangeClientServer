#include "exchange.hpp"
#include "precompiled.hpp"
#include "wallet.hpp"

namespace exchange::modules
{
    Exchange::Exchange(SQLite::Database& database, std::optional<std::filesystem::path> const log_path)
        : m_database(&database)
    {
        std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
        if (log_path)
        {
            sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.value().string()));
        }
        auto logger = std::make_shared<spdlog::logger>("exchange", sinks.begin(), sinks.end());
        spdlog::initialize_logger(logger);

        if (!database.tableExists("requests"))
        {
            try
            {
                SQLite::Statement statement(*m_database,
                                            "CREATE TABLE requests (id INTEGER PRIMARY KEY, user_id INTEGER, "
                                            "currency TEXT, amount REAL, price REAL, request_type INTEGER)");
                statement.exec();
            }
            catch (SQLite::Exception e)
            {
                spdlog::get("exchange")->log(spdlog::level::critical, e.what());
                std::exit(EXIT_FAILURE);
            }
        }
    }

    Exchange::~Exchange()
    {
        spdlog::drop("exchange");
    }

    auto Exchange::make_request(uint64_t const user_id, std::string_view const currency, float const amount,
                                float const price, RequestType const request_type) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "INSERT INTO requests (user_id, currency, amount, "
                                                     "price, request_type) VALUES (?, ?, ?, ?, ?)");
            statement.bind(1, static_cast<int64_t>(user_id));
            statement.bind(2, std::string(currency));
            statement.bind(3, amount);
            statement.bind(4, price);
            statement.bind(5, static_cast<uint32_t>(request_type));
            return statement.exec() > 0;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("exchange")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto Exchange::remove_request(uint64_t const request_id) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "DELETE FROM requests WHERE id = ?");
            statement.bind(1, static_cast<int64_t>(request_id));
            return statement.exec() > 0;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("exchange")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto Exchange::process_requests(Wallet& wallet) -> void
    {
        try
        {
            SQLite::Statement buyers_statement(*m_database, "SELECT id, user_id, amount, price, currency FROM requests "
                                                            "WHERE request_type = 0 ORDER BY price DESC, id ASC");

            while (buyers_statement.executeStep())
            {
                auto const buyer_request_id = static_cast<uint64_t>(buyers_statement.getColumn(0).getInt64());
                auto const buyer_user_id = static_cast<uint64_t>(buyers_statement.getColumn(1).getInt64());
                float const buyer_amount = buyers_statement.getColumn(2).getDouble();

                float const price = buyers_statement.getColumn(3).getDouble();
                std::string const currency = buyers_statement.getColumn(4).getString();

                std::vector<std::string> currencies;
                boost::split(currencies, currency, boost::is_any_of("/"));

                bool is_partial = false;
                {
                    SQLite::Statement sellers_statement(
                        *m_database,
                        "SELECT id, user_id, amount FROM requests WHERE request_type = 1 "
                        "and price <= ? "
                        "and amount >= ? and currency = ? and user_id != ? ORDER BY price ASC, id ASC LIMIT 1");
                    sellers_statement.bind(1, price);
                    sellers_statement.bind(2, buyer_amount);
                    sellers_statement.bind(3, currency);
                    sellers_statement.bind(4, static_cast<int64_t>(buyer_user_id));

                    if (sellers_statement.executeStep())
                    {
                        auto const seller_request_id = static_cast<uint64_t>(sellers_statement.getColumn(0).getInt64());
                        auto const seller_user_id = static_cast<uint64_t>(sellers_statement.getColumn(1).getInt64());
                        float const seller_amount = sellers_statement.getColumn(2).getDouble();

                        RequestSideInfo buyer_info{.request_id = buyer_request_id,
                                                   .user_id = buyer_user_id,
                                                   .amount = buyer_amount,
                                                   .currency = currencies[0]};
                        RequestSideInfo seller_info{.request_id = seller_request_id,
                                                    .user_id = seller_user_id,
                                                    .amount = seller_amount,
                                                    .currency = currencies[1]};
                        SQLite::Transaction transaction(*m_database);
                        this->request_step(wallet, transaction, buyer_info, seller_info, price);
                    }
                    else
                    {
                        is_partial = true;
                    }
                }

                if (is_partial)
                {
                    SQLite::Statement sellers_statement(
                        *m_database, "SELECT id, user_id, amount FROM requests WHERE request_type = 1 "
                                     "and price <= ? "
                                     "and amount < ? and currency = ? and user_id != ? ORDER BY price ASC, id ASC");
                    sellers_statement.bind(1, price);
                    sellers_statement.bind(2, buyer_amount);
                    sellers_statement.bind(3, currency);
                    sellers_statement.bind(4, static_cast<int64_t>(buyer_user_id));

                    float diff_amount = buyer_amount;

                    while (sellers_statement.executeStep() && diff_amount > 0)
                    {
                        auto const seller_request_id = static_cast<uint64_t>(sellers_statement.getColumn(0).getInt64());
                        auto const seller_user_id = static_cast<uint64_t>(sellers_statement.getColumn(1).getInt64());
                        float const seller_amount = sellers_statement.getColumn(2).getDouble();

                        RequestSideInfo buyer_info{.request_id = buyer_request_id,
                                                   .user_id = buyer_user_id,
                                                   .amount = diff_amount,
                                                   .currency = currencies[0]};
                        RequestSideInfo seller_info{.request_id = seller_request_id,
                                                    .user_id = seller_user_id,
                                                    .amount = seller_amount,
                                                    .currency = currencies[1]};
                        SQLite::Transaction transaction(*m_database);
                        this->request_step(wallet, transaction, buyer_info, seller_info, price);
                        diff_amount -= seller_amount;
                    }
                }
            }
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("exchange")->log(spdlog::level::err, e.what());
            return;
        }
    }

    auto Exchange::request_step(Wallet& wallet, SQLite::Transaction& transaction, RequestSideInfo const& buyer_info,
                                RequestSideInfo const& seller_info, float const price) -> void
    {
        auto const buyer_wallets = wallet.wallets(buyer_info.user_id).value();

        auto buyer_wallet_from = std::find_if(buyer_wallets.begin(), buyer_wallets.end(), [&](auto const& element) {
            return element.currency.compare(seller_info.currency) == 0;
        });

        auto buyer_wallet_to = std::find_if(buyer_wallets.begin(), buyer_wallets.end(), [&](auto const& element) {
            return element.currency.compare(buyer_info.currency) == 0;
        });

        auto const seller_wallets = wallet.wallets(seller_info.user_id).value();

        auto seller_wallet_from = std::find_if(seller_wallets.begin(), seller_wallets.end(), [&](auto const& element) {
            return element.currency.compare(buyer_info.currency) == 0;
        });

        auto seller_wallet_to = std::find_if(seller_wallets.begin(), seller_wallets.end(), [&](auto const& element) {
            return element.currency.compare(seller_info.currency) == 0;
        });

        bool const buyer_amount_beq = buyer_info.amount >= seller_info.amount;
        float const diff_amount = buyer_amount_beq ? seller_info.amount : buyer_info.amount;

        if (!wallet.make_transaction(buyer_wallet_from->id, diff_amount * price, WalletTransactionType::Withdraw,
                                     "Exchange actions"))
        {
            transaction.rollback();
            return;
        }

        if (!wallet.make_transaction(buyer_wallet_to->id, diff_amount, WalletTransactionType::Deposit,
                                     "Exchange actions"))
        {
            transaction.rollback();
            return;
        }

        if (!wallet.make_transaction(seller_wallet_from->id, diff_amount, WalletTransactionType::Withdraw,
                                     "Exchange actions"))
        {
            transaction.rollback();
            return;
        }

        if (!wallet.make_transaction(seller_wallet_to->id, diff_amount * price, WalletTransactionType::Deposit,
                                     "Exchange actions"))
        {
            transaction.rollback();
            return;
        }

        {
            SQLite::Statement statement(*m_database, "DELETE FROM requests WHERE id = ?");
            statement.bind(1, static_cast<int64_t>(buyer_info.request_id));
            if (statement.exec() == 0)
            {
                transaction.rollback();
                return;
            }
        }

        if (buyer_amount_beq)
        {
            SQLite::Statement statement(*m_database, "DELETE FROM requests WHERE id = ?");
            statement.bind(1, static_cast<int64_t>(seller_info.request_id));
            if (statement.exec() == 0)
            {
                transaction.rollback();
                return;
            }
        }
        else
        {
            SQLite::Statement statement(*m_database, "UPDATE requests SET amount = ? WHERE id = ?");
            statement.bind(1, seller_info.amount - diff_amount);
            statement.bind(2, static_cast<int64_t>(seller_info.request_id));
            if (statement.exec() == 0)
            {
                transaction.rollback();
                return;
            }
        }

        transaction.commit();

        spdlog::get("exchange")
            ->log(spdlog::level::debug,
                  "Request from user_id: {} completed: currency: {}/{}, type: buy, "
                  "amount: {}, price: {}",
                  buyer_info.user_id, buyer_info.currency, seller_info.currency, diff_amount, price);

        spdlog::get("exchange")
            ->log(spdlog::level::debug,
                  "Request from user_id: {} completed: currency: {}/{}, type: sell, "
                  "amount: {}, price: {}",
                  seller_info.user_id, buyer_info.currency, seller_info.currency, diff_amount, price);
    }
} // namespace exchange::modules