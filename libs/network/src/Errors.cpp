#include "rtype/network/Errors.hpp"

#include <string>

namespace rtype::network
{

namespace
{

class NetworkCategory : public std::error_category
{
public:
    [[nodiscard]] const char *name() const noexcept override { return "rtype.network"; }

    [[nodiscard]] std::string message(int value) const override
    {
        switch (static_cast<NetworkError>(value)) {
        case NetworkError::EmptyFrame:
            return "empty frame";
        case NetworkError::FrameTooLarge:
            return "frame larger than the maximum";
        case NetworkError::UntrustedServer:
            return "the server key is not the pinned one";
        case NetworkError::HandshakeInterrupted:
            return "connection closed during the TLS handshake";
        }
        return "unknown network error";
    }
};

} // namespace

const std::error_category &networkCategory()
{
    static const NetworkCategory category;
    return category;
}

std::error_code make_error_code(NetworkError error)
{
    return {static_cast<int>(error), networkCategory()};
}

} // namespace rtype::network
