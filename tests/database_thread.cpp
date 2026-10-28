// Tests of the database thread: tasks in order, results through post, failures, a bounded queue, the tasks
// left at destruction, and a lost connection opened again
// (docs/architecture/database.md#the-database-thread).

#include "TestDatabase.hpp"
#include "rtype/database/Database.hpp"
#include "rtype/database/DatabaseThread.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using rtype::database::Address;
using rtype::database::Database;
using rtype::database::DatabaseThread;
using rtype::database::Engine;
using rtype::database::Migration;
using rtype::test::TestDatabase;

namespace
{

const std::vector<Migration> Migrations{
    {1, "create the scores table", {"CREATE TABLE scores (id BIGINT PRIMARY KEY, score BIGINT NOT NULL)"}}};

Database openScores(const TestDatabase &test)
{
    Database database = test.open();
    database.migrate(Migrations);
    return database;
}

/// A SQLite database in a directory of its own, which a test can move away to make it unreachable.
struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() /
               std::string("rtype_tests_database_thread_").append(name))
    {
        std::filesystem::remove_all(path);
        std::filesystem::remove_all(moved());
    }
    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
        std::filesystem::remove_all(moved(), ignored);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;

    /// Where the directory goes when a test makes the database unreachable.
    [[nodiscard]] std::filesystem::path moved() const { return std::filesystem::path(path).concat("_moved"); }

    [[nodiscard]] Database open() const
    {
        Database database = Database::open(Address::parse("sqlite:" + (path / "rtype.db").string()));
        database.migrate(Migrations);
        return database;
    }
};

/// What the event loop of the server is to the thread: a queue of functions, run by the test.
class Mailbox
{
    std::mutex _mutex;
    std::condition_variable _posted;
    std::deque<std::function<void()>> _functions;

public:
    DatabaseThread::Post post()
    {
        return [this](std::function<void()> function) {
            {
                const std::lock_guard lock(_mutex);
                _functions.push_back(std::move(function));
            }
            _posted.notify_all();
        };
    }

    /// Runs the functions posted, on this thread, until the condition holds or 5 seconds pass.
    bool runUntil(const std::function<bool()> &condition)
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!condition()) {
            std::unique_lock lock(_mutex);
            if (!_posted.wait_until(lock, deadline, [this] { return !_functions.empty(); })) {
                return false;
            }
            std::function<void()> function = std::move(_functions.front());
            _functions.pop_front();
            lock.unlock();
            function();
        }
        return true;
    }
};

long long countScores(Database &database)
{
    long long count = 0;
    database.session() << "SELECT COUNT(*) FROM scores", soci::into(count);
    return count;
}

void insertScore(Database &database, long long id)
{
    database.session() << "INSERT INTO scores (id, score) VALUES (:id, 100)", soci::use(id);
}

/// Closes the connection, as a database server that went away, and fails like the query that found it.
void loseConnection(Database &database)
{
    database.session().close();
    throw std::runtime_error("server closed the connection unexpectedly");
}

} // namespace

TEST_CASE("Tasks run in order on the database thread, their results come back through post", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_thread_order");
    Mailbox mailbox;
    DatabaseThread thread(openScores(test), mailbox.post());
    const std::thread::id testThread = std::this_thread::get_id();
    std::vector<std::string> results;
    std::atomic<bool> onDatabaseThread = true;

    for (long long id = 1; id <= 3; ++id) {
        REQUIRE(thread.run(
            "insert a score",
            [&, id](Database &database) {
                onDatabaseThread = onDatabaseThread && std::this_thread::get_id() != testThread;
                insertScore(database, id);
            },
            [&, id](bool done) {
                CHECK(std::this_thread::get_id() == testThread);
                results.push_back(std::format("insert {} {}", id, done));
            }));
    }
    REQUIRE(thread.run("count the scores", countScores, [&](std::optional<long long> count) {
        results.push_back(std::format("count {}", count.value_or(-1)));
    }));

    REQUIRE(mailbox.runUntil([&] { return results.size() == 4; }));
    CHECK(results == std::vector<std::string>{"insert 1 true", "insert 2 true", "insert 3 true", "count 3"});
    CHECK(onDatabaseThread);
    CHECK(thread.pending() == 0);
}

TEST_CASE("A task that fails gives an empty result, and the next ones run", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_thread_failure");
    Mailbox mailbox;
    DatabaseThread thread(openScores(test), mailbox.post());
    std::optional<std::optional<long long>> missingTable;
    std::optional<bool> duplicate;
    std::optional<std::optional<long long>> count;

    thread.run(
        "read a missing table",
        [](Database &database) {
            long long value = 0;
            database.session() << "SELECT COUNT(*) FROM missing", soci::into(value);
            return value;
        },
        [&](std::optional<long long> value) { missingTable = value; });
    thread.run("insert a score", [](Database &database) { insertScore(database, 1); });
    thread.run(
        "insert the same score", [](Database &database) { insertScore(database, 1); },
        [&](bool done) { duplicate = done; });
    thread.run("count the scores", countScores, [&](std::optional<long long> value) { count = value; });

    REQUIRE(mailbox.runUntil([&] { return count.has_value(); }));
    REQUIRE(missingTable.has_value());
    CHECK_FALSE(missingTable->has_value());
    CHECK(duplicate == false);
    CHECK(count == 1);
}

TEST_CASE("A task given while the queue is full is refused", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_thread_full");
    Mailbox mailbox;
    DatabaseThread thread(openScores(test), mailbox.post(), {.maxPending = 1});
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<bool> started = false;
    int done = 0;

    // The first one runs and blocks the thread, the second one waits: the queue is full
    REQUIRE(thread.run("block", [&](Database &) {
        started = true;
        released.wait();
    }));
    while (!started) {
        std::this_thread::yield();
    }
    REQUIRE(thread.run("wait", [](Database &) {}, [&](bool) { ++done; }));
    CHECK(thread.pending() == 2);
    CHECK_FALSE(thread.run("refused", [](Database &) {}, [&](bool) { ++done; }));

    release.set_value();
    REQUIRE(mailbox.runUntil([&] { return done == 1; }));
    CHECK(thread.pending() == 0);
    CHECK(thread.run("accepted again", [](Database &) {}, [&](bool) { ++done; }));
    REQUIRE(mailbox.runUntil([&] { return done == 2; }));
}

TEST_CASE("Destroying the thread runs the tasks already given", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_thread_destroyed");
    Mailbox mailbox;
    {
        DatabaseThread thread(openScores(test), mailbox.post());
        for (long long id = 1; id <= 50; ++id) {
            thread.run("insert a score", [id](Database &database) { insertScore(database, id); });
        }
    }

    Database database = openScores(test);
    CHECK(countScores(database) == 50);
}

TEST_CASE("A lost connection is opened again before the next task", "[database]")
{
    const Engine engine = GENERATE(from_range(rtype::test::testEngines()));
    CAPTURE(rtype::test::engineName(engine));
    const TestDatabase test(engine, "database_thread_reconnect");
    Mailbox mailbox;
    DatabaseThread thread(openScores(test), mailbox.post(), {.firstRetryDelay = 10ms, .maxRetryDelay = 20ms});
    std::optional<bool> lost;
    std::optional<std::optional<long long>> count;

    thread.run("insert a score", [](Database &database) { insertScore(database, 1); });
    thread.run("lose the connection", loseConnection, [&](bool done) { lost = done; });
    thread.run("count the scores", countScores, [&](std::optional<long long> value) { count = value; });

    REQUIRE(mailbox.runUntil([&] { return count.has_value(); }));
    // The task that lost it fails, not run again; the next one runs on the same database
    CHECK(lost == false);
    CHECK(count == 1);
}

TEST_CASE("A connection the PostgreSQL server ends is opened again", "[database]")
{
    if (!rtype::database::isBuiltWith(Engine::Postgresql) || !rtype::test::testPostgresqlUrl()) {
        SKIP("No PostgreSQL server for the tests: set RTYPE_TEST_POSTGRESQL_URL");
    }
    const TestDatabase test(Engine::Postgresql, "database_thread_terminated");
    Mailbox mailbox;
    DatabaseThread thread(openScores(test), mailbox.post(), {.firstRetryDelay = 10ms, .maxRetryDelay = 20ms});
    std::optional<bool> ended;
    std::optional<std::optional<long long>> count;

    thread.run("insert a score", [](Database &database) { insertScore(database, 1); });
    // As an administrator, or a restart of the server, would
    thread.run(
        "end the connection",
        [](Database &database) { database.session() << "SELECT pg_terminate_backend(pg_backend_pid())"; },
        [&](bool done) { ended = done; });
    thread.run("count the scores", countScores, [&](std::optional<long long> value) { count = value; });

    REQUIRE(mailbox.runUntil([&] { return count.has_value(); }));
    CHECK(ended == false);
    CHECK(count == 1);
}

TEST_CASE("The next task waits until the database can be reached again", "[database]")
{
    const TempDirectory directory("unreachable");
    Mailbox mailbox;
    DatabaseThread thread(directory.open(), mailbox.post(), {.firstRetryDelay = 10ms, .maxRetryDelay = 20ms});
    std::optional<std::optional<long long>> count;

    thread.run("insert a score", [](Database &database) { insertScore(database, 1); });
    thread.run("lose the database", [&](Database &database) {
        // Its directory gone, SQLite can no longer open it
        database.session().close();
        std::filesystem::rename(directory.path, directory.moved());
        throw std::runtime_error("server closed the connection unexpectedly");
    });
    thread.run("count the scores", countScores, [&](std::optional<long long> value) { count = value; });

    while (std::filesystem::exists(directory.path)) {
        std::this_thread::yield();
    }
    // Several attempts fail meanwhile, and the task waits
    std::this_thread::sleep_for(100ms);
    CHECK(thread.pending() == 1);

    std::filesystem::rename(directory.moved(), directory.path);
    REQUIRE(mailbox.runUntil([&] { return count.has_value(); }));
    CHECK(count == 1);
}

TEST_CASE("Destroyed while it cannot connect, the thread stops at once and fails the tasks left",
          "[database]")
{
    const TempDirectory directory("stopped_unreachable");
    Mailbox mailbox;
    std::optional<bool> left;
    std::optional<DatabaseThread> thread;
    thread.emplace(directory.open(), mailbox.post(),
                   DatabaseThread::Settings{.firstRetryDelay = 10s, .maxRetryDelay = 10s});
    thread->run("lose the database", [&](Database &database) {
        database.session().close();
        std::filesystem::rename(directory.path, directory.moved());
        throw std::runtime_error("server closed the connection unexpectedly");
    });
    thread->run(
        "insert a score", [](Database &database) { insertScore(database, 1); },
        [&](bool done) { left = done; });
    while (std::filesystem::exists(directory.path)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(50ms);

    // Destroyed during the 10 seconds between two attempts
    const auto start = std::chrono::steady_clock::now();
    thread.reset();
    CHECK(std::chrono::steady_clock::now() - start < 2s);
    REQUIRE(mailbox.runUntil([&] { return left.has_value(); }));
    CHECK(left == false);
}
