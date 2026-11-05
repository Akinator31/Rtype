#include "Online/FormErrors.hpp"

namespace rtype::client
{

FormField fieldOf(lobby::ErrorCode code)
{
    using lobby::ErrorCode;
    switch (code) {
    case ErrorCode::InvalidCredentials:
    case ErrorCode::WeakPassword:
        return FormField::Password;
    case ErrorCode::UsernameTaken:
    case ErrorCode::InvalidName:
    case ErrorCode::NameReserved:
        return FormField::Username;
    case ErrorCode::InvalidEmail:
        return FormField::Email;
    case ErrorCode::InvalidCode:
    case ErrorCode::CodeExpired:
        return FormField::Code;
    default:
        return FormField::None;
    }
}

} // namespace rtype::client
