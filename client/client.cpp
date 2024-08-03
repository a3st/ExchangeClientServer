#include "client.hpp"
#include "packets/exchange.hpp"
#include "packets/login.hpp"
#include "packets/wallet.hpp"
#include "precompiled.hpp"

namespace exchange
{
    enum class MenuType
    {
        Login,
        Account
    };

    std::vector<uint8_t> salt{202, 2, 57, 19, 34, 151, 47, 212, 76, 240, 117, 65, 147, 73, 219, 123};

    Client::Client(std::string_view const address, uint32_t const port) : m_socket(m_io_context)
    {
        m_socket.connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(std::string(address)), port));
    }

    auto Client::run() -> void
    {
        bool running = true;
        MenuType menu_type = MenuType::Login;

        while (running)
        {
            switch (menu_type)
            {
                case MenuType::Login: {
                    std::cout << "Menu:\n"
                                 "1) Register\n"
                                 "2) Login\n"
                                 "3) Exit\n"
                              << std::endl;

                    uint16_t menu_option;
                    std::cout << "Select: ";
                    std::cin >> menu_option;

                    switch (menu_option)
                    {
                        case 1: {
                            std::string user_name;
                            std::cout << "Type your login: ";
                            std::cin >> user_name;

                            std::string password;
                            std::cout << "Type your password: ";
                            std::cin >> password;

                            bool registered;
                            {
                                auto packet =
                                    std::make_unique<packets::RegisterPacket>(user_name, password, salt, registered);
                                if (!packet->process(m_socket))
                                {
                                    std::cout << "\nUnknown response from server\n" << std::endl;
                                    break;
                                }
                            }
                            if (!registered)
                            {
                                std::cout << "\nA user with the same login already exists\n" << std::endl;
                                break;
                            }

                            std::cout << "\nThe account has been registered\n" << std::endl;
                            break;
                        }

                        case 2: {
                            std::string user_name;
                            std::cout << "Type your login: ";
                            std::cin >> user_name;

                            std::string password;
                            std::cout << "Type your password: ";
                            std::cin >> password;

                            std::string B;
                            {
                                auto packet =
                                    std::make_unique<packets::LoginChallangePacket>(user_name, password, salt, B);
                                if (!packet->process(m_socket))
                                {
                                    std::cout << "\nUnknown response from server\n" << std::endl;
                                    break;
                                }
                            }
                            if (B.empty())
                            {
                                std::cout << "\nThe entered login or password is incorrect\n" << std::endl;
                                break;
                            }

                            bool auth;
                            {
                                auto packet = std::make_unique<packets::LoginChallangeProofPacket>(user_name, password,
                                                                                                   B, salt, auth);
                                if (!packet->process(m_socket))
                                {
                                    std::cout << "\nUnknown response from server\n" << std::endl;
                                    break;
                                }
                            }
                            if (!auth)
                            {
                                std::cout << "\nThe entered login or password is incorrect\n" << std::endl;
                                break;
                            }

                            std::cout << "\nWelcome, " << user_name << "!\n" << std::endl;
                            menu_type = MenuType::Account;
                            break;
                        }
                        case 3: {
                            running = false;
                            break;
                        }
                        default: {
                            std::cout << "\nUnknown menu option\n" << std::endl;
                            break;
                        }
                    }
                    break;
                }

                case MenuType::Account: {
                    std::cout << "Account Menu:\n"
                                 "1) My Wallet\n"
                                 "2) Make Request\n"
                                 "3) Logout\n"
                              << std::endl;

                    uint16_t menu_option;
                    std::cout << "Select: ";
                    std::cin >> menu_option;

                    switch (menu_option)
                    {
                        case 1: {
                            std::vector<packets::WalletInfo> wallet_infos;
                            {
                                auto packet = std::make_unique<packets::WalletListPacket>(wallet_infos);
                                if (!packet->process(m_socket))
                                {
                                    std::cout << "\nUnknown response from server\n" << std::endl;
                                    break;
                                }
                            }
                            if (wallet_infos.empty())
                            {
                                std::cout << "\nNo wallets available\n" << std::endl;
                                break;
                            }
                            std::cout << "\nMy Wallet\n" << std::endl;
                            std::cout << std::format("{:10} {:8}", "Currency", "Amount") << std::endl;

                            for (auto const& wallet_info : wallet_infos)
                            {
                                std::cout << std::format("{:10} {:8}", wallet_info.currency, wallet_info.amount)
                                          << std::endl;
                            }

                            std::cout << std::endl;
                            break;
                        }
                        case 2: {
                            float amount;
                            std::cout << "Type amount: ";
                            std::cin >> amount;

                            float price;
                            std::cout << "Type price (per unit): ";
                            std::cin >> price;

                            uint32_t request_type;
                            while (true)
                            {
                                std::cout << "Type '0' if you want to Buy or '1' if you want to Sell: ";
                                std::cin >> request_type;

                                if (request_type == 0 || request_type == 1)
                                {
                                    break;
                                }
                            }

                            std::cout << std::format("Request to {} {} USD for {} ({}) RUB",
                                                     request_type == 0 ? "buy" : "sell", amount, price, amount * price)
                                      << std::endl;

                            uint16_t confirm;
                            std::cout << "Do you want to continue? (0 for continue, any other number for cancel): ";
                            std::cin >> confirm;
                            if (confirm != 0)
                            {
                                break;
                            }

                            bool successful;
                            {
                                auto packet = std::make_unique<packets::MakeRequestPacket>("USD/RUB", amount, price,
                                                                                           request_type, successful);
                                if (!packet->process(m_socket))
                                {
                                    std::cout << "\nUnknown response from server\n" << std::endl;
                                    break;
                                }
                            }
                            if (!successful)
                            {
                                std::cout << "\nAn error occurred while processing the request\n" << std::endl;
                                break;
                            }

                            std::cout << "\nRequest was created\n" << std::endl;
                            break;
                        }
                        case 3: {
                            auto packet = std::make_unique<packets::LogoutPacket>();
                            if (!packet->process(m_socket))
                            {
                                std::cout << "\nUnknown response from server\n" << std::endl;
                                break;
                            }

                            std::cout << "\nBye bye!\n" << std::endl;
                            menu_type = MenuType::Login;
                            break;
                        }
                        default: {
                            std::cout << "\nUnknown menu option\n" << std::endl;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
} // namespace exchange