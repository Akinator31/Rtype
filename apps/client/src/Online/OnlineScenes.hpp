#pragma once

#include "Discord/DiscordFriends.hpp"
#include "Discord/DiscordInvite.hpp"
#include "Discord/FriendPictures.hpp"
#include "Game/GameScenes.hpp"
#include "Online/FormErrors.hpp"
#include "Online/KnownProfiles.hpp"
#include "Online/KnownServers.hpp"
#include "Online/LobbyClient.hpp"
#include "Online/OnlineScreen.hpp"
#include "Online/SessionTokens.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/discord/Authorizer.hpp"
#include "rtype/discord/Connection.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Tls.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rtype::client
{

struct VoiceDevices;

/// Name of the list of the games of a server.
inline constexpr const char *LobbyScene = "online_lobby";
/// Name of the question asked at the first connection to a server: trust its key?
inline constexpr const char *TrustServerScene = "online_trust";
/// Name of the warning shown when a known server presents another key.
inline constexpr const char *ChangedKeyScene = "online_changed_key";
/// Names of the scenes of the accounts (docs/architecture/client-online.md#accounts): what the server offers,
/// then signing in, creating an account, the code of the email, the forgotten password, and the account
/// signed in.
inline constexpr const char *AccountsScene = "online_accounts";
inline constexpr const char *SignInScene = "online_sign_in";
inline constexpr const char *RegisterScene = "online_register";
inline constexpr const char *CodeScene = "online_code";
inline constexpr const char *ForgotPasswordScene = "online_forgot_password";
inline constexpr const char *ResetPasswordScene = "online_reset_password";
inline constexpr const char *MyAccountScene = "online_my_account";
inline constexpr const char *DeleteAccountScene = "online_delete_account";
/// Names of the scenes of Discord: waiting for the player to accept in Discord, then the username of a new
/// account.
inline constexpr const char *DiscordScene = "online_discord";
inline constexpr const char *ChooseUsernameScene = "online_choose_username";

/// A fingerprint as shown to the player: its 64 digits in groups of 4, `groupsPerLine` groups per line.
[[nodiscard]] std::vector<std::string> fingerprintLines(const network::KeyFingerprint &fingerprint,
                                                        std::size_t groupsPerLine = 8);

/// Splits "host:port" into its host and its port, the default port without one. IPv6 addresses go in
/// brackets: "[::1]:4242".
/// @return Nothing if the port is not a number from 1 to 65535.
[[nodiscard]] std::optional<std::pair<std::string, std::uint16_t>>
splitServer(const std::string &text, std::uint16_t defaultPort = 4242);

/// Playing online (docs/architecture/client-online.md): the Online menu, where the player gives a server and
/// its name, or picks one met before, then the lobby of the server, with its games. The screens follow the
/// Série 14 of the design: the fields of a form, its errors under them, Discord first, and the profile
/// of the account (its Discord picture and names) wherever it shows.
///
/// - **Connect** opens a LobbyClient to the server, trusting the key KnownServers knows for it (ADR 0021).
///   A server never met shows its key, which the player may trust: it is remembered, and the client connects
///   again. A known server with another key is refused, with a warning, until the player forgets its key:
///   from the warning itself (**Forget the old key**, which connects again to show the new one), from the
///   row of the server in the servers met (**Forget**), or by typing it (**Forget the server key**).
/// - **The lobby** lists the games: Create a game, Join one, Refresh, Disconnect. Creating or joining one
///   first opens the hangar, where the player picks its ship. Joining plays a RemoteSession in the game
///   scene; when the game ends, the player is back in the lobby, after its results if it went to its end
///   (GameScenes::showResults()).
/// - **The accounts**: on a server that offers them, WELCOME leads to a choice (sign in, create an account,
///   play as a guest). The session token of SIGNED_IN is kept for the server (SessionTokens) and sent in the
///   next HELLO: the player comes back signed in. It is forgotten at sign-out, at the deletion of the
///   account, and on SESSION_EXPIRED, after which the client connects again as a guest. A server without
///   accounts looks as before them.
/// - **Discord**, on a server that offers it: the client asks the master for its application, the player
///   accepts it in Discord (discord::Authorizer), and the master signs in, or asks for the username of a new
///   account. Signed in, the same flow links Discord to the account, or confirms the deletion of an account
///   without password.
/// - **The profile**: signed in, the client reads the Discord profile of the account (GET_PROFILE) and its
///   picture (GET_AVATAR), shown in the lobby and in My account, and remembered with the server for the
///   Online menu (KnownProfiles), the picture kept on disk under its version.
/// - **Invitations**: a friend invited from Discord, or accepted, joins the game: joinFromDiscord().
/// - **Friends**, connected to Discord: the lobby lists the Discord friends of the player beside the games,
///   the invitations received first (Join), then those playing R-Type (Ask to join while their game has
///   room), online, offline. A new invitation shows a toast wherever the player is.
/// - What goes wrong with a field is shown under it; the rest in a toast, in the language of the game: the
///   server cannot be reached, is not the expected one, the game is full...
class OnlineScenes
{
    /// What a Discord confirmation is for.
    enum class DiscordPurpose {
        SignIn,
        Link,
        DeleteAccount,
    };

    engine::Engine &_engine;
    const engine::InterfaceStyle &_style;
    render::ResourceManager &_resources;
    network::EventLoop &_loop;
    discord::Authorizer &_discord;
    /// The connection to Discord with the token of the player, on a master with the social features.
    discord::Connection &_discordConnection;
    /// The Discord friends of the player, once connected with that token.
    discord::Friends &_friends;
    /// The rows of friends the lobby shows, and the invitations already told in a toast.
    std::vector<FriendRow> _friendRows;
    std::vector<std::uint64_t> _toldInvitations;
    /// The pictures of the friends, downloaded from the CDN of Discord.
    FriendPictures _friendPictures;
    GameScenes &_games;
    localization::Localization &_translations;
    ShipSelection &_ships;
    /// The definitions of the client: the weapon of the player, whose shots its games predict.
    const game::GameConfig &_config;
    /// The devices of the voice of the games joined.
    VoiceDevices &_voice;
    KnownServers _known;
    SessionTokens _sessions;
    KnownProfiles _profiles;
    /// The sky of the screens that are not over the main menu, and the logo of Discord.
    render::TextureId _sky;
    render::TextureId _discordLogo;
    std::string _serverText = "127.0.0.1:4242";
    std::string _name = "Player";
    std::optional<network::Address> _server;
    /// The server connected to, as KnownServers knows it.
    std::string _address;
    /// The key a server presented and the client did not trust, shown to the player.
    std::optional<network::KeyFingerprint> _presented;
    std::unique_ptr<LobbyClient> _lobby;
    std::vector<lobby::GameInfo> _list;
    /// The username or email of the account whose email is verified with a code, in CodeScene.
    std::string _login;
    /// The email of the forgotten password, in ResetPasswordScene, or of the account just created, in
    /// CodeScene.
    std::string _email;
    /// The Discord profile of the account signed in, and its picture, once read.
    std::optional<lobby::ProfileMessage> _profile;
    std::optional<render::TextureId> _picture;
    /// The new Discord user confirmed, who chooses its username, and its picture.
    std::optional<lobby::ProfileMessage> _newUser;
    std::optional<render::TextureId> _newUserPicture;
    /// The experience of the account as the player joined the game played: none for a guest.
    std::optional<std::uint64_t> _xpBefore;
    /// The game of a friend the player accepted to join from Discord, joined once on its server.
    std::optional<GameInvite> _invite;
    /// Where a Discord confirmation stands, for DiscordScene: 0 Discord opened, 1 waiting for the player,
    /// 2 the server checking.
    int _discordStep = 0;

    void connect();
    /// The server presented a key not trusted: asks the player, or warns it.
    void untrusted(const network::KeyFingerprint &presented);
    /// Forgets the key of the server typed, if the player trusted one.
    void forgetServer();
    /// Forgets the key trusted for this server, if any, and tells the player in a toast.
    void forgetKey(const std::string &address);
    void disconnect();
    /// Lists the games, then shows them.
    void refresh();
    void joined(const LobbyClient::JoinResult &result);
    /// The hangar, then the game: a friend invited the player to it.
    void joinInvited(std::uint32_t gameId);
    /// The friends or the invitations changed: a toast for each new invitation, the lobby built again if its
    /// rows changed.
    void friendsChanged();
    /// The action of a row of friends of the lobby.
    void actOnFriend(const FriendRow &row);
    /// The game played ends: leaves it, then back to the list.
    void gameEnded();
    void lobbyClosed(const std::string &reason);
    /// A game online counted for the account: its results show the experience it gave; in a toast with the
    /// level, LEVEL UP if it went up, if they are not shown.
    void showProgress(const lobby::ProgressMessage &progress);
    /// A toast for each ship of the hangar that a level up from `before` to `level` unlocks.
    void showUnlockedShips(std::uint32_t before, std::uint32_t level);
    /// Forgets the lobby later: this may run in one of its handlers.
    void forgetLobby();

    /// What an error of the master means, in the language of the game.
    [[nodiscard]] std::string message(lobby::ErrorCode code) const;
    /// Shows an error of the master under the field of the form it is about, if the form has it, else in a
    /// toast.
    /// @param fields The fields of the form, by what they hold; 0 for those it has not.
    void showError(lobby::ErrorCode code, const std::map<FormField, ecs::entity_t> &fields);
    /// Starts a screen in its scene: its sky behind it when it is not over another one.
    [[nodiscard]] OnlineScreen screen(engine::Engine &target, engine::SceneId scene, bool sky = true) const;
    void declareServerScenes(const engine::InterfaceStyle &style);
    void declareLobbyScene(const engine::InterfaceStyle &style);
    void declareAccountScenes(const engine::InterfaceStyle &style);
    void declareFormScenes(const engine::InterfaceStyle &style);
    void declareDiscordScenes(const engine::InterfaceStyle &style);
    void declareMyAccountScenes(const engine::InterfaceStyle &style);
    /// Reads the profile and the picture of the account signed in, then shows them where they are.
    void readProfile();
    /// A picture as a round texture, cached under its version.
    [[nodiscard]] std::optional<render::TextureId> roundPicture(const std::string &hash,
                                                                std::span<const std::byte> png);
    /// The avatar of the account signed in, the remembered one of a server, or of a guest.
    [[nodiscard]] engine::Avatar accountAvatar(float size) const;
    /// Builds a scene again if it is the one shown, to show what changed.
    void refreshIfShown(const char *scene);
    /// WELCOME: the choice of the accounts if the server offers them and the player is not signed in, else
    /// the games.
    void welcomed();
    /// The answer to a request that signs in: keeps the token, then the games.
    void signedIn(const LobbyClient::SignInResult &result);
    /// The answer to SIGN_OUT or DELETE_ACCOUNT: forgets the token, then connects again as a guest.
    void signedOut(std::optional<lobby::ErrorCode> error, const char *done);
    /// DISCORD_SIGN_IN, then the player accepts in Discord, then DISCORD_CONFIRM.
    void startDiscord(DiscordPurpose purpose);
    /// The answer to DISCORD_CONFIRM.
    void discordConfirmed(DiscordPurpose purpose, const LobbyClient::DiscordConfirmResult &result);
    /// The scene a Discord confirmation goes back to when it fails.
    [[nodiscard]] static const char *discordOrigin(DiscordPurpose purpose);
    /// DISCORD_TOKEN: keeps its refresh token for this server, and connects to Discord with it.
    void discordToken(const lobby::DiscordTokenMessage &token);
    /// DISCORD_REFRESH with the refresh token kept for this server, signed in: at the connection, and when
    /// the token expires. A refresh token refused is forgotten.
    void refreshDiscord();

public:
    /// Declares the Online menu and the lobby. Everything given must outlive the engine.
    /// The servers trusted are kept in the settings of the engine.
    /// @param resources Loads the sky, the logo of Discord and the pictures of the accounts.
    /// @param pictures The folder the pictures of the accounts are kept in.
    /// @param discord Asks the player to accept the Discord application of a server; polled by the game loop.
    /// @param discordConnection Connects to Discord with the token a server gives (DISCORD_TOKEN), and
    ///   disconnects when the player leaves the server.
    /// @param friends The Discord friends of the player, shown in the lobby, joined from there.
    /// @param ships The ship the player picks in the hangar, before creating or joining a game.
    /// @param config The definitions of the client (loadGameConfig()): the weapon of the player, whose shots
    ///   its games online predict.
    OnlineScenes(engine::Engine &engine, render::ResourceManager &resources, network::EventLoop &loop,
                 GameScenes &games, const engine::InterfaceStyle &style,
                 localization::Localization &translations, discord::Authorizer &discord,
                 discord::Connection &discordConnection, discord::Friends &friends, ShipSelection &ships,
                 std::filesystem::path pictures, const game::GameConfig &config, VoiceDevices &voice);

    /// Tells the friends no longer: they outlive the scenes.
    ~OnlineScenes();

    OnlineScenes(const OnlineScenes &) = delete;
    OnlineScenes &operator=(const OnlineScenes &) = delete;

    /// The player accepted, in Discord, to join the game of a friend (discord::Presence::onJoin()): connects
    /// to its server, through the usual trust and sign-in, then joins it after the hangar. Refused during a
    /// game, or for a secret that is not one of a game.
    void joinFromDiscord(const std::string &secret);
    /// The server whose list of games the player is looking at, as KnownServers knows it: friends can be
    /// invited to it from Discord. None elsewhere.
    [[nodiscard]] std::optional<std::string> lobbyServer() const;
};

} // namespace rtype::client
