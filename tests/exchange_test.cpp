#include "modules/exchange.hpp"
#include "modules/wallet.hpp"
#include "precompiled.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <gtest/gtest.h>

using namespace exchange;

TEST(Exchange, MakeRequest_Test)
{
    SQLite::Database test_db("test.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS requests");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Exchange exchange(test_db, std::nullopt);

    ASSERT_TRUE(exchange.make_request(1, "USD/RUB", 50, 62, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(2, "USD/RUB", 40, 70, modules::RequestType::Sell));
    ASSERT_TRUE(exchange.make_request(3, "USD/RUB", 120, 100, modules::RequestType::Sell));

    {
        SQLite::Statement statement(test_db, "SELECT currency, amount, price, request_type FROM requests WHERE id = 1");
        ASSERT_TRUE(statement.executeStep());
        ASSERT_EQ(statement.getColumn(0).getString(), "USD/RUB");
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);
        ASSERT_EQ(statement.getColumn(2).getDouble(), 62);
        ASSERT_EQ(statement.getColumn(3).getUInt(), static_cast<uint32_t>(modules::RequestType::Buy));
    }
}

TEST(Exchange, RemoveRequest_Test)
{
    SQLite::Database test_db("test.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS requests");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Exchange exchange(test_db, std::nullopt);

    ASSERT_TRUE(exchange.make_request(1, "USD/RUB", 50, 62, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(2, "USD/RUB", 40, 70, modules::RequestType::Sell));
    ASSERT_TRUE(exchange.make_request(3, "USD/RUB", 120, 100, modules::RequestType::Sell));

    {
        SQLite::Statement statement(test_db, "SELECT * FROM requests");
        for (uint32_t const i : std::views::iota(0u, 3u))
        {
            ASSERT_TRUE(statement.executeStep());
        }
    }

    ASSERT_TRUE(exchange.remove_request(1));

    {
        SQLite::Statement statement(test_db, "SELECT * FROM requests");
        for (uint32_t const i : std::views::iota(0u, 2u))
        {
            ASSERT_TRUE(statement.executeStep());
        }
    }
}

TEST(Exchange, ProcessEqRequests_Test)
{
    SQLite::Database test_db("test.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS requests");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Exchange exchange(test_db, std::nullopt);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS wallets");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS transactions");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Wallet wallet(test_db, std::nullopt);

    for (uint32_t const i : std::views::iota(1u, 6u))
    {
        uint64_t new_wallet_id;
        ASSERT_TRUE(wallet.create_wallet(i, "RUB", new_wallet_id));
        ASSERT_TRUE(wallet.create_wallet(i, "USD", new_wallet_id));
    }

    ASSERT_TRUE(exchange.make_request(1, "USD/RUB", 50, 62, modules::RequestType::Sell));
    ASSERT_TRUE(exchange.make_request(2, "USD/RUB", 50, 63, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(3, "USD/RUB", 50, 64, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(4, "USD/RUB", 50, 60, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(5, "USD/RUB", 50, 61, modules::RequestType::Sell));

    exchange.process_requests(wallet);

    // Withdraw Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 0");

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 64);

        // User (id: 5)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 5);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 63);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);
    }

    // Deposit Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 1");

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);

        // User (id: 5)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 5);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 64);

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 63);
    }

    // Wallet Testing

    // User (id: 1)
    {
        auto const wallets = wallet.wallets(1).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, 3150.0f);
        ASSERT_EQ(USD_wallet->amount, -50.0f);
    }

    // User (id: 2)
    {
        auto const wallets = wallet.wallets(2).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -3150.0f);
        ASSERT_EQ(USD_wallet->amount, 50.0f);
    }

    // User (id: 3)
    {
        auto const wallets = wallet.wallets(3).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -3200.0f);
        ASSERT_EQ(USD_wallet->amount, 50.0f);
    }

    // User (id: 4)
    {
        auto const wallets = wallet.wallets(4).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, 0.0f);
        ASSERT_EQ(USD_wallet->amount, 0.0f);
    }

    // User (id: 5)
    {
        auto const wallets = wallet.wallets(5).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, 3200.0f);
        ASSERT_EQ(USD_wallet->amount, -50.0f);
    }
}

TEST(Exchange, ProcessNotEqRequests_Test)
{
    SQLite::Database test_db("test.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS requests");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Exchange exchange(test_db, std::nullopt);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS wallets");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS transactions");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Wallet wallet(test_db, std::nullopt);

    for (uint32_t const i : std::views::iota(1u, 5u))
    {
        uint64_t new_wallet_id;
        ASSERT_TRUE(wallet.create_wallet(i, "RUB", new_wallet_id));
        ASSERT_TRUE(wallet.create_wallet(i, "USD", new_wallet_id));
    }

    ASSERT_TRUE(exchange.make_request(1, "USD/RUB", 100, 62, modules::RequestType::Sell));
    ASSERT_TRUE(exchange.make_request(2, "USD/RUB", 50, 63, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(3, "USD/RUB", 40, 64, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(4, "USD/RUB", 50, 62, modules::RequestType::Buy));

    exchange.process_requests(wallet);

    // Withdraw Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 0");

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 40 * 64);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 40);

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 63);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);

        // User (id: 4)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 4);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10 * 62);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10);
    }

    // Deposit Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 1");

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 40);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 40 * 64);

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 50 * 63);

        // User (id: 4)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 4);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10 * 62);
    }

    // Wallet Testing

    // User (id: 1)
    {
        auto const wallets = wallet.wallets(1).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, 6330.0f);
        ASSERT_EQ(USD_wallet->amount, -100.0f);
    }

    // User (id: 2)
    {
        auto const wallets = wallet.wallets(2).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -3150.0f);
        ASSERT_EQ(USD_wallet->amount, 50.0f);
    }

    // User (id: 3)
    {
        auto const wallets = wallet.wallets(3).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -2560.0f);
        ASSERT_EQ(USD_wallet->amount, 40.0f);
    }

    // User (id: 4)
    {
        auto const wallets = wallet.wallets(4).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -620.0f);
        ASSERT_EQ(USD_wallet->amount, 10.0f);
    }
}

TEST(Exchange, ProcessPartialRequests_Test)
{
    SQLite::Database test_db("test.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS requests");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Exchange exchange(test_db, std::nullopt);

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS wallets");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    {
        SQLite::Statement statement(test_db, "DROP TABLE IF EXISTS transactions");
        ASSERT_EQ(statement.exec(), SQLite::OK);
    }

    modules::Wallet wallet(test_db, std::nullopt);

    for (uint32_t const i : std::views::iota(1u, 5u))
    {
        uint64_t new_wallet_id;
        ASSERT_TRUE(wallet.create_wallet(i, "RUB", new_wallet_id));
        ASSERT_TRUE(wallet.create_wallet(i, "USD", new_wallet_id));
    }

    ASSERT_TRUE(exchange.make_request(1, "USD/RUB", 10, 62, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(2, "USD/RUB", 20, 63, modules::RequestType::Buy));
    ASSERT_TRUE(exchange.make_request(3, "USD/RUB", 50, 61, modules::RequestType::Sell));

    exchange.process_requests(wallet);

    // Withdraw Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 0");

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 63 * 20);

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 20);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 62 * 10);

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10);
    }

    // Deposit Testing
    {
        SQLite::Statement statement(
            test_db, "SELECT wallets.user_id AS user_id, amount FROM transactions "
                     "INNER JOIN wallets ON transactions.wallet_id = wallets.id WHERE transaction_type = 1");

        // User (id: 2)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 2);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 20);

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 63 * 20);

        // User (id: 1)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 1);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 10);

        // User (id: 3)
        ASSERT_TRUE(statement.executeStep());

        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 62 * 10);
    }

    // Requests Testing
    {
        SQLite::Statement statement(test_db, "SELECT user_id, amount FROM requests");

        // Request (id: 3, user_id: 3)
        ASSERT_TRUE(statement.executeStep());
        ASSERT_EQ(statement.getColumn(0).getInt64(), 3);
        ASSERT_EQ(statement.getColumn(1).getDouble(), 20);
    }

    // Wallet Testing

    // User (id: 1)
    {
        auto const wallets = wallet.wallets(1).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -620.0f);
        ASSERT_EQ(USD_wallet->amount, 10.0f);
    }

    // User (id: 2)
    {
        auto const wallets = wallet.wallets(2).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, -1260.0f);
        ASSERT_EQ(USD_wallet->amount, 20.0f);
    }

    // User (id: 3)
    {
        auto const wallets = wallet.wallets(3).value();

        auto RUB_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("RUB") == 0; });

        auto USD_wallet = std::find_if(wallets.begin(), wallets.end(),
                                       [&](auto const& element) { return element.currency.compare("USD") == 0; });

        ASSERT_EQ(RUB_wallet->amount, 1880.0f);
        ASSERT_EQ(USD_wallet->amount, -30.0f);
    }
}

auto main(int32_t argc, char** argv) -> int32_t
{
    spdlog::set_level(spdlog::level::debug);

    testing::InitGoogleTest(&argc, argv);
    return ::RUN_ALL_TESTS();
}