// The scenes of the accounts of OnlineScenes (docs/architecture/client-online.md#accounts), after the Série
// 14 of the design: what the server offers, sign in, create an account, the code of the email, the forgotten
// password, Discord, and the account signed in.

#include "MainMenu/Layout.hpp"
#include "Online/OnlineScenes.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <variant>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("online");
    return instance;
}

using online_layout::columnLeft;
using online_layout::rightLeft;
using Tone = OnlineScreen::Tone;

/// Longest password typed, in characters: what the master accepts.
constexpr std::size_t MaxPasswordCharacters = 128;
/// Shortest password the master accepts, in characters: the gauge of the strength is full there.
constexpr std::size_t MinPasswordCharacters = 10;

/// The top of the buttons under the fields of a form of `rows` fields.
float buttonsTop(int rows)
{
    return online_layout::fieldsTop + (static_cast<float>(rows) * online_layout::fieldsSpacing) + 20.0f;
}

/// Reads the text of a field of a form.
std::string textOf(engine::Engine &engine, ecs::entity_t field)
{
    return engine.getEcs().getComponent<engine::TextField>(field).input.text();
}

/// An email as shown on a screen that others may see: its first letter, then its domain.
std::string maskEmail(const std::string &email)
{
    const std::size_t at = email.find('@');
    if (at == std::string::npos || at == 0) {
        return email;
    }
    return email.substr(0, 1) + "***" + email.substr(at);
}

/// A date of the protocol (milliseconds since 1970, UTC) as day/month/year.
std::string formatDate(std::uint64_t milliseconds)
{
    const std::chrono::sys_days day =
        std::chrono::floor<std::chrono::days>(std::chrono::sys_time<std::chrono::milliseconds>(
            std::chrono::milliseconds(static_cast<long long>(milliseconds))));
    const std::chrono::year_month_day date(day);
    return std::format("{:02}/{:02}/{}", static_cast<unsigned>(date.day()),
                       static_cast<unsigned>(date.month()), static_cast<int>(date.year()));
}

/// The username proposed to a new Discord user: its name on Discord, kept to what a username allows.
std::string proposedUsername(const std::string &discordUsername, const std::string &fallback)
{
    std::string proposed;
    for (const char c : discordUsername) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
            c == '-') {
            proposed += c;
        }
    }
    proposed.resize(std::min(proposed.size(), lobby::MaxUsernameLength));
    return proposed.size() >= 3 ? proposed : fallback;
}

} // namespace

std::string OnlineScenes::message(lobby::ErrorCode code) const
{
    return _translations.text(errorKey(code));
}

void OnlineScenes::showError(lobby::ErrorCode code, const std::map<FormField, ecs::entity_t> &fields)
{
    const auto found = fields.find(fieldOf(code));
    ecs::Ecs &ecs = _engine.getEcs();
    if (found != fields.end() && ecs.isAlive(found->second) &&
        ecs.hasComponent<engine::TextField>(found->second)) {
        ecs.getComponent<engine::TextField>(found->second).error = message(code);
        return;
    }
    _engine.showToast(message(code), engine::ToastType::Error);
}

void OnlineScenes::welcomed()
{
    if (_lobby && _lobby->account()) {
        // Signed in by the token of HELLO: the Discord token too, from the refresh token kept
        refreshDiscord();
        readProfile();
    } else if (_lobby && (_lobby->features().passwordAccounts || _lobby->features().discord)) {
        _engine.switchScene(AccountsScene);
        return;
    }
    refresh();
}

void OnlineScenes::discordToken(const lobby::DiscordTokenMessage &token)
{
    if (!_sessions.keepDiscord(_address, token.refreshToken)) {
        logger().warning("The Discord token of {} is not kept: it is asked again at the next sign-in",
                         _address);
    }
    _discordConnection.connect(token.accessToken);
}

void OnlineScenes::refreshDiscord()
{
    if (!_lobby || !_lobby->account()) {
        return;
    }
    const std::optional<std::string> refreshToken = _sessions.discordRefreshToken(_address);
    if (!refreshToken) {
        // Signed in before the server gave the tokens, or with a password: no voice through Discord
        logger().info("No Discord token kept for {}: sign in with Discord again for its voice", _address);
        return;
    }
    _lobby->discordRefresh(*refreshToken, [this](std::optional<lobby::ErrorCode> error) {
        if (!error) {
            return;
        }
        if (*error == lobby::ErrorCode::DiscordTokenRefused) {
            // Discord, or the server, will not renew it: the next sign-in with Discord gives another
            _sessions.forgetDiscord(_address);
            _discordConnection.disconnect();
        }
        logger().info("No Discord token from {}: {}", _address, describe(*error));
    });
}

void OnlineScenes::signedIn(const LobbyClient::SignInResult &result)
{
    if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
        _engine.showToast(message(*error), engine::ToastType::Error);
        return;
    }
    const auto &signedIn = std::get<lobby::SignedInMessage>(result);
    if (!_sessions.keep(_address, signedIn.token)) {
        _engine.showToast(_translations.text("account.not_kept"), engine::ToastType::Warning);
    }
    _engine.showToast(
        std::format("{} {}", _translations.text("account.signed_in"), signedIn.account.username),
        engine::ToastType::Success);
    _newUser.reset();
    _newUserPicture.reset();
    readProfile();
    refresh();
}

void OnlineScenes::signedOut(std::optional<lobby::ErrorCode> error, const char *done)
{
    if (error) {
        _engine.showToast(message(*error), engine::ToastType::Error);
        return;
    }
    _sessions.forget(_address);
    _profiles.forget(_address);
    _engine.showToast(_translations.text(done));
    // The master closed the connection: back as a guest
    connect();
}

const char *OnlineScenes::discordOrigin(DiscordPurpose purpose)
{
    switch (purpose) {
    case DiscordPurpose::SignIn:
        return AccountsScene;
    case DiscordPurpose::Link:
        return MyAccountScene;
    case DiscordPurpose::DeleteAccount:
        return DeleteAccountScene;
    }
    return AccountsScene;
}

void OnlineScenes::startDiscord(DiscordPurpose purpose)
{
    if (!_lobby) {
        return;
    }
    _lobby->discordSignIn([this, purpose](const auto &result) {
        if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
            _engine.showToast(message(*error), engine::ToastType::Error);
            return;
        }
        // Discord opens: the player accepts there, then comes back
        _discordStep = 1;
        _engine.pushScene(DiscordScene);
        const auto &authorize = std::get<lobby::DiscordAuthorizeMessage>(result);
        _discord.authorize(
            authorize.applicationId, authorize.scopes,
            [this, purpose](const discord::AuthorizationResult &authorization) {
                if (!_lobby) {
                    return;
                }
                if (const auto *error = std::get_if<discord::AuthorizationError>(&authorization)) {
                    _engine.showToast(_translations.text(*error == discord::AuthorizationError::Refused
                                                             ? "account.discord_refused"
                                                             : "account.discord_failed"),
                                      engine::ToastType::Warning);
                    if (_engine.isSceneOpen(DiscordScene)) {
                        _engine.popScene();
                    }
                    return;
                }
                // Accepted: the server checks it with Discord
                _discordStep = 2;
                if (_engine.isSceneOpen(DiscordScene)) {
                    _engine.popScene();
                    _engine.pushScene(DiscordScene);
                }
                const auto &accepted = std::get<discord::Authorization>(authorization);
                _lobby->discordConfirm({.code = accepted.code,
                                        .codeVerifier = accepted.codeVerifier,
                                        .redirectUri = accepted.redirectUri},
                                       [this, purpose](const LobbyClient::DiscordConfirmResult &confirmed) {
                                           discordConfirmed(purpose, confirmed);
                                       });
            });
    });
}

void OnlineScenes::discordConfirmed(DiscordPurpose purpose, const LobbyClient::DiscordConfirmResult &result)
{
    const auto closeWaiting = [this] {
        if (_engine.isSceneOpen(DiscordScene)) {
            _engine.popScene();
        }
    };
    if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
        _engine.showToast(message(*error), engine::ToastType::Error);
        closeWaiting();
        return;
    }
    if (const auto *account = std::get_if<lobby::SignedInMessage>(&result)) {
        // An identity that has an account
        signedIn(*account);
        return;
    }
    switch (purpose) {
    case DiscordPurpose::SignIn: {
        // A new Discord user: its account needs a username, chosen with its Discord profile in view
        _newUser = std::get<LobbyClient::DiscordConfirmed>(result).newUser;
        _newUserPicture.reset();
        _engine.switchScene(ChooseUsernameScene);
        if (_newUser && !_newUser->avatarHash.empty() && _lobby) {
            _lobby->picture(0, [this](const auto &picture) {
                if (const auto *received = std::get_if<LobbyClient::Picture>(&picture)) {
                    _profiles.keepPicture(received->hash, received->png);
                    _newUserPicture = roundPicture(received->hash, received->png);
                    refreshIfShown(ChooseUsernameScene);
                }
            });
        }
        return;
    }
    case DiscordPurpose::Link:
        _engine.showToast(_translations.text("account.discord_linked"), engine::ToastType::Success);
        closeWaiting();
        readProfile();
        return;
    case DiscordPurpose::DeleteAccount:
        // Confirmed for five minutes: the deletion needs no password
        closeWaiting();
        _lobby->deleteAccount(
            "", [this](std::optional<lobby::ErrorCode> error) { signedOut(error, "account.deleted"); });
        return;
    }
}

void OnlineScenes::declareAccountScenes(const engine::InterfaceStyle &style)
{
    // E1: what the server offers, Discord first, then an account, then a guest under an "or"
    _engine.createScene(
        AccountsScene, {.onBack = [this] { disconnect(); }},
        [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_serverText, [this] { disconnect(); });
            if (!_lobby) {
                return;
            }
            const lobby::ServerFeatures &features = _lobby->features();
            std::string offers = _translations.text("account.offers_key");
            if (features.passwordAccounts) {
                offers += " · " + _translations.text("account.offers_accounts");
            }
            if (features.discord) {
                offers += " · " + _translations.text("account.offers_discord");
            }
            if (features.guests) {
                offers += " · " + _translations.text("account.offers_guests");
            }
            page.note(offers, {engine::AnchorPoint::TopLeft, screenMargin + 80.0f, screenMargin + 64.0f},
                      Tone::Hint, 20.0f);

            float top = 300.0f;
            if (features.discord) {
                page.discordButton(
                    _translations.text("account.discord"), [this] { startDiscord(DiscordPurpose::SignIn); },
                    {engine::AnchorPoint::TopLeft, columnLeft, top}, _discordLogo);
                page.note(_translations.text("account.discord_note"),
                          {engine::AnchorPoint::TopLeft, columnLeft, top + 70.0f});
                top += 170.0f;
            }
            if (features.passwordAccounts) {
                page.button(_translations.text("account.sign_in"),
                            [this] { _engine.switchScene(SignInScene); },
                            {engine::AnchorPoint::TopLeft, columnLeft, top});
                page.button(_translations.text("account.create"),
                            [this] { _engine.switchScene(RegisterScene); },
                            {engine::AnchorPoint::TopLeft, columnLeft, top + online_layout::buttonsSpacing});
                top += 2.0f * online_layout::buttonsSpacing + 30.0f;
            }
            page.rule({engine::AnchorPoint::TopLeft, columnLeft, top}, 240.0f, {51, 55, 59, 255});
            page.note(toCapitals(_translations.text("account.or")),
                      {engine::AnchorPoint::TopLeft, columnLeft + 258.0f, top - 10.0f}, Tone::Hint, 20.0f);
            page.rule({engine::AnchorPoint::TopLeft, columnLeft + 310.0f, top}, 240.0f, {51, 55, 59, 255});
            top += 40.0f;
            if (features.guests) {
                page.button(_translations.text("account.guest"), [this] { refresh(); },
                            {engine::AnchorPoint::TopLeft, columnLeft, top});
                page.note(std::format("{} · {}", toCapitals(_name), _translations.text("account.guest_note")),
                          {engine::AnchorPoint::TopLeft, columnLeft, top + 70.0f}, Tone::Hint);
            } else {
                page.note(_translations.text("account.no_guests"),
                          {engine::AnchorPoint::TopLeft, columnLeft, top + 10.0f}, Tone::Hint);
            }
        });

    declareFormScenes(style);
    declareDiscordScenes(style);
    declareMyAccountScenes(style);
}

void OnlineScenes::declareFormScenes(const engine::InterfaceStyle &style)
{
    const auto backTo = [this](const char *scene) {
        return engine::SceneSettings{.onBack = [this, scene] { _engine.switchScene(scene); }};
    };
    // Discord on the right of a form, beyond an "or", if the server offers it
    const auto discordBeside = [this](OnlineScreen &page, float top, float bottom) {
        if (!_lobby || !_lobby->features().discord) {
            return;
        }
        page.divider(top, bottom, _translations.text("account.or"));
        const float middle = (top + bottom) / 2.0f;
        page.discordButton(
            _translations.text("account.discord"), [this] { startDiscord(DiscordPurpose::SignIn); },
            {engine::AnchorPoint::TopLeft, rightLeft, middle - 40.0f}, _discordLogo);
        page.note(_translations.text("account.discord_beside"),
                  {engine::AnchorPoint::TopLeft, rightLeft, middle + 34.0f}, Tone::Hint);
    };

    // E2: the login and the password, the error under the field it is about
    _engine.createScene(
        SignInScene, backTo(AccountsScene),
        [this, discordBeside](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.sign_in"),
                        [this] { _engine.switchScene(AccountsScene); });
            const ecs::entity_t login =
                page.field(_translations.text("account.login"), _login, 0, lobby::MaxEmailLength);
            const auto password = std::make_shared<ecs::entity_t>(0);
            const auto submit = [this, &target, login, password] {
                if (!_lobby) {
                    return;
                }
                _login = textOf(target, login);
                const std::map<FormField, ecs::entity_t> fields{{FormField::Login, login},
                                                                {FormField::Password, *password}};
                _lobby->signIn(_login, textOf(target, *password),
                               [this, fields](const LobbyClient::SignInResult &result) {
                                   // The right password, an email not verified: a new code was sent
                                   if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
                                       if (*error == lobby::ErrorCode::EmailNotVerified) {
                                           _engine.showToast(message(*error), engine::ToastType::Warning);
                                           _email = _login;
                                           _engine.switchScene(CodeScene);
                                       } else {
                                           showError(*error, fields);
                                       }
                                       return;
                                   }
                                   signedIn(result);
                               });
            };
            *password = page.password(_translations.text("account.password"), 1, MaxPasswordCharacters,
                                      [submit](const std::string &) { submit(); });
            page.button(_translations.text("account.sign_in"), submit,
                        {engine::AnchorPoint::TopLeft, columnLeft, buttonsTop(2)});
            page.button(
                _translations.text("account.forgot"), [this] { _engine.switchScene(ForgotPasswordScene); },
                {engine::AnchorPoint::TopLeft, columnLeft, buttonsTop(2) + online_layout::buttonsSpacing});
            discordBeside(page, online_layout::fieldsTop, 780.0f);
        });

    // E3: the username, the email and the password, its strength in a gauge
    _engine.createScene(
        RegisterScene, backTo(AccountsScene),
        [this, &style, discordBeside](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.create"), [this] { _engine.switchScene(AccountsScene); });
            const ecs::entity_t username =
                page.field(_translations.text("account.username"), _name, 0, lobby::MaxUsernameLength);
            const ecs::entity_t email =
                page.field(_translations.text("account.email"), _email, 1, lobby::MaxEmailLength);
            const ecs::entity_t password =
                page.password(_translations.text("account.password"), 2, MaxPasswordCharacters);
            const float gaugeTop = buttonsTop(3) - 10.0f;
            page.ui().gauge(
                toCapitals(_translations.text("account.strength")),
                [&target, password] {
                    ecs::Ecs &ecs = target.getEcs();
                    if (!ecs.isAlive(password) || !ecs.hasComponent<engine::TextField>(password)) {
                        return 0.0f;
                    }
                    const std::size_t length = ecs.getComponent<engine::TextField>(password).input.length();
                    return std::min(1.0f, static_cast<float>(length) / MinPasswordCharacters);
                },
                {engine::AnchorPoint::TopLeft, columnLeft, gaugeTop});
            page.note(_translations.text("account.password_rules"),
                      {engine::AnchorPoint::TopLeft, columnLeft, gaugeTop + style.gauge.getHeight() + 12.0f},
                      Tone::Hint, 20.0f);
            page.button(
                _translations.text("account.create"),
                [this, &target, username, email, password] {
                    if (!_lobby) {
                        return;
                    }
                    _login = textOf(target, username);
                    _email = textOf(target, email);
                    const std::map<FormField, ecs::entity_t> fields{{FormField::Username, username},
                                                                    {FormField::Email, email},
                                                                    {FormField::Password, password}};
                    _lobby->registerAccount(_login, _email, textOf(target, password),
                                            [this, fields](std::optional<lobby::ErrorCode> error) {
                                                if (error) {
                                                    showError(*error, fields);
                                                    return;
                                                }
                                                _engine.switchScene(CodeScene);
                                            });
                },
                {engine::AnchorPoint::TopLeft, columnLeft, gaugeTop + style.gauge.getHeight() + 70.0f});
            discordBeside(page, online_layout::fieldsTop, 820.0f);
        });

    // C1: six cells for the code of the email, sent as soon as they are full
    _engine.createScene(
        CodeScene, backTo(AccountsScene), [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.verify_email"),
                        [this] { _engine.switchScene(AccountsScene); });
            const std::string to = _email.find('@') != std::string::npos ? maskEmail(_email) : _login;
            page.note(std::format("{} {}.", _translations.text("account.code_sent_to"), to),
                      {engine::AnchorPoint::TopLeft, columnLeft, 236.0f}, Tone::Detail, 24.0f);
            page.note(_translations.text("account.code_expiry"),
                      {engine::AnchorPoint::TopLeft, columnLeft, 274.0f}, Tone::Hint);
            const auto code = std::make_shared<ecs::entity_t>(0);
            const auto verify = [this, code](const std::string &digits) {
                if (_lobby) {
                    _lobby->verifyEmail(_login, digits,
                                        [this, code](const LobbyClient::SignInResult &result) {
                                            if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
                                                showError(*error, {{FormField::Code, *code}});
                                                return;
                                            }
                                            signedIn(result);
                                        });
                }
            };
            *code = page.ui().codeField(toCapitals(_translations.text("account.code")), lobby::CodeLength,
                                        verify, {engine::AnchorPoint::TopLeft, columnLeft, 340.0f});
            page.button(_translations.text("account.verify"),
                        [&target, code, verify] { verify(textOf(target, *code)); },
                        {engine::AnchorPoint::TopLeft, columnLeft, 540.0f});
            page.button(_translations.text("account.resend"),
                        [this, code] {
                            if (_lobby) {
                                _lobby->resendCode(
                                    _login, [this, code](std::optional<lobby::ErrorCode> error) {
                                        if (error) {
                                            showError(*error, {{FormField::Code, *code}});
                                            return;
                                        }
                                        _engine.showToast(_translations.text("account.code_sent"));
                                    });
                            }
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, 540.0f + online_layout::buttonsSpacing});
        });

    _engine.createScene(
        ForgotPasswordScene, backTo(SignInScene), [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.forgot"), [this] { _engine.switchScene(SignInScene); });
            page.note(_translations.text("account.forgot_note"),
                      {engine::AnchorPoint::TopLeft, columnLeft, 200.0f});
            const ecs::entity_t email =
                page.field(_translations.text("account.email"), _email, 0, lobby::MaxEmailLength);
            page.button(_translations.text("account.send_code"),
                        [this, &target, email] {
                            if (!_lobby) {
                                return;
                            }
                            _email = textOf(target, email);
                            _lobby->forgotPassword(_email,
                                                   [this, email](std::optional<lobby::ErrorCode> error) {
                                                       if (error) {
                                                           showError(*error, {{FormField::Email, email}});
                                                           return;
                                                       }
                                                       _engine.switchScene(ResetPasswordScene);
                                                   });
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, buttonsTop(1)});
        });

    _engine.createScene(
        ResetPasswordScene, backTo(SignInScene), [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.new_password"),
                        [this] { _engine.switchScene(SignInScene); });
            page.note(std::format("{} {}.", _translations.text("account.code_sent_to"), maskEmail(_email)),
                      {engine::AnchorPoint::TopLeft, columnLeft, 200.0f});
            const ecs::entity_t code =
                page.ui().codeField(toCapitals(_translations.text("account.code")), lobby::CodeLength, {},
                                    {engine::AnchorPoint::TopLeft, columnLeft, 260.0f});
            const ecs::entity_t password =
                page.password(_translations.text("account.new_password"), 2, MaxPasswordCharacters);
            page.note(_translations.text("account.password_rules"),
                      {engine::AnchorPoint::TopLeft, columnLeft, buttonsTop(3) - 20.0f}, Tone::Hint, 20.0f);
            page.button(_translations.text("account.reset"),
                        [this, &target, code, password] {
                            if (_lobby) {
                                const std::map<FormField, ecs::entity_t> fields{
                                    {FormField::Code, code}, {FormField::Password, password}};
                                _lobby->resetPassword(
                                    _email, textOf(target, code), textOf(target, password),
                                    [this, fields](const LobbyClient::SignInResult &result) {
                                        if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
                                            showError(*error, fields);
                                            return;
                                        }
                                        signedIn(result);
                                    });
                            }
                        },
                        {engine::AnchorPoint::TopLeft, columnLeft, buttonsTop(3) + 30.0f});
        });
}

void OnlineScenes::declareDiscordScenes(const engine::InterfaceStyle &style)
{
    // D1: the ring of the logo while the player accepts in Discord, and where the confirmation stands
    const auto leaveDiscord = [this] {
        _discord.cancel();
        if (_engine.isSceneOpen(DiscordScene)) {
            _engine.popScene();
        }
    };
    _engine.createScene(
        DiscordScene,
        {.overlay = true, .hidesInterfaceBelow = true, .updatesWhenCovered = false, .onBack = leaveDiscord},
        [this, &style, leaveDiscord](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page(target, scene, style);
            page.veil();
            constexpr float ringSize = 240.0f;
            constexpr float ringTop = 200.0f;
            // A wider ring that breathes around the logo, while Discord answers
            const ecs::entity_t halo =
                page.ui().avatar({.kind = engine::AvatarKind::Discord, .size = ringSize + 40.0f},
                                 {engine::AnchorPoint::Top, 0.0f, ringTop - 20.0f});
            target.getEcs().addComponent<engine::Opacity>(halo, 0.2f);
            target.getEcs().addComponent<engine::Tween>(halo,
                                                        engine::Tween{.target = engine::TweenTarget::Opacity,
                                                                      .from = 0.15f,
                                                                      .to = 0.6f,
                                                                      .duration = 1.2f,
                                                                      .easing = engine::Easing::Linear,
                                                                      .pingPong = true});
            page.ui().avatar({.kind = engine::AvatarKind::Discord, .size = ringSize},
                             {engine::AnchorPoint::Top, 0.0f, ringTop});
            constexpr float logoSize = 96.0f;
            const ecs::entity_t logo =
                page.ui().sprite(_discordLogo, {0.0f, 0.0f, 128.0f, 128.0f}, logoSize, logoSize,
                                 {engine::AnchorPoint::Top, 0.0f, ringTop + ((ringSize - logoSize) / 2.0f)});
            // Over the disc of the ring
            target.getEcs().getComponent<engine::Drawable>(logo).order = 2;
            page.question(_translations.text("account.discord_title"), 520.0f);
            page.note(_translations.text("account.discord_waiting"), {engine::AnchorPoint::Top, 0.0f, 600.0f},
                      Tone::Detail, 24.0f);
            // The three steps: those done in grey, the current one lit, those to come dark
            constexpr const char *steps[] = {"account.discord_step_open", "account.discord_step_accept",
                                             "account.discord_step_check"};
            for (int step = 0; step < 3; ++step) {
                const Tone tone = step < _discordStep    ? Tone::Detail
                                  : step == _discordStep ? Tone::Bright
                                                         : Tone::Hint;
                page.note(toCapitals(_translations.text(steps[step])),
                          {engine::AnchorPoint::Top, (static_cast<float>(step) - 1.0f) * 420.0f, 680.0f},
                          tone, 20.0f);
            }
            page.button(_translations.text("online.cancel"), leaveDiscord,
                        {engine::AnchorPoint::Top, 0.0f, 780.0f});
            page.note(toCapitals(_translations.text("account.discord_browser")),
                      {engine::AnchorPoint::Bottom, 0.0f, -84.0f}, Tone::Hint, 20.0f);
        });

    // D2: the Discord profile of a new user above the username it chooses, proposed from Discord
    _engine.createScene(
        ChooseUsernameScene, {.onBack = [this] { _engine.switchScene(AccountsScene); }},
        [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.new_pilot"),
                        [this] { _engine.switchScene(AccountsScene); });
            const std::string discordName = _newUser ? _newUser->discordUsername : std::string();
            const std::string shownName =
                _newUser && !_newUser->displayName.empty() ? _newUser->displayName : discordName;
            constexpr float avatarSize = 280.0f;
            page.ui().avatar({.picture = _newUserPicture,
                              .initial = initialOf(shownName),
                              .kind = engine::AvatarKind::Discord,
                              .size = avatarSize},
                             {engine::AnchorPoint::TopLeft, 240.0f, 300.0f});
            constexpr float textLeft = 760.0f;
            page.note(toCapitals(_translations.text("account.welcome")),
                      {engine::AnchorPoint::TopLeft, textLeft, 264.0f}, Tone::Detail, 20.0f);
            const ecs::entity_t name =
                page.ui().title(toCapitals(shownName), {engine::AnchorPoint::TopLeft, textLeft, 296.0f});
            target.getEcs().getComponent<engine::Text>(name).size = 52.0f;
            if (!discordName.empty()) {
                page.ui().sprite(_discordLogo, {0.0f, 0.0f, 128.0f, 128.0f}, 24.0f, 24.0f,
                                 {engine::AnchorPoint::TopLeft, textLeft, 376.0f});
                page.note(discordName, {engine::AnchorPoint::TopLeft, textLeft + 36.0f, 376.0f});
            }
            const ecs::entity_t username = page.ui().textField(
                toCapitals(_translations.text("account.pilot_name")), proposedUsername(discordName, _name),
                {engine::AnchorPoint::TopLeft, textLeft, 470.0f}, lobby::MaxUsernameLength);
            page.note(_translations.text("account.pilot_name_rules"),
                      {engine::AnchorPoint::TopLeft, textLeft, 586.0f}, Tone::Hint, 20.0f);
            page.button(_translations.text("account.choose_create"),
                        [this, &target, username] {
                            if (_lobby) {
                                _lobby->chooseUsername(
                                    textOf(target, username),
                                    [this, username](const LobbyClient::SignInResult &result) {
                                        if (const auto *error = std::get_if<lobby::ErrorCode>(&result)) {
                                            showError(*error, {{FormField::Username, username}});
                                            return;
                                        }
                                        signedIn(result);
                                    });
                            }
                        },
                        {engine::AnchorPoint::TopLeft, textLeft, 660.0f});
            page.button(_translations.text("online.cancel"), [this] { _engine.switchScene(AccountsScene); },
                        {engine::AnchorPoint::TopLeft, textLeft, 660.0f + online_layout::buttonsSpacing});
        });
}

void OnlineScenes::declareMyAccountScenes(const engine::InterfaceStyle &style)
{
    // A1: the profile of the pilot on the left, what the server knows of the account on the right
    _engine.createScene(
        MyAccountScene, {.onBack = [this] { refresh(); }},
        [this](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page = screen(target, scene);
            page.header(_translations.text("account.mine"), [this] { refresh(); });
            if (!_lobby || !_lobby->account()) {
                return;
            }
            const lobby::AccountInfo &account = *_lobby->account();
            constexpr float avatarSize = 280.0f;
            constexpr float avatarLeft = 120.0f;
            constexpr float avatarTop = 250.0f;
            page.ui().avatar(accountAvatar(avatarSize),
                             {engine::AnchorPoint::TopLeft, avatarLeft, avatarTop});
            // Centred under the avatar: the Top anchor centres, from the middle of the screen
            const float under = avatarLeft + (avatarSize / 2.0f) - 960.0f;
            const ecs::entity_t name =
                page.ui().title(toCapitals(account.username),
                                {engine::AnchorPoint::Top, under, avatarTop + avatarSize + 40.0f});
            target.getEcs().getComponent<engine::Text>(name).size = 48.0f;
            if (_profile && !_profile->displayName.empty()) {
                page.note(_profile->displayName,
                          {engine::AnchorPoint::Top, under, avatarTop + avatarSize + 110.0f});
            }
            if (account.role == lobby::AccountRole::Admin) {
                page.note(toCapitals(_translations.text("account.role_admin")),
                          {engine::AnchorPoint::Top, under, avatarTop + avatarSize + 150.0f}, Tone::Bright,
                          20.0f);
            }

            // The sheet: a caption on the left, its value on the right, a rule under each line
            constexpr float sheetLeft = 760.0f;
            constexpr float sheetWidth = 1000.0f;
            constexpr float rowHeight = 76.0f;
            const bool discord = _profile && _profile->discord;
            std::string email =
                account.email.empty() ? _translations.text("account.none") : maskEmail(account.email);
            if (!account.email.empty() && account.emailVerified) {
                email += " · " + _translations.text("account.verified");
            }
            const std::vector<std::pair<std::string, std::string>> rows{
                {_translations.text("account.row_server"), _serverText},
                {_translations.text("account.row_since"), formatDate(account.createdAt)},
                {_translations.text("account.email"), email},
                {_translations.text("account.password"),
                 _translations.text(account.hasPassword ? "account.password_set" : "account.password_none")},
                {"Discord",
                 discord ? "@" + _profile->discordUsername : _translations.text("account.discord_none")},
                {_translations.text("account.row_role"),
                 _translations.text(account.role == lobby::AccountRole::Admin ? "account.role_admin"
                                                                              : "account.role_player")},
            };
            float top = 250.0f;
            for (const auto &[caption, value] : rows) {
                page.note(caption, {engine::AnchorPoint::TopLeft, sheetLeft, top + 20.0f}, Tone::Detail,
                          24.0f);
                const ecs::entity_t shown = page.note(
                    toCapitals(value),
                    {engine::AnchorPoint::TopRight, -(1920.0f - sheetLeft - sheetWidth), top + 22.0f},
                    Tone::Bright, 22.0f);
                target.getEcs().getComponent<engine::Text>(shown).font = _style.field.valueFont;
                page.rule({engine::AnchorPoint::TopLeft, sheetLeft, top + rowHeight - 6.0f}, sheetWidth,
                          {51, 55, 59, 255});
                top += rowHeight;
            }

            float buttons = top + 30.0f;
            if (_lobby->features().discord) {
                page.discordButton(
                    _translations.text(discord ? "account.discord_refresh" : "account.discord_link"),
                    [this] { startDiscord(DiscordPurpose::Link); },
                    {engine::AnchorPoint::TopLeft, sheetLeft, buttons}, _discordLogo);
                buttons += online_layout::buttonsSpacing;
            }
            page.button(_translations.text("account.sign_out"),
                        [this] {
                            if (_lobby) {
                                _lobby->signOut([this](std::optional<lobby::ErrorCode> error) {
                                    signedOut(error, "account.signed_out");
                                });
                            }
                        },
                        {engine::AnchorPoint::TopLeft, sheetLeft, buttons});
            page.dangerButton(_translations.text("account.delete"),
                              [this] { _engine.pushScene(DeleteAccountScene); },
                              {engine::AnchorPoint::TopLeft, sheetLeft + 520.0f, buttons});
        });

    // A2: the deletion asked over My account, in red, No by default
    _engine.createScene(
        DeleteAccountScene, {.overlay = true, .hidesInterfaceBelow = true},
        [this, &style](engine::Engine &target, engine::SceneId scene) {
            OnlineScreen page(target, scene, style);
            page.veil();
            const ecs::entity_t title =
                page.note(toCapitals(_translations.text("account.delete_question")),
                          {engine::AnchorPoint::Top, 0.0f, 190.0f}, Tone::Alert, 44.0f);
            target.getEcs().getComponent<engine::Text>(title).font = style.field.valueFont;
            page.rule({engine::AnchorPoint::Top, 0.0f, 262.0f}, 800.0f, {232, 114, 110, 200});
            const std::string username = _lobby ? _lobby->name() : _name;
            page.note(
                std::format("{} {}", toCapitals(username), _translations.text("account.delete_warning")),
                {engine::AnchorPoint::Top, 0.0f, 300.0f}, Tone::Detail, 24.0f);
            page.note(_translations.text("account.delete_kept"), {engine::AnchorPoint::Top, 0.0f, 338.0f},
                      Tone::Detail, 24.0f);

            const bool hasPassword = _lobby && _lobby->account() && _lobby->account()->hasPassword;
            const auto password = std::make_shared<ecs::entity_t>(0);
            // No is first: it has the focus when the question appears
            page.button(_translations.text("account.no"), [&target] { target.popScene(); },
                        {engine::AnchorPoint::Top, -260.0f, 780.0f});
            if (hasPassword) {
                *password =
                    page.ui().passwordField(toCapitals(_translations.text("account.password")),
                                            {engine::AnchorPoint::Top, 0.0f, 420.0f}, MaxPasswordCharacters);
            }
            if (_lobby && _lobby->features().discord) {
                page.note(toCapitals(_translations.text(hasPassword ? "account.delete_discord_or"
                                                                    : "account.delete_discord_only")),
                          {engine::AnchorPoint::Top, 0.0f, 588.0f}, Tone::Hint, 20.0f);
                page.discordButton(
                    _translations.text("account.discord_delete"),
                    [this] { startDiscord(DiscordPurpose::DeleteAccount); },
                    {engine::AnchorPoint::Top, -40.0f, 626.0f}, _discordLogo);
            }
            if (hasPassword) {
                page.dangerButton(_translations.text("account.delete_confirm"),
                                  [this, &target, password] {
                                      if (_lobby) {
                                          _lobby->deleteAccount(
                                              textOf(target, *password),
                                              [this, password](std::optional<lobby::ErrorCode> error) {
                                                  if (error) {
                                                      showError(*error, {{FormField::Password, *password}});
                                                      return;
                                                  }
                                                  signedOut(error, "account.deleted");
                                              });
                                      }
                                  },
                                  {engine::AnchorPoint::Top, 260.0f, 780.0f});
            }
        });
}

} // namespace rtype::client
