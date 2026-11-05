#include "Accounts/MailMessages.hpp"

#include <format>
#include <vector>

namespace rtype::server
{

namespace
{

/// What a mail says, before it is written as text and as HTML.
struct Body {
    MailLanguage language = MailLanguage::English;
    std::string subject;
    std::string greeting;
    std::vector<std::string> before;
    /// Shown large, on a line of its own; empty: none.
    std::string code;
    std::vector<std::string> after;
    /// The line that says what to do if the mail was not expected, smaller.
    std::string footer;
};

std::string escapeHtml(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        switch (c) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += c;
        }
    }
    return escaped;
}

std::string text(const Body &body)
{
    std::string result = body.greeting + "\n\n";
    for (const std::string &paragraph : body.before) {
        result += paragraph + "\n\n";
    }
    if (!body.code.empty()) {
        result += "    " + body.code + "\n\n";
    }
    for (const std::string &paragraph : body.after) {
        result += paragraph + "\n\n";
    }
    result += body.footer + "\n\n-- \nR-Type\n";
    return result;
}

// The colours of the game: black, white, the silver of the secondary text. Inline styles only, as mail
// clients keep no others
std::string html(const Body &body)
{
    const auto paragraph = [](std::string_view content) {
        return std::format("<p style=\"margin:0 0 16px 0\">{}</p>\n", escapeHtml(content));
    };
    std::string result = std::format(
        "<!DOCTYPE html>\n<html lang=\"{}\">\n<head>\n<meta charset=\"utf-8\">\n<title>{}</title>\n</head>\n"
        "<body style=\"margin:0;padding:32px 24px;background:#000000;color:#ffffff;"
        "font-family:Helvetica,Arial,sans-serif;font-size:15px;line-height:1.5\">\n",
        body.language == MailLanguage::French ? "fr" : "en", escapeHtml(body.subject));
    result += paragraph(body.greeting);
    for (const std::string &content : body.before) {
        result += paragraph(content);
    }
    if (!body.code.empty()) {
        result += std::format("<p style=\"margin:24px 0;font-family:'Courier New',Courier,monospace;"
                              "font-size:40px;font-weight:bold;letter-spacing:10px\">{}</p>\n",
                              escapeHtml(body.code));
    }
    for (const std::string &content : body.after) {
        result += paragraph(content);
    }
    result += std::format("<p style=\"margin:32px 0 0 0;color:#8a9097;font-size:13px\">{}</p>\n",
                          escapeHtml(body.footer));
    result += "</body>\n</html>\n";
    return result;
}

MailContent write(const Body &body)
{
    return {.subject = body.subject, .text = text(body), .html = html(body)};
}

std::string minutes(MailLanguage language, std::chrono::minutes duration)
{
    const auto count = duration.count();
    if (language == MailLanguage::French) {
        return std::format("{} minute{}", count, count > 1 ? "s" : "");
    }
    return std::format("{} minute{}", count, count == 1 ? "" : "s");
}

std::string greeting(MailLanguage language, std::string_view username)
{
    return language == MailLanguage::French ? std::format("Bonjour {},", username)
                                            : std::format("Hello {},", username);
}

} // namespace

MailLanguage mailLanguage(std::string_view locale)
{
    const bool french = locale.size() >= 2 && (locale[0] == 'f' || locale[0] == 'F') &&
                        (locale[1] == 'r' || locale[1] == 'R') &&
                        (locale.size() == 2 || locale[2] == '-' || locale[2] == '_');
    return french ? MailLanguage::French : MailLanguage::English;
}

MailContent verificationMail(MailLanguage language, std::string_view username, std::string_view code,
                             std::chrono::minutes validFor)
{
    if (language == MailLanguage::French) {
        return write({
            .language = language,
            .subject = "Votre code de vérification R-Type",
            .greeting = greeting(language, username),
            .before = {"Voici le code qui vérifie l'adresse de votre compte R-Type :"},
            .code = std::string(code),
            .after = {std::format("Tapez-le dans le jeu. Il expire dans {}.", minutes(language, validFor))},
            .footer = "Si vous n'avez pas créé de compte R-Type, ignorez ce message : aucun compte ne sera "
                      "gardé sans ce code.",
        });
    }
    return write({
        .language = language,
        .subject = "Your R-Type verification code",
        .greeting = greeting(language, username),
        .before = {"Here is the code that verifies the email of your R-Type account:"},
        .code = std::string(code),
        .after = {std::format("Type it in the game. It expires in {}.", minutes(language, validFor))},
        .footer = "If you did not create an R-Type account, ignore this email: no account is kept without "
                  "this code.",
    });
}

MailContent resetMail(MailLanguage language, std::string_view username, std::string_view code,
                      std::chrono::minutes validFor)
{
    if (language == MailLanguage::French) {
        return write({
            .language = language,
            .subject = "Votre code R-Type pour un nouveau mot de passe",
            .greeting = greeting(language, username),
            .before =
                {"Voici le code qui permet de choisir un nouveau mot de passe pour votre compte R-Type :"},
            .code = std::string(code),
            .after = {std::format("Tapez-le dans le jeu. Il expire dans {}.", minutes(language, validFor))},
            .footer = "Si vous n'avez pas demandé de nouveau mot de passe, ignorez ce message : votre mot de "
                      "passe ne change pas.",
        });
    }
    return write({
        .language = language,
        .subject = "Your R-Type password reset code",
        .greeting = greeting(language, username),
        .before = {"Here is the code that lets you choose a new password for your R-Type account:"},
        .code = std::string(code),
        .after = {std::format("Type it in the game. It expires in {}.", minutes(language, validFor))},
        .footer = "If you did not ask for a new password, ignore this email: your password does not change.",
    });
}

MailContent existingAccountMail(MailLanguage language, std::string_view username)
{
    if (language == MailLanguage::French) {
        return write({
            .language = language,
            .subject = "Vous avez déjà un compte R-Type",
            .greeting = greeting(language, username),
            .before = {"Quelqu'un a voulu créer un compte R-Type avec cette adresse, qui en a déjà un : le "
                       "vôtre, à ce nom.",
                       "Si c'était vous, connectez-vous avec ce compte. Si vous avez oublié votre mot de "
                       "passe, demandez-en un nouveau depuis l'écran de connexion du jeu."},
            .code = {},
            .after = {},
            .footer = "Si ce n'était pas vous, ignorez ce message : rien ne change sur votre compte.",
        });
    }
    return write({
        .language = language,
        .subject = "You already have an R-Type account",
        .greeting = greeting(language, username),
        .before = {"Someone tried to create an R-Type account with this email, which has one already: yours, "
                   "under this name.",
                   "If it was you, sign in with this account. If you forgot your password, ask for a new one "
                   "from the sign-in screen of the game."},
        .code = {},
        .after = {},
        .footer = "If it was not you, ignore this email: nothing changes on your account.",
    });
}

MailContent tooManyCodesMail(MailLanguage language, std::string_view username)
{
    if (language == MailLanguage::French) {
        return write({
            .language = language,
            .subject = "Trop de codes erronés sur votre compte R-Type",
            .greeting = greeting(language, username),
            .before = {"Trop de codes erronés ont été tapés aujourd'hui pour votre compte R-Type. Plus aucun "
                       "code n'est vérifié avant demain (minuit UTC).",
                       "Un code erroné n'ouvre rien : votre mot de passe ne change pas."},
            .code = {},
            .after = {},
            .footer = "Si ce n'était pas vous, quelqu'un essaie peut-être de deviner vos codes : ne donnez à "
                      "personne ceux que vous recevez.",
        });
    }
    return write({
        .language = language,
        .subject = "Too many wrong codes on your R-Type account",
        .greeting = greeting(language, username),
        .before =
            {"Too many wrong codes were typed today for your R-Type account. No code is checked any more "
             "until tomorrow (midnight UTC).",
             "A wrong code opens nothing: your password does not change."},
        .code = {},
        .after = {},
        .footer = "If it was not you, someone may be trying to guess your codes: never give anyone those you "
                  "receive.",
    });
}

} // namespace rtype::server
