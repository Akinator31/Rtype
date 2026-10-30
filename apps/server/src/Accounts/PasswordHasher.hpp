#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace rtype::server
{

/// The cost of a hash: what an attacker pays for each guess, and the master for each sign-in.
struct Argon2Parameters {
    /// KiB of memory.
    std::uint32_t memory = 19 * 1024;
    std::uint32_t passes = 2;
    std::uint32_t lanes = 1;

    friend bool operator==(const Argon2Parameters &, const Argon2Parameters &) = default;
};

/// What OWASP recommends at least for Argon2id: 19 MiB, 2 passes, 1 lane (ADR 0024).
inline constexpr Argon2Parameters DefaultArgon2Parameters{};

/// Hashes a password with Argon2id and a new random salt of 16 bytes, on the calling thread: tens of
/// milliseconds and the memory of the parameters.
/// @return The hash in its standard form, `$argon2id$v=19$m=19456,t=2,p=1$<salt>$<hash>`, which carries its
/// parameters: a hash made with other parameters is still checked with its own.
/// @throws std::runtime_error if Argon2 fails: memory missing, parameters out of its bounds.
[[nodiscard]] std::string hashPassword(std::string_view password,
                                       const Argon2Parameters &parameters = DefaultArgon2Parameters);

/// Checks a password against a hash of hashPassword(), in constant time, on the calling thread.
/// @return False if it does not match, or if the hash is not an Argon2id hash.
[[nodiscard]] bool verifyPassword(std::string_view hash, std::string_view password);

/// Hashes and checks passwords on threads of their own, never on the event loop of the master
/// (docs/architecture/accounts.md#passwords).
///
/// - **A few at once**: each hash takes the memory of its parameters, 19 MiB; two threads by default.
/// - **The results come back through `post`**, given to the constructor: the post() of the event loop of the
///   master.
/// - **The queue is bounded**: a request beyond `maxPending` is refused at once, and the lobby answers
/// `BUSY`.
///   A request holds its place until its result is delivered through `post`.
/// - **Destroying it** drops the requests not started, finishes those running, and never calls their `done`.
///
/// @code
/// PasswordHasher hasher([&](std::function<void()> function) { loop.post(std::move(function)); });
/// if (!hasher.verify(account.passwordHash, password, [](bool matches) { /* on the loop */ })) {
///     // busy
/// }
/// @endcode
class PasswordHasher
{
public:
    /// Hands a function to the thread that waits for the results. It must stay valid until the PasswordHasher
    /// is destroyed.
    using Post = std::function<void(std::function<void()>)>;

    struct Settings {
        std::size_t threads = 2;
        /// Most requests not delivered yet, those running included.
        std::size_t maxPending = 32;
        Argon2Parameters parameters = DefaultArgon2Parameters;
    };

private:
    /// What outlives the hasher: the count of the requests the results of which are still to be delivered.
    struct Shared {
        std::mutex mutex;
        std::size_t pending = 0;
    };

    Post _post;
    Settings _settings;
    std::shared_ptr<Shared> _shared = std::make_shared<Shared>();
    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<std::function<std::function<void()>()>> _jobs;
    bool _stopping = false;
    std::vector<std::thread> _threads;

    /// Queues a job, which gives what delivers its result. False if the queue is full.
    bool enqueue(std::function<std::function<void()>()> job);
    void loop();

public:
    explicit PasswordHasher(Post post, Settings settings);
    explicit PasswordHasher(Post post) : PasswordHasher(std::move(post), Settings{}) {}
    ~PasswordHasher();

    PasswordHasher(const PasswordHasher &) = delete;
    PasswordHasher &operator=(const PasswordHasher &) = delete;
    PasswordHasher(PasswordHasher &&) = delete;
    PasswordHasher &operator=(PasswordHasher &&) = delete;

    /// Hashes a password, then calls `done` through post with its hash, or nothing if Argon2 failed.
    /// @return False if the queue is full: nothing is done, and `done` is never called.
    bool hash(std::string password, std::function<void(std::optional<std::string>)> done);

    /// Checks a password against a hash, then calls `done` through post with whether it matches.
    /// @return False if the queue is full: nothing is done, and `done` is never called.
    bool verify(std::string hash, std::string password, std::function<void(bool)> done);

    /// The requests whose result is not delivered yet.
    [[nodiscard]] std::size_t pending() const;

    /// What its hashes cost.
    [[nodiscard]] const Argon2Parameters &parameters() const { return _settings.parameters; }
};

} // namespace rtype::server
