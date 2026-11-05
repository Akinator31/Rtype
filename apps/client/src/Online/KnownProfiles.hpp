#pragma once

#include "rtype/engine/Settings/Settings.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// What the client remembers of the account signed in on a server, to show it before connecting.
struct RememberedProfile {
    /// The username of the account.
    std::string username;
    /// Whether it is linked to Discord.
    bool discord = false;
    /// The version of its picture, empty for none: the picture is a file named after it.
    std::string avatarHash{};

    friend bool operator==(const RememberedProfile &, const RememberedProfile &) = default;
};

/// The accounts signed in on the servers the player met, for the list of the Online menu: each under
/// `servers.profile.<address>` (serverAddress()) in the settings, its picture in a folder of its own, as
/// `<hash>.png`, read without asking the master.
///
/// @code
/// KnownProfiles profiles(engine.getSettings(), userDataDirectory() / "pictures");
/// profiles.remember(address, {.username = "Kaze", .discord = true, .avatarHash = "a1b2"});
/// profiles.keepPicture("a1b2", png);
/// @endcode
class KnownProfiles
{
    engine::Settings &_settings;
    std::filesystem::path _pictures;

public:
    /// Where the profiles are kept in the settings.
    static constexpr std::string_view SettingsKeyPrefix = "servers.profile.";

    /// @param pictures The folder of the pictures, made when the first one is kept.
    KnownProfiles(engine::Settings &settings, std::filesystem::path pictures);

    /// The account remembered for this server, if any.
    [[nodiscard]] std::optional<RememberedProfile> profile(const std::string &address) const;
    /// Remembers the account of a server, and writes the settings.
    /// @return False if the settings could not be written.
    bool remember(const std::string &address, const RememberedProfile &profile);
    /// Forgets the account of a server, and writes the settings: at sign-out and at the deletion of the
    /// account. The picture stays, as another server may show it.
    bool forget(const std::string &address);

    /// The picture of this version, if the client keeps it.
    [[nodiscard]] std::optional<std::vector<unsigned char>> picture(const std::string &hash) const;
    /// Keeps a picture under its version.
    /// @return False if it could not be written, or if the version cannot name a file.
    bool keepPicture(const std::string &hash, std::span<const std::byte> png);
};

/// Whether a text can be the version of a picture, and so the name of its file: 1 to 64 of `A-Z a-z 0-9 _`.
[[nodiscard]] bool isPictureVersion(std::string_view hash);

} // namespace rtype::client
