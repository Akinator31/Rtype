#include "rtype/database/DatabaseThread.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <exception>

namespace rtype::database
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("database");
    return instance;
}

} // namespace

DatabaseThread::DatabaseThread(Database database, Post post, Settings settings)
    : _database(std::move(database)), _post(std::move(post)), _settings(settings)
{
    _thread = std::thread([this] { loop(); });
}

DatabaseThread::~DatabaseThread()
{
    {
        const std::lock_guard lock(_mutex);
        _stopping = true;
    }
    _wake.notify_all();
    _thread.join();
}

std::size_t DatabaseThread::pending()
{
    const std::lock_guard lock(_mutex);
    return _jobs.size() + (_running ? 1 : 0);
}

bool DatabaseThread::enqueue(Job job)
{
    {
        const std::lock_guard lock(_mutex);
        if (_jobs.size() >= _settings.maxPending) {
            logger().warning("Database {}: {} tasks already waiting, \"{}\" is refused",
                             _database.address().toString(), _jobs.size(), job.description);
            return false;
        }
        _jobs.push_back(std::move(job));
    }
    _wake.notify_all();
    return true;
}

void DatabaseThread::loop()
{
    while (true) {
        {
            std::unique_lock lock(_mutex);
            _wake.wait(lock, [this] { return _stopping || !_jobs.empty(); });
            if (_jobs.empty()) {
                return;
            }
        }
        if (_disconnected && !reconnect()) {
            failRemaining();
            return;
        }
        Job job;
        {
            const std::lock_guard lock(_mutex);
            job = std::move(_jobs.front());
            _jobs.pop_front();
            _running = true;
        }
        std::function<void()> deliver = execute(job);
        {
            const std::lock_guard lock(_mutex);
            _running = false;
        }
        // Once it no longer counts as pending: whoever gets the result sees the task finished
        if (deliver) {
            _post(std::move(deliver));
        }
    }
}

std::function<void()> DatabaseThread::execute(Job &job)
{
    std::function<void()> deliver;
    try {
        deliver = job.run(_database);
    } catch (const std::exception &error) {
        logger().error("Database {}: \"{}\" failed: {}", _database.address().toString(), job.description,
                       error.what());
        deliver = job.failed;
        _disconnected = !_database.connected();
    }
    return deliver;
}

bool DatabaseThread::reconnect()
{
    const std::string address = _database.address().toString();
    logger().warning("Database {}: connection lost, connecting again", address);
    std::chrono::milliseconds delay = _settings.firstRetryDelay;
    while (true) {
        try {
            _database.reconnect();
            if (_database.connected()) {
                logger().info("Database {}: connected again", address);
                _disconnected = false;
                return true;
            }
        } catch (const std::exception &error) {
            logger().warning("Database {}: cannot connect: {}. Next attempt in {} ms", address, error.what(),
                             delay.count());
        }
        std::unique_lock lock(_mutex);
        if (_wake.wait_for(lock, delay, [this] { return _stopping; })) {
            return false;
        }
        delay = std::min(delay * 2, _settings.maxRetryDelay);
    }
}

void DatabaseThread::failRemaining()
{
    std::deque<Job> jobs;
    {
        const std::lock_guard lock(_mutex);
        jobs.swap(_jobs);
    }
    if (!jobs.empty()) {
        logger().error("Database {}: stopped without a connection, {} tasks not done",
                       _database.address().toString(), jobs.size());
    }
    for (Job &job : jobs) {
        if (job.failed) {
            _post(std::move(job.failed));
        }
    }
}

} // namespace rtype::database
