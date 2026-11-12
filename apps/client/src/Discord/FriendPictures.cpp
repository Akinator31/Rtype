#include "Discord/FriendPictures.hpp"

#include "rtype/log/Logger.hpp"

// libcurl includes windows.h: without its min and max macros, which break std::min
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <utility>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("discord");
    return instance;
}

/// What a download gathers, and whether the object asked it to stop.
struct Download {
    std::vector<std::byte> bytes;
    bool tooLarge = false;
    const std::atomic<bool> *stopping = nullptr;
};

std::size_t gather(char *data, std::size_t size, std::size_t count, void *user)
{
    auto &download = *static_cast<Download *>(user);
    const std::size_t length = size * count;
    if (download.bytes.size() + length > MaxFriendPictureBytes) {
        download.tooLarge = true;
        return 0;
    }
    const auto *first = reinterpret_cast<const std::byte *>(data);
    download.bytes.insert(download.bytes.end(), first, first + length);
    return length;
}

int progress(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    // A non-zero answer aborts the download: the object is going
    const auto &download = *static_cast<Download *>(user);
    return download.stopping->load() ? 1 : 0;
}

bool isPng(const std::vector<std::byte> &bytes)
{
    constexpr std::array<unsigned char, 8> signature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    return bytes.size() >= signature.size() &&
           std::equal(signature.begin(), signature.end(), bytes.begin(),
                      [](unsigned char expected, std::byte actual) { return std::byte{expected} == actual; });
}

/// The PNG at `url`, none if it fails.
std::optional<std::vector<std::byte>> download(const std::string &url, const std::atomic<bool> &stopping)
{
    struct Deleter {
        void operator()(CURL *curl) const { curl_easy_cleanup(curl); }
    };
    const std::unique_ptr<CURL, Deleter> curl(curl_easy_init());
    if (!curl) {
        return std::nullopt;
    }
    Download result;
    result.stopping = &stopping;
    CURL *handle = curl.get();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "r-type_client (https://github.com/Akinator31/Rtype)");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, gather);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &result);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    const CURLcode code = curl_easy_perform(handle);
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    if (code != CURLE_OK || status != 200 || !isPng(result.bytes)) {
        if (!stopping) {
            logger().info("No picture of a Discord friend: {}",
                          code != CURLE_OK ? curl_easy_strerror(code) : "not a PNG");
        }
        return std::nullopt;
    }
    return std::move(result.bytes);
}

} // namespace

bool isDiscordPicture(std::string_view url)
{
    // Its host, then a path: never another host that starts the same
    return url.starts_with(DiscordCdn) && url.size() > DiscordCdn.size() && url.size() <= 512;
}

FriendPictures::FriendPictures(network::EventLoop &loop) : _loop(loop), _thread([this] { run(); }) {}

FriendPictures::~FriendPictures()
{
    *_alive = false;
    {
        // Under the mutex, so that the thread cannot miss the wake
        const std::scoped_lock lock(_queue->mutex);
        _queue->stopping = true;
    }
    _queue->wake.notify_all();
    _thread.join();
}

void FriendPictures::request(const std::string &url)
{
    if (!isDiscordPicture(url) || !_asked.insert(url).second) {
        return;
    }
    {
        const std::scoped_lock lock(_queue->mutex);
        _queue->waiting.push_back(url);
    }
    _queue->wake.notify_one();
}

const std::vector<std::byte> *FriendPictures::picture(const std::string &url) const
{
    const auto found = _pictures.find(url);
    return found == _pictures.end() ? nullptr : &found->second;
}

void FriendPictures::run()
{
    static const bool initialized = [] { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK; }();
    if (!initialized) {
        return;
    }
    const std::shared_ptr<Queue> queue = _queue;
    for (;;) {
        std::string url;
        {
            std::unique_lock lock(queue->mutex);
            queue->wake.wait(lock, [&queue] { return queue->stopping || !queue->waiting.empty(); });
            if (queue->stopping) {
                return;
            }
            url = std::move(queue->waiting.front());
            queue->waiting.pop_front();
        }
        std::optional<std::vector<std::byte>> png = download(url, queue->stopping);
        if (!png) {
            continue;
        }
        // Handed to the game loop, which owns the pictures
        _loop.post([this, alive = std::weak_ptr<bool>(_alive), url = std::move(url),
                    png = std::move(*png)]() mutable {
            const std::shared_ptr<bool> living = alive.lock();
            if (!living || !*living) {
                return;
            }
            _pictures.insert_or_assign(std::move(url), std::move(png));
            if (_ready) {
                _ready();
            }
        });
    }
}

} // namespace rtype::client
