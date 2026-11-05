// Tests of what the client remembers of the account of each server (its username and its picture), and of
// where the errors of the master are shown in the forms of the accounts.

#include "Online/FormErrors.hpp"
#include "Online/KnownProfiles.hpp"
#include "Online/OnlineScreen.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

using rtype::client::FormField;
using rtype::client::KnownProfiles;
using rtype::client::RememberedProfile;
using rtype::lobby::ErrorCode;

namespace
{

/// The settings and the pictures in a folder of their own, removed after the test.
struct TemporaryFolder {
    std::filesystem::path path;
    rtype::engine::Settings settings;

    explicit TemporaryFolder(const std::string &test)
        : path(std::filesystem::temp_directory_path() / ("rtype_tests_profiles_" + test)),
          settings(path / "settings.txt")
    {
        std::filesystem::remove_all(path);
    }
    ~TemporaryFolder() { std::filesystem::remove_all(path); }
};

std::vector<std::byte> bytes(const std::string &text)
{
    std::vector<std::byte> result;
    for (const char c : text) {
        result.push_back(static_cast<std::byte>(c));
    }
    return result;
}

} // namespace

TEST_CASE("The account of a server is remembered, read back after a restart, and forgotten",
          "[online][profiles]")
{
    TemporaryFolder folder("remember");
    {
        KnownProfiles profiles(folder.settings, folder.path / "pictures");
        CHECK_FALSE(profiles.profile("play.rtype.gg:4242"));
        CHECK(profiles.remember("play.rtype.gg:4242",
                                {.username = "Kaze", .discord = true, .avatarHash = "a1b2"}));
        CHECK(profiles.remember("127.0.0.1:4242", {.username = "Nova"}));
    }
    rtype::engine::Settings again(folder.path / "settings.txt");
    again.readFile();
    KnownProfiles profiles(again, folder.path / "pictures");
    CHECK(profiles.profile("play.rtype.gg:4242") ==
          RememberedProfile{.username = "Kaze", .discord = true, .avatarHash = "a1b2"});
    CHECK(profiles.profile("127.0.0.1:4242") == RememberedProfile{.username = "Nova"});
    CHECK(profiles.forget("127.0.0.1:4242"));
    CHECK_FALSE(profiles.profile("127.0.0.1:4242"));
    CHECK_FALSE(profiles.forget("127.0.0.1:4242"));

    // A value written by hand that is not one is none
    again.setString(std::string(KnownProfiles::SettingsKeyPrefix) + "broken:1", "2:x:Kaze");
    CHECK_FALSE(profiles.profile("broken:1"));
}

TEST_CASE("A picture is kept under its version, which must be able to name a file", "[online][profiles]")
{
    TemporaryFolder folder("pictures");
    KnownProfiles profiles(folder.settings, folder.path / "pictures");
    CHECK_FALSE(profiles.picture("a1b2"));
    CHECK(profiles.keepPicture("a1b2", bytes("png bytes")));
    CHECK(profiles.picture("a1b2") ==
          std::vector<unsigned char>{'p', 'n', 'g', ' ', 'b', 'y', 't', 'e', 's'});

    CHECK_FALSE(profiles.keepPicture("../settings", bytes("png")));
    CHECK_FALSE(profiles.keepPicture("", bytes("png")));
    CHECK_FALSE(profiles.picture("../settings"));
    CHECK(rtype::client::isPictureVersion("a_1b2"));
    CHECK_FALSE(rtype::client::isPictureVersion("a.png"));
    CHECK_FALSE(rtype::client::isPictureVersion(std::string(65, 'a')));
}

TEST_CASE("An error of the master is shown under the field it is about", "[online][profiles]")
{
    CHECK(rtype::client::fieldOf(ErrorCode::InvalidCredentials) == FormField::Password);
    CHECK(rtype::client::fieldOf(ErrorCode::WeakPassword) == FormField::Password);
    CHECK(rtype::client::fieldOf(ErrorCode::UsernameTaken) == FormField::Username);
    CHECK(rtype::client::fieldOf(ErrorCode::InvalidName) == FormField::Username);
    CHECK(rtype::client::fieldOf(ErrorCode::InvalidEmail) == FormField::Email);
    CHECK(rtype::client::fieldOf(ErrorCode::InvalidCode) == FormField::Code);
    CHECK(rtype::client::fieldOf(ErrorCode::CodeExpired) == FormField::Code);
    // The others are not about a field: a toast
    CHECK(rtype::client::fieldOf(ErrorCode::Busy) == FormField::None);
    CHECK(rtype::client::fieldOf(ErrorCode::Banned) == FormField::None);
}

TEST_CASE("The initial of an avatar is a whole character, in capitals", "[online][profiles]")
{
    CHECK(rtype::client::initialOf("kaze") == "K");
    CHECK(rtype::client::initialOf("\xC3\xA9tienne") == "\xC3\x89");
    CHECK(rtype::client::initialOf("\xE2\x98\x85star") == "\xE2\x98\x85");
    CHECK(rtype::client::initialOf("") == "?");
}
