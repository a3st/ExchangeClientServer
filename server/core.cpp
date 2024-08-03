#include "core.hpp"
#include "core/json.hpp"
#include "precompiled.hpp"
#include <botan/hash.h>
#include <botan/system_rng.h>

namespace exchange
{
    Core::Core(std::optional<std::filesystem::path> const log_path)
        : m_database("database.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE), m_login_system(m_database, log_path),
          m_wallet(m_database, log_path), m_exchange(m_database, log_path)
    {
        std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
        if (log_path)
        {
            sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.value().string()));
        }
        auto logger = std::make_shared<spdlog::logger>("core", sinks.begin(), sinks.end());
        spdlog::initialize_logger(logger);
    }

    Core::~Core()
    {
        spdlog::drop("core");
    }

    auto Core::start() -> void
    {
        m_exchange.process_requests(m_wallet);
    }

    auto Core::on_session_connected(uint64_t const session_id) -> void
    {
        m_login_system.initialize_session(session_id);
        m_srp6_sessions[session_id] = {};
    }

    auto Core::on_session_closed(uint64_t const session_id) -> void
    {
        m_login_system.close_session(session_id);
        m_srp6_sessions.erase(session_id);
    }

    auto Core::on_message(uint64_t const session_id, std::span<uint8_t const> const buffer) -> Response
    {
        try
        {
            auto packet = nlohmann::json::parse(buffer);
            auto message_type = static_cast<core::RequestMessageType>(packet["type"].get<uint16_t>());
            auto payload = packet["payload"];

            spdlog::get("server")->log(spdlog::level::trace, "Packet (msg: {}) was received",
                                       static_cast<uint16_t>(message_type));

            switch (message_type)
            {
                case core::RequestMessageType::MakeRequest: {
                    nlohmann::json response;
                    if (!m_login_system.auth_session(session_id))
                    {
                        response["error_code"] = core::ErrorCode::Restricted;
                        return Response(core::RequestMessageType::MakeRequest, response);
                    }

                    auto const currency = payload["currency"].get<std::string>();
                    auto const amount = payload["amount"].get<float>();
                    auto const price = payload["price"].get<float>();

                    auto const request_type = payload["request_type"].get<uint32_t>();
                    if (!(request_type == 0 || request_type == 1))
                    {
                        response["error_code"] = core::ErrorCode::ValidationError;
                        return Response(core::RequestMessageType::MakeRequest, response);
                    }

                    if (!m_exchange.make_request(m_login_system.user_id(session_id), currency, amount, price,
                                                 static_cast<modules::RequestType>(request_type)))
                    {
                        response["error_code"] = core::ErrorCode::DBFailed;
                        return Response(core::RequestMessageType::MakeRequest, response);
                    }
                    else
                    {
                        m_exchange.process_requests(m_wallet);

                        response["error_code"] = core::ErrorCode::Success;
                        return Response(core::RequestMessageType::MakeRequest, response);
                    }
                }
                break;

                case core::RequestMessageType::WalletList: {
                    nlohmann::json response;
                    if (!m_login_system.auth_session(session_id))
                    {
                        response["error_code"] = core::ErrorCode::Restricted;
                        return Response(core::RequestMessageType::WalletList, response);
                    }

                    auto result = m_wallet.wallets(m_login_system.user_id(session_id));
                    if (!result)
                    {
                        response["error_code"] = core::ErrorCode::DBFailed;
                        return Response(core::RequestMessageType::WalletList, response);
                    }
                    else
                    {
                        response["error_code"] = core::ErrorCode::Success;
                        response["wallets"] = result.value();
                        return Response(core::RequestMessageType::WalletList, response);
                    }
                    break;
                }

                case core::RequestMessageType::Logout: {
                    if (m_login_system.auth_session(session_id))
                    {
                        m_login_system.logout_session(session_id);
                        return Response(core::RequestMessageType::Logout, std::nullopt);
                    }
                    else
                    {
                        return Response(core::RequestMessageType::Unknown, std::nullopt);
                    }
                    break;
                }

                case core::RequestMessageType::Register: {
                    auto const user_name = payload["user_name"].get<std::string>();
                    if (m_login_system.exists(user_name))
                    {
                        nlohmann::json response;
                        response["error_code"] = core::ErrorCode::AuthExists;
                        return Response(core::RequestMessageType::Register, response);
                    }
                    else
                    {
                        auto const verifier = payload["verifier"].get<std::string>();

                        nlohmann::json response;
                        uint64_t user_id;
                        if (m_login_system.register_account(user_name, verifier, user_id))
                        {
                            uint64_t wallet_id;
                            m_wallet.create_wallet(user_id, "USD", wallet_id);
                            m_wallet.create_wallet(user_id, "RUB", wallet_id);

                            response["error_code"] = core::ErrorCode::Success;
                        }
                        else
                        {
                            response["error_code"] = core::ErrorCode::AuthFailed;
                        }
                        return Response(core::RequestMessageType::Register, response);
                    }
                    break;
                }

                case core::RequestMessageType::ChallengeLogin: {
                    auto const user_name = payload["user_name"].get<std::string>();
                    if (!m_login_system.exists(user_name))
                    {
                        nlohmann::json response;
                        response["error_code"] = core::ErrorCode::AuthNotFound;
                        return Response(core::RequestMessageType::ChallengeLogin, response);
                    }
                    else
                    {
                        auto const verifier = payload["verifier"].get<std::string>();

                        if (!m_login_system.login_account(user_name, verifier, session_id))
                        {
                            nlohmann::json response;
                            response["error_code"] = core::ErrorCode::AuthFailed;
                            return Response(core::RequestMessageType::ChallengeLogin, response);
                        }
                        else
                        {
                            auto& srp6_session = m_srp6_sessions[session_id];

                            std::string const B = srp6_session.srp6
                                                      .step1(Botan::BigInt::from_string(verifier), "modp/srp/1024",
                                                             "SHA-256", Botan::system_rng())
                                                      .to_hex_string();
                            srp6_session.B = B;

                            nlohmann::json response;
                            response["error_code"] = core::ErrorCode::Success;
                            response["B"] = B;
                            return Response(core::RequestMessageType::ChallengeLogin, response);
                        }
                    }
                }

                case core::RequestMessageType::ChallengeProof: {
                    auto const A = Botan::BigInt::from_string(payload["A"]);
                    auto const M1 = payload["M1"].get<std::string>();
                    auto const user_name = payload["user_name"].get<std::string>();

                    auto& srp6_session = m_srp6_sessions[session_id];
                    std::string const secret = srp6_session.srp6.step2(A).to_string();

                    auto sha256 = Botan::HashFunction::create("SHA-256");
                    sha256->update(A.to_hex_string());
                    sha256->update(srp6_session.B);
                    sha256->update(secret);
                    auto const M = Botan::BigInt::from_bytes(sha256->final()).to_hex_string();

                    nlohmann::json response;
                    if (M.compare(M1) == 0)
                    {
                        m_login_system.login_session(session_id);

                        response["error_code"] = core::ErrorCode::Success;
                    }
                    else
                    {
                        response["error_code"] = core::ErrorCode::AuthFailed;
                    }
                    return Response(core::RequestMessageType::ChallengeProof, response);
                }

                default: {
                    return Response(core::RequestMessageType::Unknown, std::nullopt);
                }
            }
        }
        catch (nlohmann::json::exception e)
        {
            return Response(core::RequestMessageType::Unknown, std::nullopt);
        }
    }
} // namespace exchange