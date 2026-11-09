#pragma once

// The HTTP client of the tests of the web server of the master, over libcurl, and the settings file that
// gives the panel of a master a free port.

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace testing
{

struct HttpResponse {
    /// 0 if no response came: `body` then holds the error of libcurl.
    long status = 0;
    /// By name in lower case; a header given twice keeps its last value.
    std::map<std::string, std::string> headers;
    /// Every Set-Cookie, in the order they came.
    std::vector<std::string> cookies;
    std::string body;

    [[nodiscard]] std::string header(const std::string &name) const
    {
        const auto found = headers.find(name);
        return found == headers.end() ? std::string{} : found->second;
    }

    /// The Set-Cookie of the cookie `name`, empty without it.
    [[nodiscard]] std::string cookie(const std::string &name) const
    {
        for (const std::string &line : cookies) {
            if (line.starts_with(name + "=")) {
                return line;
            }
        }
        return {};
    }
};

struct HttpRequest {
    std::string method = "GET";
    /// Whole lines, `Cookie: rtype_session=…`.
    std::vector<std::string> headers;
};

/// Sends one request on `curl` and waits for its response, with its body if not empty. Never follows a
/// redirection, and sends the path as written, `..` included.
inline HttpResponse perform(CURL *curl, const std::string &url, const HttpRequest &request,
                            const std::string &body)
{
    HttpResponse response;
    curl_slist *headers = nullptr;
    for (const std::string &line : request.headers) {
        headers = curl_slist_append(headers, line.c_str());
    }
    const auto writeBody = +[](char *data, std::size_t size, std::size_t count, void *user) -> std::size_t {
        static_cast<std::string *>(user)->append(data, size * count);
        return size * count;
    };
    const auto writeHeader = +[](char *data, std::size_t size, std::size_t count, void *user) -> std::size_t {
        const std::string line(data, size * count);
        const std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
            std::string value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ')) {
                value.erase(value.begin());
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
                value.pop_back();
            }
            auto &response = *static_cast<HttpResponse *>(user);
            if (name == "set-cookie") {
                response.cookies.push_back(value);
            }
            response.headers[name] = value;
        }
        return size * count;
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, request.method == "HEAD" ? 1L : 0L);
    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_PATH_AS_IS, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    const CURLcode result = curl_easy_perform(curl);
    if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    } else {
        response.body = curl_easy_strerror(result);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    curl_slist_free_all(headers);
    return response;
}

/// Sends one request on a connection of its own and waits for its response.
inline HttpResponse http(const std::string &url, const HttpRequest &request = {},
                         const std::string &body = {})
{
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        return {.status = 0, .headers = {}, .cookies = {}, .body = "curl_easy_init failed"};
    }
    HttpResponse response = perform(curl, url, request, body);
    curl_easy_cleanup(curl);
    return response;
}

/// GETs the addresses one after the other on one connection, kept alive between them, as a browser does.
/// `reused` tells whether they all went through the first connection.
inline std::vector<HttpResponse> httpSequence(const std::vector<std::string> &urls, bool *reused = nullptr)
{
    std::vector<HttpResponse> responses;
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        return responses;
    }
    long connections = 0;
    for (const std::string &url : urls) {
        responses.push_back(perform(curl, url, {}, {}));
        long opened = 0;
        curl_easy_getinfo(curl, CURLINFO_NUM_CONNECTS, &opened);
        connections += opened;
    }
    curl_easy_cleanup(curl);
    if (reused != nullptr) {
        *reused = connections == 1;
    }
    return responses;
}

/// Writes a settings file that gives the games and the panel of a master free ports, and returns its path:
/// the masters of the tests run side by side, and must not all take UDP 4242 and TCP 8080.
inline std::filesystem::path onAnyPorts(const std::filesystem::path &file)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "[server]\ngame_port = 0\n[web]\nlisten = \"127.0.0.1:0\"\n";
    return file;
}

} // namespace testing
