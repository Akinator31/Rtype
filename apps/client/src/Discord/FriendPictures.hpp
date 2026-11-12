#pragma once

#include "rtype/network/EventLoop.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

/// The pictures of the Discord friends of the player (docs/architecture/client-online.md#friends), downloaded
/// from the CDN of Discord: the SDK gives their address only.
namespace rtype::client
{

/// Where the pictures may come from: the CDN of Discord, nothing else.
inline constexpr std::string_view DiscordCdn = "https://cdn.discordapp.com/";
/// Most bytes of a picture kept: Discord sends a few kilobytes at 64 pixels.
inline constexpr std::size_t MaxFriendPictureBytes = 512 * 1024;

/// Whether `url` is a picture of the CDN of Discord, which FriendPictures downloads.
[[nodiscard]] bool isDiscordPicture(std::string_view url);

/// Downloads the pictures of the friends, once each, with libcurl on a thread of its own, so that the game
/// loop never waits: each picture comes back through the post() of the loop, which the game loop polls, to
/// the handler of onReady(). A picture that fails, or is not a PNG, is not asked again. The thread stops with
/// the object, a download running aborted.
///
/// @code
/// pictures.onReady([&] { showFriendsAgain(); });
/// if (const auto *png = pictures.picture(url)) { /* a texture of it */ } else { pictures.request(url); }
/// @endcode
class FriendPictures
{
public:
    /// @param loop Polled by the game loop: it outlives the pictures.
    explicit FriendPictures(network::EventLoop &loop);
    ~FriendPictures();

    FriendPictures(const FriendPictures &) = delete;
    FriendPictures &operator=(const FriendPictures &) = delete;

    /// Downloads a picture, if it is one of the CDN of Discord, not asked already.
    void request(const std::string &url);
    /// The PNG of a picture downloaded, none until it came.
    [[nodiscard]] const std::vector<std::byte> *picture(const std::string &url) const;
    /// Called on the game loop each time a picture came.
    void onReady(std::function<void()> ready) { _ready = std::move(ready); }

private:
    /// What the thread and the game loop share.
    struct Queue {
        std::mutex mutex;
        std::condition_variable wake;
        std::deque<std::string> waiting;
        /// Read by the download running, without the mutex: it aborts at once.
        std::atomic<bool> stopping = false;
    };

    network::EventLoop &_loop;
    std::map<std::string, std::vector<std::byte>> _pictures;
    /// Every picture asked, downloaded or not.
    std::set<std::string> _asked;
    std::function<void()> _ready;
    std::shared_ptr<Queue> _queue = std::make_shared<Queue>();
    /// Whether the object is alive, for the results the loop runs after it is gone.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);
    std::thread _thread;

    void run();
};

} // namespace rtype::client
