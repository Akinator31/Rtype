#pragma once

#include "rtype/lobby/Messages.hpp"

namespace rtype::client
{

/// The field of a form of the accounts an error of the master is about.
enum class FormField {
    /// None: the error is shown in a toast.
    None,
    /// The username or the email that signs in.
    Login,
    /// The username of a new account, or the name of a guest.
    Username,
    Email,
    Password,
    /// The code sent by email.
    Code,
};

/// Where an error of the master is shown: under the field it is about, or in a toast for the others. A form
/// that has not this field shows it in a toast too.
[[nodiscard]] FormField fieldOf(lobby::ErrorCode code);

} // namespace rtype::client
