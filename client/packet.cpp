#include "packet.hpp"
#include "precompiled.hpp"

namespace exchange
{
    Packet::Packet(core::RequestMessageType const message_type) : m_message_type(message_type)
    {
    }

    auto Packet::process(boost::asio::ip::tcp::socket& socket) -> bool
    {
        {
            nlohmann::json packet;
            packet["type"] = static_cast<uint16_t>(m_message_type);

            nlohmann::json payload;
            this->send(payload);

            packet["payload"] = payload;

            std::string const message = packet.dump();
            boost::asio::write(socket, boost::asio::buffer(message));
        }

        {
            boost::asio::streambuf buffer;
            boost::asio::read_until(socket, buffer, "\0");
            std::istream is(&buffer);

            std::string const message(std::istreambuf_iterator<char>(is), {});

            try
            {
                auto packet = nlohmann::json::parse(message);
                auto type = static_cast<core::RequestMessageType>(packet["type"].get<uint16_t>());

                if (type != m_message_type)
                {
                    return false;
                }

                this->accept(packet["payload"]);
                return true;
            }
            catch (nlohmann::json::exception e)
            {
                return false;
            }
        }
    }
} // namespace exchange