// The scenes of the accounts of OnlineScenes (docs/architecture/client-online.md#accounts): what the server
// offers, sign in, create an account, the code of the email, the forgotten password, and the account signed
// in.

#include "Online/OnlineScenes.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"

#include <format>
#include <string>
#include <variant>

namespace rtype::client
{

namespace
{

/// Longest password typed, in characters: what the master accepts.
constexpr std::size_t MaxPasswordCharacters = 128;

/// Reads the text of a field of a menu.
std::string textOf(engine::Engine &engine, ecs::entity_t field)
{
    return engine.getEcs().getComponent<engine::TextField>(field).input.text();
}

/// A field for a code sent by email: its 6 digits only.
ecs::entity_t codeField(engine::MenuBuilder &menu, const std::string &caption)
{
    return menu.textField(caption, "", {}, lobby::CodeLength);
}

void digitsOnly(engine::Engine &engine, ecs::entity_t field)
{
    engine.getEcs().getComponent<engine::TextField>(field).input.setFilter(
        [](char32_t character) { return character >= U'0' && character <= U'9'; });
}

} // namespace

std::string OnlineScenes::message(lobby::ErrorCode code) const
{
    return _translations.text(errorKey(code));
}

void OnlineScenes::welcomed()
{
    if (_lobby && !_lobby->account() && _lobby->features().passwordAccounts) {
        _engine.switchScene(AccountsScene);
        return;
    }
    refresh();
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
    refresh();
}

void OnlineScenes::signedOut(std::optional<lobby::ErrorCode> error, const char *done)
{
    if (error) {
        _engine.showToast(message(*error), engine::ToastType::Error);
        return;
    }
    _sessions.forget(_address);
    _engine.showToast(_translations.text(done));
    // The master closed the connection: back as a guest
    connect();
}

void OnlineScenes::declareAccountScenes(const engine::Metrics &metrics, const engine::InterfaceStyle &style)
{
    const auto backTo = [this](const char *scene) {
        return engine::SceneSettings{.onBack = [this, scene] { _engine.switchScene(scene); }};
    };

    // What the server offers, for a player not signed in
    _engine.createScene(
        AccountsScene, {.onBack = [this] { disconnect(); }},
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("menu.online"));
            menu.label(_serverText);
            menu.button(_translations.text("account.sign_in"), [this] { _engine.switchScene(SignInScene); });
            menu.button(_translations.text("account.create"), [this] { _engine.switchScene(RegisterScene); });
            if (_lobby && _lobby->features().guests) {
                menu.button(_translations.text("account.guest"), [this] { refresh(); });
            } else {
                menu.label(_translations.text("account.no_guests"));
            }
            menu.button(_translations.text("online.disconnect"), [this] { disconnect(); });
        });

    _engine.createScene(
        SignInScene, backTo(AccountsScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.sign_in"));
            const ecs::entity_t login =
                menu.textField(_translations.text("account.login"), _login, {}, lobby::MaxEmailLength);
            const ecs::entity_t password =
                menu.passwordField(_translations.text("account.password"), MaxPasswordCharacters);
            menu.button(_translations.text("account.sign_in"), [this, &target, login, password] {
                if (!_lobby) {
                    return;
                }
                _login = textOf(target, login);
                _lobby->signIn(_login, textOf(target, password),
                               [this](const LobbyClient::SignInResult &result) {
                                   // The right password, an email not verified: a new code was sent
                                   if (const auto *error = std::get_if<lobby::ErrorCode>(&result);
                                       error && *error == lobby::ErrorCode::EmailNotVerified) {
                                       _engine.showToast(message(*error), engine::ToastType::Warning);
                                       _engine.switchScene(CodeScene);
                                       return;
                                   }
                                   signedIn(result);
                               });
            });
            menu.button(_translations.text("account.forgot"),
                        [this] { _engine.switchScene(ForgotPasswordScene); });
            menu.button(_translations.text("online.back"), [this] { _engine.switchScene(AccountsScene); });
        });

    _engine.createScene(
        RegisterScene, backTo(AccountsScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.create"));
            const ecs::entity_t username =
                menu.textField(_translations.text("account.username"), _name, {}, lobby::MaxUsernameLength);
            const ecs::entity_t email =
                menu.textField(_translations.text("account.email"), "", {}, lobby::MaxEmailLength);
            const ecs::entity_t password =
                menu.passwordField(_translations.text("account.password"), MaxPasswordCharacters);
            menu.label(_translations.text("account.password_rules"));
            menu.button(_translations.text("account.create"), [this, &target, username, email, password] {
                if (!_lobby) {
                    return;
                }
                _login = textOf(target, username);
                _lobby->registerAccount(_login, textOf(target, email), textOf(target, password),
                                        [this](std::optional<lobby::ErrorCode> error) {
                                            if (error) {
                                                _engine.showToast(message(*error), engine::ToastType::Error);
                                                return;
                                            }
                                            _engine.switchScene(CodeScene);
                                        });
            });
            menu.button(_translations.text("online.back"), [this] { _engine.switchScene(AccountsScene); });
        });

    _engine.createScene(
        CodeScene, backTo(AccountsScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.code"));
            menu.label(_translations.text("account.code_sent"));
            menu.label(_translations.text("account.code_expiry"));
            const ecs::entity_t code = codeField(menu, _translations.text("account.code"));
            digitsOnly(target, code);
            menu.button(_translations.text("account.verify"), [this, &target, code] {
                if (_lobby) {
                    _lobby->verifyEmail(
                        _login, textOf(target, code),
                        [this](const LobbyClient::SignInResult &result) { signedIn(result); });
                }
            });
            menu.button(_translations.text("account.resend"), [this] {
                if (_lobby) {
                    _lobby->resendCode(_login, [this](std::optional<lobby::ErrorCode> error) {
                        _engine.showToast(error ? message(*error) : _translations.text("account.code_sent"),
                                          error ? engine::ToastType::Error : engine::ToastType::Info);
                    });
                }
            });
            menu.button(_translations.text("online.back"), [this] { _engine.switchScene(AccountsScene); });
        });

    _engine.createScene(
        ForgotPasswordScene, backTo(SignInScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.forgot"));
            const ecs::entity_t email =
                menu.textField(_translations.text("account.email"), _email, {}, lobby::MaxEmailLength);
            menu.button(_translations.text("account.send_code"), [this, &target, email] {
                if (!_lobby) {
                    return;
                }
                _email = textOf(target, email);
                _lobby->forgotPassword(_email, [this](std::optional<lobby::ErrorCode> error) {
                    if (error) {
                        _engine.showToast(message(*error), engine::ToastType::Error);
                        return;
                    }
                    _engine.switchScene(ResetPasswordScene);
                });
            });
            menu.button(_translations.text("online.back"), [this] { _engine.switchScene(SignInScene); });
        });

    _engine.createScene(
        ResetPasswordScene, backTo(SignInScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.new_password"));
            menu.label(_translations.text("account.code_sent"));
            const ecs::entity_t code = codeField(menu, _translations.text("account.code"));
            digitsOnly(target, code);
            const ecs::entity_t password =
                menu.passwordField(_translations.text("account.new_password"), MaxPasswordCharacters);
            menu.label(_translations.text("account.password_rules"));
            menu.button(_translations.text("account.reset"), [this, &target, code, password] {
                if (_lobby) {
                    _lobby->resetPassword(
                        _email, textOf(target, code), textOf(target, password),
                        [this](const LobbyClient::SignInResult &result) { signedIn(result); });
                }
            });
            menu.button(_translations.text("online.back"), [this] { _engine.switchScene(SignInScene); });
        });

    _engine.createScene(
        MyAccountScene, {.onBack = [this] { refresh(); }},
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.mine"));
            if (_lobby && _lobby->account()) {
                const lobby::AccountInfo &account = *_lobby->account();
                menu.label(std::format("{}: {}", _translations.text("account.username"), account.username));
                if (account.role == lobby::AccountRole::Admin) {
                    menu.label(_translations.text("account.admin"));
                }
            }
            menu.button(_translations.text("account.sign_out"), [this] {
                if (_lobby) {
                    _lobby->signOut([this](std::optional<lobby::ErrorCode> error) {
                        signedOut(error, "account.signed_out");
                    });
                }
            });
            menu.button(_translations.text("account.delete"),
                        [this] { _engine.switchScene(DeleteAccountScene); });
            menu.button(_translations.text("online.back"), [this] { refresh(); });
        });

    _engine.createScene(
        DeleteAccountScene, backTo(MyAccountScene),
        [this, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
            engine::MenuBuilder menu(target.getEcs(), scene, metrics, style,
                                     _translations.text("account.delete"));
            menu.label(_translations.text("account.delete_warning"));
            const ecs::entity_t password =
                menu.passwordField(_translations.text("account.password"), MaxPasswordCharacters);
            menu.button(_translations.text("account.delete_confirm"), [this, &target, password] {
                if (_lobby) {
                    _lobby->deleteAccount(textOf(target, password),
                                          [this](std::optional<lobby::ErrorCode> error) {
                                              signedOut(error, "account.deleted");
                                          });
                }
            });
            menu.button(_translations.text("trust.cancel"), [this] { _engine.switchScene(MyAccountScene); });
        });
}

} // namespace rtype::client
