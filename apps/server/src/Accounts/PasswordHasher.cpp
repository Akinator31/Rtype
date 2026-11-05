#include "Accounts/PasswordHasher.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/network/Crypto.hpp"

#include <argon2.h>

#include <array>
#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::server
{

namespace
{

constexpr std::size_t SaltSize = 16;
constexpr std::size_t HashSize = 32;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("passwords");
    return instance;
}

} // namespace

std::string hashPassword(std::string_view password, const Argon2Parameters &parameters)
{
    const std::array<std::byte, SaltSize> salt = rtype::network::randomBytes<SaltSize>();
    std::string encoded(argon2_encodedlen(parameters.passes, parameters.memory, parameters.lanes, SaltSize,
                                          HashSize, Argon2_id),
                        '\0');
    const int result = argon2id_hash_encoded(parameters.passes, parameters.memory, parameters.lanes,
                                             password.data(), password.size(), salt.data(), salt.size(),
                                             HashSize, encoded.data(), encoded.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error(std::format("cannot hash a password: {}", argon2_error_message(result)));
    }
    // Written as a C string, shorter than the room argon2_encodedlen() counts
    encoded.resize(encoded.find('\0'));
    return encoded;
}

bool verifyPassword(std::string_view hash, std::string_view password)
{
    // Argon2 reads the hash as a C string
    const std::string encoded(hash);
    if (!encoded.starts_with("$argon2id$")) {
        return false;
    }
    return argon2id_verify(encoded.c_str(), password.data(), password.size()) == ARGON2_OK;
}

PasswordHasher::PasswordHasher(Post post, Settings settings) : _post(std::move(post)), _settings(settings)
{
    for (std::size_t i = 0; i < _settings.threads; ++i) {
        _threads.emplace_back([this] { loop(); });
    }
}

PasswordHasher::~PasswordHasher()
{
    {
        const std::lock_guard lock(_mutex);
        _stopping = true;
        _jobs.clear();
    }
    _wake.notify_all();
    for (std::thread &thread : _threads) {
        thread.join();
    }
}

bool PasswordHasher::enqueue(std::function<std::function<void()>()> job)
{
    {
        const std::lock_guard lock(_shared->mutex);
        if (_shared->pending >= _settings.maxPending) {
            logger().warning("{} passwords waiting to be hashed: one more refused", _shared->pending);
            return false;
        }
        ++_shared->pending;
    }
    {
        const std::lock_guard lock(_mutex);
        _jobs.push_back(std::move(job));
    }
    _wake.notify_one();
    return true;
}

void PasswordHasher::loop()
{
    while (true) {
        std::function<std::function<void()>()> job;
        {
            std::unique_lock lock(_mutex);
            _wake.wait(lock, [this] { return _stopping || !_jobs.empty(); });
            if (_stopping) {
                return;
            }
            job = std::move(_jobs.front());
            _jobs.pop_front();
        }
        std::function<void()> deliver = job();
        // The place is freed when the result is delivered: the queue bounds what the loop has to handle too
        _post([shared = _shared, deliver = std::move(deliver)] {
            {
                const std::lock_guard lock(shared->mutex);
                --shared->pending;
            }
            deliver();
        });
    }
}

bool PasswordHasher::hash(std::string password, std::function<void(std::optional<std::string>)> done)
{
    return enqueue([password = std::move(password), done = std::move(done),
                    parameters = _settings.parameters]() mutable -> std::function<void()> {
        std::optional<std::string> hash;
        try {
            hash = hashPassword(password, parameters);
        } catch (const std::exception &error) {
            logger().error("{}", error.what());
        }
        return [done = std::move(done), hash = std::move(hash)]() mutable { done(std::move(hash)); };
    });
}

bool PasswordHasher::verify(std::string hash, std::string password, std::function<void(bool)> done)
{
    return enqueue([hash = std::move(hash), password = std::move(password),
                    done = std::move(done)]() mutable -> std::function<void()> {
        const bool matches = verifyPassword(hash, password);
        return [done = std::move(done), matches] { done(matches); };
    });
}

std::size_t PasswordHasher::pending() const
{
    const std::lock_guard lock(_shared->mutex);
    return _shared->pending;
}

} // namespace rtype::server
