// Tests of the logging library (libs/log). Messages are captured by a sink instead of being printed.

#include "rtype/log/Log.hpp"

#include "rtype/log/Logger.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

using namespace rtype::log;

namespace
{

struct CapturedRecord {
    Level level;
    std::string logger;
    std::string message;
};

class CaptureSink : public Sink
{
public:
    std::vector<CapturedRecord> records;

    void write(const Record &record) override
    {
        records.push_back({record.level, std::string(record.logger), std::string(record.message)});
    }
};

// Captures the messages during a test and restores the global settings afterwards
class Capture
{
private:
    Level _previousLevel = level();

public:
    std::shared_ptr<CaptureSink> sink = std::make_shared<CaptureSink>();

    explicit Capture(Level minimum)
    {
        setLevel(minimum);
        setSink(sink);
    }
    ~Capture()
    {
        setSink(nullptr);
        setLevel(_previousLevel);
    }

    Capture(const Capture &) = delete;
    Capture &operator=(const Capture &) = delete;
};

Record recordWith(Level recordLevel, std::string_view logger, std::string_view message)
{
    return {recordLevel, logger, message, std::chrono::system_clock::now()};
}

} // namespace

TEST_CASE("Messages are formatted and tagged with the logger name", "[log]")
{
    const Capture capture(Level::Trace);
    const Logger logger("network");

    logger.info("Client {} connected from {}:{}", 2, "127.0.0.1", 4242);

    REQUIRE(capture.sink->records.size() == 1);
    REQUIRE(capture.sink->records[0].level == Level::Info);
    REQUIRE(capture.sink->records[0].logger == "network");
    REQUIRE(capture.sink->records[0].message == "Client 2 connected from 127.0.0.1:4242");
}

TEST_CASE("Messages below the minimum level are ignored", "[log]")
{
    const Capture capture(Level::Warning);
    const Logger logger("test");

    logger.trace("trace");
    logger.debug("debug");
    logger.info("info");
    logger.warning("warning");
    logger.error("error");

    REQUIRE(capture.sink->records.size() == 2);
    REQUIRE(capture.sink->records[0].level == Level::Warning);
    REQUIRE(capture.sink->records[1].level == Level::Error);
    REQUIRE_FALSE(logger.isEnabled(Level::Info));
    REQUIRE(logger.isEnabled(Level::Error));
}

TEST_CASE("Level names are parsed whatever their case", "[log]")
{
    REQUIRE(parseLevel("trace") == Level::Trace);
    REQUIRE(parseLevel("Debug") == Level::Debug);
    REQUIRE(parseLevel("INFO") == Level::Info);
    REQUIRE(parseLevel("warning") == Level::Warning);
    REQUIRE(parseLevel("warn") == Level::Warning);
    REQUIRE(parseLevel("error") == Level::Error);
    REQUIRE(parseLevel("verbose") == std::nullopt);
    REQUIRE(parseLevel("") == std::nullopt);
}

TEST_CASE("Level names all have the same width", "[log]")
{
    for (const Level each : {Level::Trace, Level::Debug, Level::Info, Level::Warning, Level::Error}) {
        REQUIRE(levelName(each).size() == 5);
    }
}

TEST_CASE("A formatted line shows the time, the level, the logger and the message", "[log]")
{
    const std::string line = formatRecord(recordWith(Level::Warning, "render", "Texture missing"), false);

    REQUIRE(std::regex_match(line, std::regex(R"(\d\d:\d\d:\d\d\.\d{3} WARN  render   Texture missing)")));
}

TEST_CASE("Colours are only added when asked", "[log]")
{
    const Record record = recordWith(Level::Error, "server", "Crash");

    REQUIRE(formatRecord(record, false).find('\x1b') == std::string::npos);
    REQUIRE(formatRecord(record, true).find('\x1b') != std::string::npos);
}

TEST_CASE("The lines of a multi-line message are aligned under the first one", "[log]")
{
    const std::string line = formatRecord(recordWith(Level::Info, "client", "first\nsecond"), false);
    const std::size_t messageColumn = line.find("first");
    const std::size_t secondLine = line.find('\n') + 1;

    REQUIRE(line.substr(secondLine) == std::string(messageColumn, ' ') + "second");
}

TEST_CASE("A long logger name pushes the message instead of being cut", "[log]")
{
    const std::string line = formatRecord(recordWith(Level::Info, "averylongname", "message"), false);

    REQUIRE(line.find("averylongname message") != std::string::npos);
}
