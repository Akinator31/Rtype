#pragma once

#include "rtype/database/Database.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace rtype::database
{

/// A Database on a thread of its own, so that a slow query never blocks the thread that asks for it
/// (docs/architecture/database.md#the-database-thread).
///
/// - **Tasks run in the order they are given**, one at a time, on the database thread.
/// - **Their result comes back through `post`**, given to the constructor: the server gives the post() of its
///   event loop, so that the result is handled on the loop, like a message received.
/// - **A task that throws is logged**, and its result is empty; the next tasks run.
/// - **A lost connection is opened again** before the next task, with a growing delay between the attempts.
///   The task during which it was lost fails: it is not run twice, since it may have been done.
/// - **The queue is bounded**: a task given while it is full is refused.
/// - **Destroying it runs the tasks already given**, then stops the thread; those that cannot run, the
///   database being unreachable, fail.
///
/// @code
/// rtype::database::DatabaseThread thread(std::move(database), [&](auto function) { loop.post(function); });
/// thread.run("read the best score",
///            [](rtype::database::Database &database) { return readBestScore(database); },
///            [](std::optional<long long> best) { /* on the loop; empty if it failed */ });
/// @endcode
class DatabaseThread
{
public:
    /// Hands a function to the thread that waits for the results. It must stay valid until the DatabaseThread
    /// is destroyed.
    using Post = std::function<void(std::function<void()>)>;

    struct Settings {
        /// Most tasks waiting at once.
        std::size_t maxPending = 1024;
        /// Delay before the second attempt to connect again, doubled at each attempt up to maxRetryDelay.
        std::chrono::milliseconds firstRetryDelay{500};
        std::chrono::milliseconds maxRetryDelay{30'000};
    };

private:
    struct Job {
        std::string description;
        /// Runs the task, and gives what delivers its result; throws if the task fails.
        std::function<std::function<void()>(Database &)> run;
        /// Delivers a failure; empty if nobody waits for the result.
        std::function<void()> failed;
    };

    Database _database;
    Post _post;
    Settings _settings;
    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Job> _jobs;
    bool _running = false;
    bool _stopping = false;
    /// A task failed with the connection lost: it is opened again before the next one. Database thread only.
    bool _disconnected = false;
    std::thread _thread;

    bool enqueue(Job job);
    void loop();
    /// Connects again until it works, or the thread is stopping. True if connected.
    bool reconnect();
    /// Runs the task: what delivers its result, or its failure.
    std::function<void()> execute(Job &job);
    /// Fails the tasks still waiting: the thread stops without a connection.
    void failRemaining();

public:
    DatabaseThread(Database database, Post post, Settings settings);
    DatabaseThread(Database database, Post post)
        : DatabaseThread(std::move(database), std::move(post), Settings{})
    {
    }
    ~DatabaseThread();

    DatabaseThread(const DatabaseThread &) = delete;
    DatabaseThread &operator=(const DatabaseThread &) = delete;
    DatabaseThread(DatabaseThread &&) = delete;
    DatabaseThread &operator=(DatabaseThread &&) = delete;

    /// Runs `task(database)` on the database thread, then `done` through post, with what the task returned:
    /// `std::optional<Result>`, empty if it failed, or `bool` for a task that returns nothing.
    /// @param description What the task does, for the logs: "record game 4".
    /// @return False if the queue is full: the task is not run, and `done` is never called.
    template <typename Task, typename Done>
    bool run(std::string description, Task task, Done done)
    {
        using Result = std::invoke_result_t<Task &, Database &>;
        auto shared = std::make_shared<Done>(std::move(done));
        Job job{std::move(description),
                [task = std::move(task), shared](Database &database) mutable -> std::function<void()> {
                    if constexpr (std::is_void_v<Result>) {
                        task(database);
                        return [shared] { (*shared)(true); };
                    } else {
                        auto result = std::make_shared<std::optional<Result>>(task(database));
                        return [shared, result] { (*shared)(std::move(*result)); };
                    }
                },
                [shared] {
                    if constexpr (std::is_void_v<Result>) {
                        (*shared)(false);
                    } else {
                        (*shared)(std::optional<Result>());
                    }
                }};
        return enqueue(std::move(job));
    }

    /// Runs `task(database)` on the database thread, without waiting for its result: a failure is logged.
    /// @return False if the queue is full: the task is not run.
    template <typename Task>
    bool run(std::string description, Task task)
    {
        return enqueue(Job{std::move(description),
                           [task = std::move(task)](Database &database) mutable -> std::function<void()> {
                               task(database);
                               return {};
                           },
                           {}});
    }

    /// The tasks given and not finished yet, the one running included.
    [[nodiscard]] std::size_t pending();
};

} // namespace rtype::database
