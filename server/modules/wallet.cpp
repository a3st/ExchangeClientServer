#include "wallet.hpp"
#include "precompiled.hpp"

namespace exchange::modules
{
    Wallet::Wallet(SQLite::Database& database, std::optional<std::filesystem::path> const log_path)
        : m_database(&database)
    {
        std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
        if (log_path)
        {
            sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.value().string()));
        }
        auto logger = std::make_shared<spdlog::logger>("wallet", sinks.begin(), sinks.end());
        spdlog::initialize_logger(logger);

        if (!database.tableExists("wallets"))
        {
            try
            {
                SQLite::Statement statement(
                    *m_database, "CREATE TABLE wallets (id INTEGER PRIMARY KEY, user_id INTEGER, currency TEXT)");
                statement.exec();
            }
            catch (SQLite::Exception e)
            {
                spdlog::get("wallet")->log(spdlog::level::critical, e.what());
                std::exit(EXIT_FAILURE);
            }
        }

        if (!database.tableExists("transactions"))
        {
            try
            {
                SQLite::Statement statement(*m_database,
                                            "CREATE TABLE transactions (id INTEGER PRIMARY KEY, wallet_id "
                                            "INTEGER, amount REAL, transaction_type INTEGER, description TEXT)");
                statement.exec();
            }
            catch (SQLite::Exception e)
            {
                spdlog::get("wallet")->log(spdlog::level::critical, e.what());
                std::exit(EXIT_FAILURE);
            }
        }
    }

    Wallet::~Wallet()
    {
        spdlog::drop("wallet");
    }

    auto Wallet::create_wallet(uint64_t const user_id, std::string_view const currency, uint64_t& wallet_id) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database,
                                        "INSERT INTO wallets (user_id, currency) VALUES (?, ?) RETURNING id");
            statement.bind(1, static_cast<int64_t>(user_id));
            statement.bind(2, std::string(currency));
            if (statement.executeStep())
            {
                wallet_id = statement.getColumn(0).getInt64();
            }
            return true;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("wallet")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto Wallet::make_transaction(uint64_t const wallet_id, float const amount,
                                  WalletTransactionType const transaction_type,
                                  std::string_view const description) -> bool
    {
        try
        {
            SQLite::Statement statement(*m_database, "INSERT INTO transactions (wallet_id, amount, "
                                                     "transaction_type, description) VALUES (?, ?, ?, ?)");
            statement.bind(1, static_cast<int64_t>(wallet_id));
            statement.bind(2, amount);
            statement.bind(3, static_cast<uint32_t>(transaction_type));
            statement.bind(4, std::string(description));
            return statement.exec() > 0;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("wallet")->log(spdlog::level::err, e.what());
            return false;
        }
    }

    auto Wallet::wallets(uint64_t const user_id) -> std::optional<std::vector<WalletInfo>>
    {
        try
        {
            std::vector<WalletInfo> wallet_infos;
            {
                SQLite::Statement statement(*m_database, "SELECT id, currency FROM wallets WHERE user_id = ?");
                statement.bind(1, static_cast<int64_t>(user_id));

                while (statement.executeStep())
                {
                    WalletInfo wallet_info{.id = static_cast<uint64_t>(statement.getColumn(0).getInt64()),
                                           .currency = statement.getColumn(1).getString()};
                    wallet_infos.emplace_back(std::move(wallet_info));
                }
            }

            for (auto& wallet_info : wallet_infos)
            {
                float amount = 0.0f;

                {
                    SQLite::Statement statement(
                        *m_database,
                        "SELECT SUM(amount) FROM transactions WHERE wallet_id = ? and transaction_type = 1");
                    statement.bind(1, static_cast<int64_t>(wallet_info.id));

                    if (statement.executeStep())
                    {
                        amount = statement.getColumn(0).getDouble();
                    }
                }

                {
                    SQLite::Statement statement(
                        *m_database,
                        "SELECT SUM(amount) FROM transactions WHERE wallet_id = ? and transaction_type = 0");
                    statement.bind(1, static_cast<int64_t>(wallet_info.id));

                    if (statement.executeStep())
                    {
                        amount -= statement.getColumn(0).getDouble();
                    }
                }

                wallet_info.amount = amount;
            }
            return wallet_infos;
        }
        catch (SQLite::Exception e)
        {
            spdlog::get("wallet")->log(spdlog::level::err, e.what());
            return std::nullopt;
        }
    }
} // namespace exchange::modules