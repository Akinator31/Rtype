#include "Accounts/Mailer.hpp"

#include "Common/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <iterator>

namespace rtype::server
{

namespace
{

/// The longest line of base64 MIME allows.
constexpr std::size_t Base64LineLength = 76;
/// The bytes of one encoded word: 60 characters of base64, 72 with its markers, under the 75 of RFC 2047.
constexpr std::size_t EncodedWordBytes = 45;

bool isPrintableAscii(std::string_view text)
{
    return std::ranges::all_of(text, [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 0x20 && byte < 0x7F;
    });
}

/// The text as a header carries it: as it is if it is printable ASCII, else in encoded words of UTF-8
/// (RFC 2047), each a whole number of characters, on folded lines.
std::string headerText(std::string_view text)
{
    if (isPrintableAscii(text)) {
        return std::string(text);
    }
    std::string result;
    while (!text.empty()) {
        std::size_t size = std::min(text.size(), EncodedWordBytes);
        // Never in the middle of a character: its continuation bytes are 10xxxxxx
        while (size < text.size() && size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0U) == 0x80U) {
            --size;
        }
        if (!result.empty()) {
            result += "\r\n ";
        }
        result += "=?UTF-8?B?" + base64(text.substr(0, size)) + "?=";
        text.remove_prefix(size);
    }
    return result;
}

/// `R-Type <rtype@example.org>` as a header carries it: its name quoted if it holds a special character,
/// encoded if it is not ASCII.
std::string mailboxHeader(std::string_view mailbox)
{
    const std::size_t open = mailbox.rfind('<');
    const std::string address = mailboxAddress(mailbox).value_or(std::string(mailbox));
    std::string_view name = open == std::string_view::npos ? std::string_view{} : mailbox.substr(0, open);
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
        name.remove_suffix(1);
    }
    while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
        name.remove_prefix(1);
    }
    if (name.empty()) {
        return address;
    }
    if (!isPrintableAscii(name)) {
        return std::format("{} <{}>", headerText(name), address);
    }
    if (name.find_first_of("()<>[]:;@\\,.\"") == std::string_view::npos) {
        return std::format("{} <{}>", name, address);
    }
    std::string quoted = "\"";
    for (const char c : name) {
        if (c == '"' || c == '\\') {
            quoted += '\\';
        }
        quoted += c;
    }
    return std::format("{}\" <{}>", quoted, address);
}

/// Line ends as SMTP wants them: CRLF.
std::string crlf(std::string_view text)
{
    std::string result;
    result.reserve(text.size() + text.size() / 32);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            result += '\r';
        }
        result += text[i];
    }
    return result;
}

/// A part of the mail: its headers, then its body in base64, in lines of 76 characters.
std::string part(std::string_view boundary, std::string_view type, std::string_view body)
{
    std::string result = std::format("--{}\r\nContent-Type: {}; charset=utf-8\r\n"
                                     "Content-Transfer-Encoding: base64\r\n\r\n",
                                     boundary, type);
    const std::string encoded = base64(crlf(body));
    for (std::size_t i = 0; i < encoded.size(); i += Base64LineLength) {
        result += encoded.substr(i, Base64LineLength) + "\r\n";
    }
    return result;
}

} // namespace

std::string base64(std::string_view bytes)
{
    static constexpr std::string_view Alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::size_t count = std::min<std::size_t>(3, bytes.size() - i);
        std::uint32_t group = 0;
        for (std::size_t j = 0; j < 3; ++j) {
            group <<= 8U;
            if (j < count) {
                group |= static_cast<unsigned char>(bytes[i + j]);
            }
        }
        for (std::size_t j = 0; j < 4; ++j) {
            result += j <= count ? Alphabet[(group >> (18U - 6U * j)) & 0x3FU] : '=';
        }
    }
    return result;
}

std::string formatMail(const Mail &mail, std::string_view from, std::chrono::system_clock::time_point date,
                       std::string_view messageId)
{
    // "=_" never appears in base64, which ends with "=" at most: the boundary is in no part
    std::string boundary = "=_rtype_";
    std::ranges::copy_if(messageId, std::back_inserter(boundary),
                         [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; });

    std::string result = std::format("Date: {:%a, %d %b %Y %H:%M:%S} +0000\r\n"
                                     "From: {}\r\n"
                                     "To: {}\r\n"
                                     "Subject: {}\r\n"
                                     "Message-ID: {}\r\n"
                                     "MIME-Version: 1.0\r\n"
                                     "Auto-Submitted: auto-generated\r\n"
                                     "Content-Type: multipart/alternative; boundary=\"{}\"\r\n"
                                     "\r\n",
                                     std::chrono::floor<std::chrono::seconds>(date), mailboxHeader(from),
                                     mail.to, headerText(mail.content.subject), messageId, boundary);
    result += part(boundary, "text/plain", mail.content.text);
    result += part(boundary, "text/html", mail.content.html);
    result += std::format("--{}--\r\n", boundary);
    return result;
}

} // namespace rtype::server
