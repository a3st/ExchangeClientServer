#include "login.hpp"
#include "precompiled.hpp"
#include <botan/hash.h>
#include <botan/srp6.h>
#include <botan/system_rng.h>

namespace exchange::packets
{
    LoginChallangePacket::LoginChallangePacket(std::string_view const user_name, std::string_view const password,
                                               std::span<uint8_t const> const salt, std::string& B)
        : Packet(core::RequestMessageType::ChallengeLogin), m_user_name(user_name), m_password(password), m_salt(salt),
          m_B(&B)
    {
    }

    auto LoginChallangePacket::accept(nlohmann::json const& payload) -> void
    {
        auto error_code = static_cast<core::ErrorCode>(payload["error_code"].get<uint16_t>());
        if (error_code != core::ErrorCode::Success)
        {
            return;
        }

        *m_B = payload["B"].get<std::string>();
    }

    auto LoginChallangePacket::send(nlohmann::json& payload) -> void
    {
        std::vector<uint8_t> salt;
        salt.assign(m_salt.begin(), m_salt.end());

        std::string const verifier =
            Botan::srp6_generate_verifier(m_user_name, m_password, salt, "modp/srp/1024", "SHA-256").to_hex_string();

        payload["verifier"] = verifier;
        payload["user_name"] = m_user_name;
    }

    LoginChallangeProofPacket::LoginChallangeProofPacket(std::string_view const user_name,
                                                         std::string_view const password, std::string_view const B,
                                                         std::span<uint8_t const> const salt, bool& auth)
        : Packet(core::RequestMessageType::ChallengeProof), m_user_name(user_name), m_password(password), m_B(B),
          m_salt(salt), m_auth(&auth)
    {
    }

    auto LoginChallangeProofPacket::accept(nlohmann::json const& payload) -> void
    {
        auto error_code = static_cast<core::ErrorCode>(payload["error_code"].get<uint16_t>());
        if (error_code != core::ErrorCode::Success)
        {
            *m_auth = false;
            return;
        }

        *m_auth = true;
    }

    auto LoginChallangeProofPacket::send(nlohmann::json& payload) -> void
    {
        std::vector<uint8_t> salt;
        salt.assign(m_salt.begin(), m_salt.end());

        auto [A, secret] = Botan::srp6_client_agree(m_user_name, m_password, "modp/srp/1024", "SHA-256", salt,
                                                    Botan::BigInt::from_string(m_B), Botan::system_rng());

        std::string const hex_secret = secret.to_string();
        std::string const hex_A = A.to_hex_string();

        auto sha256 = Botan::HashFunction::create("SHA-256");
        sha256->update(hex_A);
        sha256->update(m_B);
        sha256->update(hex_secret);
        std::string const M1 = Botan::BigInt::from_bytes(sha256->final()).to_hex_string();

        payload["M1"] = M1;
        payload["A"] = hex_A;
        payload["user_name"] = m_user_name;
    }

    RegisterPacket::RegisterPacket(std::string_view const user_name, std::string_view const password,
                                   std::span<uint8_t const> const salt, bool& registered)
        : Packet(core::RequestMessageType::Register), m_user_name(user_name), m_password(password), m_salt(salt),
          m_registered(&registered)
    {
    }

    auto RegisterPacket::accept(nlohmann::json const& payload) -> void
    {
        auto error_code = static_cast<core::ErrorCode>(payload["error_code"].get<uint16_t>());
        if (error_code != core::ErrorCode::Success)
        {
            *m_registered = false;
            return;
        }

        *m_registered = true;
    }

    auto RegisterPacket::send(nlohmann::json& payload) -> void
    {
        std::vector<uint8_t> salt;
        salt.assign(m_salt.begin(), m_salt.end());

        std::string const verifier =
            Botan::srp6_generate_verifier(m_user_name, m_password, salt, "modp/srp/1024", "SHA-256").to_hex_string();

        payload["verifier"] = verifier;
        payload["user_name"] = m_user_name;
    }

    LogoutPacket::LogoutPacket() : Packet(core::RequestMessageType::Logout)
    {
    }

    auto LogoutPacket::accept(nlohmann::json const& payload) -> void
    {
    }

    auto LogoutPacket::send(nlohmann::json& payload) -> void
    {
    }
} // namespace exchange::packets