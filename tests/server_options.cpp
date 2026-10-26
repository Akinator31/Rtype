// Tests of the command line of r-type_server and of the secret of its instances
// (docs/architecture/server.md).

#include "Common/GameConfig.hpp"
#include "Common/Options.hpp"
#include "Common/Secret.hpp"
#include "Server.hpp"
#include "rtype/network/Tls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace rtype::server;
using rtype::network::Address;

namespace
{

Options parse(std::vector<std::string_view> arguments)
{
    return parseOptions(arguments);
}

} // namespace

// --- Master ---

TEST_CASE("Without arguments, the server is a master with the default settings", "[server]")
{
    const Options options = parse({});

    CHECK(options.mode == Options::Mode::Master);
    CHECK(options.master.listen == Address::anyIpv4(4242));
    CHECK(options.master.firstGamePort == 4243);
    CHECK(options.master.lastGamePort == 4299);
    CHECK(options.master.key == "server.key");
    CHECK_FALSE(options.master.check);
    CHECK(options.master.config.empty());
    CHECK(options.instance.config.empty());
    CHECK_FALSE(options.logLevel);
}

TEST_CASE("The option --config is read by the master and by an instance", "[server]")
{
    CHECK(parse({"--config", "/srv/rtype/config"}).master.config == "/srv/rtype/config");
    const Options instance =
        parse({"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--config", "conf"});
    CHECK(instance.instance.config == "conf");
    CHECK_THROWS_AS(parse({"--config"}), std::invalid_argument);
}

TEST_CASE("The configuration of the game is next to the executable, unless --config says otherwise",
          "[server]")
{
    CHECK(getConfigDirectory({}).filename() == "config");
    CHECK(getConfigDirectory("elsewhere") == "elsewhere");
}

TEST_CASE("A server reads a valid configuration, and refuses one with errors", "[server]")
{
    const rtype::log::Logger logger("test");
    const std::filesystem::path broken = std::filesystem::temp_directory_path() / "rtype_tests_server_config";
    std::filesystem::remove_all(broken);
    std::filesystem::create_directories(broken / "ships");
    std::ofstream(broken / "ships" / "broken.lua") << "return { id = 'broken' }";

    const auto valid = readGameConfig(RTYPE_TEST_CONFIG, logger);
    REQUIRE(valid);
    CHECK_FALSE(valid->enemies.empty());
    CHECK_FALSE(readGameConfig(broken, logger));
    std::filesystem::remove_all(broken);
}

TEST_CASE("A master with a configuration that has errors does not start", "[server]")
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "rtype_tests_master_config";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory / "enemies");
    std::ofstream(directory / "enemies" / "broken.lua") << "return 3";
    const std::filesystem::path key = directory / "server.key";
    std::ofstream(key) << rtype::network::ServerIdentity::generate().privateKeyPem();
    const std::string keyArgument = key.string();
    const std::string configArgument = directory.string();
    std::ostringstream out;
    std::istringstream in;

    CHECK(run(std::vector<std::string_view>{"--check", "--port", "0", "--key", keyArgument}, out, in) ==
          Success);
    CHECK(run(std::vector<std::string_view>{"--check", "--port", "0", "--key", keyArgument, "--config",
                                            configArgument},
              out, in) == Failure);
    std::filesystem::remove_all(directory);
}

TEST_CASE("Every option of the master is read", "[server]")
{
    const Options options = parse({"--port", "5000", "--bind", "127.0.0.1", "--game-ports", "6000-6009",
                                   "--key", "/etc/rtype/server.key", "--check", "--log-level", "debug"});

    CHECK(options.mode == Options::Mode::Master);
    CHECK(options.master.listen == Address::loopbackIpv4(5000));
    CHECK(options.master.firstGamePort == 6000);
    CHECK(options.master.lastGamePort == 6009);
    CHECK(options.master.key == "/etc/rtype/server.key");
    CHECK(options.master.check);
    CHECK(options.logLevel == rtype::log::Level::Debug);
    CHECK(parse({"-p", "0", "-k", "a.key"}).master.listen == Address::anyIpv4(0));
}

TEST_CASE("Invalid options of the master are refused", "[server]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--port"},
        {"--port", "65536"},
        {"--port", "-1"},
        {"--port", "42a"},
        {"--bind", "localhost"},
        {"--game-ports", "4243"},
        {"--game-ports", "4300-4243"},
        {"--game-ports", "0-10"},
        {"--log-level", "loud"},
        {"--unknown"},
        {"4242"},
        {"--id", "3"},
        {"--standalone"},
    };

    for (const auto &arguments : invalid) {
        CAPTURE(arguments);
        CHECK_THROWS_AS(parse(arguments), std::invalid_argument);
    }
}

// --- Instance ---

TEST_CASE("An instance reads its id and ports", "[server]")
{
    const Options options = parse({"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000"});

    CHECK(options.mode == Options::Mode::Instance);
    CHECK(options.instance.id == 7);
    CHECK(options.instance.udpPort == 4251);
    CHECK(options.instance.masterPort == 5000);
    CHECK_FALSE(options.instance.standalone);
}

TEST_CASE("An instance needs its id and ports, and refuses the options of the master", "[server]")
{
    const std::vector<std::vector<std::string_view>> invalid{
        {"--instance"},
        {"--instance", "--udp-port", "4251", "--master-port", "5000"},
        {"--instance", "--id", "7", "--master-port", "5000"},
        {"--instance", "--id", "7", "--udp-port", "4251"},
        {"--instance", "--id", "0", "--udp-port", "4251", "--master-port", "5000"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--key", "server.key"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--check"},
        {"--instance", "--id", "7", "--udp-port", "4251", "--master-port", "5000", "--fingerprint"},
    };

    for (const auto &arguments : invalid) {
        CAPTURE(arguments);
        CHECK_THROWS_AS(parse(arguments), std::invalid_argument);
    }
}

TEST_CASE("The standalone mode only exists in debug builds, without a master", "[server]")
{
    if (!standaloneAvailable()) {
        CHECK_THROWS_AS(parse({"--instance", "--standalone", "--udp-port", "4242"}), std::invalid_argument);
        return;
    }
    const Options options = parse({"--instance", "--standalone", "--udp-port", "4242"});
    CHECK(options.instance.standalone);
    CHECK(options.instance.id == 1);
    CHECK_THROWS_AS(parse({"--instance", "--standalone", "--udp-port", "4242", "--master-port", "5000"}),
                    std::invalid_argument);
}

// --- Help, version, exit codes ---

TEST_CASE("The option --fingerprint shows the key of the master instead of running it", "[server]")
{
    const Options options = parse({"--fingerprint", "--key", "/etc/rtype/server.key"});

    CHECK(options.mode == Options::Mode::Fingerprint);
    CHECK(options.master.key == "/etc/rtype/server.key");
    CHECK(parse({"--fingerprint"}).master.key == "server.key");
}

TEST_CASE("--help and --version win over everything else", "[server]")
{
    CHECK(parse({"--unknown", "--help"}).mode == Options::Mode::Help);
    CHECK(parse({"-h"}).mode == Options::Mode::Help);
    CHECK(parse({"--instance", "--version"}).mode == Options::Mode::Version);
}

TEST_CASE("The program shows its help and version, and refuses invalid arguments", "[server]")
{
    std::ostringstream out;
    std::istringstream in;

    CHECK(run(std::vector<std::string_view>{"--help"}, out, in) == Success);
    CHECK(out.str() == usage());
    out.str("");
    CHECK(run(std::vector<std::string_view>{"--version"}, out, in) == Success);
    CHECK(out.str() == "r-type_server " + std::string(version()) + "\n");
    out.str("");
    CHECK(run(std::vector<std::string_view>{"--port", "nope"}, out, in) == InvalidArguments);
    CHECK(out.str().empty());
}

TEST_CASE("An instance without a secret on its standard input does not start", "[server]")
{
    std::ostringstream out;
    std::istringstream nothing;
    std::istringstream wrong("not a secret\n");

    const std::vector<std::string_view> arguments{"--instance", "--id",          "7",   "--udp-port",
                                                  "4251",       "--master-port", "5000"};
    CHECK(run(arguments, out, nothing) == Failure);
    CHECK(run(arguments, out, wrong) == Failure);
}

// --- Secret ---

TEST_CASE("A secret survives a round trip through its text", "[server]")
{
    Secret secret{};
    for (std::size_t i = 0; i < secret.size(); ++i) {
        secret[i] = static_cast<std::byte>(i * 9);
    }

    const std::string text = formatSecret(secret);

    CHECK(text.size() == 64);
    CHECK(text.starts_with("0009121b"));
    CHECK(parseSecret(text) == secret);
    CHECK(parseSecret(text + "\r") == secret);
    std::istringstream input(text + "\n");
    CHECK(readSecret(input) == secret);
}

TEST_CASE("Anything but 64 lowercase hexadecimal digits is not a secret", "[server]")
{
    const std::string valid(64, 'a');
    std::string upper = valid;
    upper[10] = 'A';
    std::string other = valid;
    other[63] = 'g';

    CHECK(parseSecret(valid));
    CHECK_FALSE(parseSecret(upper));
    CHECK_FALSE(parseSecret(other));
    CHECK_FALSE(parseSecret(valid.substr(1)));
    CHECK_FALSE(parseSecret(valid + "a"));
    CHECK_FALSE(parseSecret(""));
    std::istringstream empty;
    CHECK_FALSE(readSecret(empty));
}
