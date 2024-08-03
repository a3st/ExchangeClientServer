#pragma once

#include "packet.hpp"

namespace exchange::packets
{
    class LoginChallangePacket : public Packet
    {
      public:
        LoginChallangePacket(std::string_view const user_name, std::string_view const password,
                             std::span<uint8_t const> const salt, std::string& B);

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;

      private:
        std::string_view m_user_name;
        std::string_view m_password;
        std::span<uint8_t const> m_salt;

        std::string* m_B;
    };

    class LoginChallangeProofPacket : public Packet
    {
      public:
        LoginChallangeProofPacket(std::string_view const user_name, std::string_view const password,
                                  std::string_view const B, std::span<uint8_t const> const salt, bool& auth);

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;

      private:
        std::string_view m_user_name;
        std::string_view m_password;
        std::string_view m_B;
        std::span<uint8_t const> m_salt;

        bool* m_auth;
    };

    class RegisterPacket : public Packet
    {
      public:
        RegisterPacket(std::string_view const user_name, std::string_view const password,
                       std::span<uint8_t const> const salt, bool& registered);

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;

      private:
        std::string_view m_user_name;
        std::string_view m_password;
        std::span<uint8_t const> m_salt;

        bool* m_registered;
    };

    class LogoutPacket : public Packet
    {
      public:
        LogoutPacket();

      protected:
        auto accept(nlohmann::json const& payload) -> void override;

        auto send(nlohmann::json& payload) -> void override;
    };
} // namespace exchange::packets