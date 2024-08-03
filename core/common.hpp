#pragma once

namespace core
{
    enum class RequestMessageType : uint16_t
    {
        Unknown = 1 << 0,
        ChallengeLogin = 1 << 1,
        ChallengeProof = 1 << 2,
        Logout = 1 << 3,
        Register = 1 << 4,
        WalletList = 1 << 5,
        MakeRequest = 1 << 6
    };

    enum class ErrorCode : uint16_t
    {
        Success = 0,
        AuthFailed = 1,
        AuthNotFound = 2,
        AuthExists = 3,
        DBFailed = 4,
        Restricted = 5,
        ValidationError = 6 
    };
} // namespace core