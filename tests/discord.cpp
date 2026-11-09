// Tests of the fake Social SDK (tests/helpers/FakeSocialSdk.hpp), which the tests of the features of Discord
// use in place of libs/discord/sdk: it keeps the contract of the interfaces of rtype::discord, every answer
// of Discord delivered by a poll(), never inside the call that asked
// (docs/architecture/client-online.md#discord).

#include "FakeSocialSdk.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using rtype::discord::Authorization;
using rtype::discord::AuthorizationError;
using rtype::discord::AuthorizationResult;
using rtype::discord::ConnectionStatus;

TEST_CASE("Discord fake: an authorization is answered at the next poll only", "[discord]")
{
    testing::FakeSocialSdk sdk;
    std::vector<AuthorizationResult> results;
    sdk.authorizer.authorize(42, "identify",
                             [&](const AuthorizationResult &result) { results.push_back(result); });
    CHECK(sdk.authorizer.running());
    CHECK(sdk.authorizer.applications == std::vector<std::uint64_t>{42});

    const Authorization accepted{
        .code = "code", .codeVerifier = "verifier", .redirectUri = "http://127.0.0.1"};
    sdk.authorizer.accept(accepted);
    CHECK(results.empty());
    sdk.poll();
    REQUIRE(results.size() == 1);
    CHECK(std::get<Authorization>(results.front()) == accepted);
    CHECK_FALSE(sdk.authorizer.running());

    // Answered once
    sdk.authorizer.refuse(AuthorizationError::Refused);
    sdk.poll();
    CHECK(results.size() == 1);
}

TEST_CASE("Discord fake: a cancelled or replaced authorization never calls its done", "[discord]")
{
    testing::FakeSocialSdk sdk;
    int first = 0;
    int second = 0;

    sdk.authorizer.authorize(1, "identify", [&](const AuthorizationResult &) { ++first; });
    sdk.authorizer.refuse(AuthorizationError::Failed);
    sdk.authorizer.cancel();
    sdk.poll();
    CHECK(first == 0);
    CHECK_FALSE(sdk.authorizer.running());

    // The answer of the first, posted before the second started, is dropped
    sdk.authorizer.authorize(1, "identify", [&](const AuthorizationResult &) { ++first; });
    sdk.authorizer.refuse(AuthorizationError::Refused);
    sdk.authorizer.authorize(2, "identify", [&](const AuthorizationResult &result) {
        ++second;
        CHECK(std::get<AuthorizationError>(result) == AuthorizationError::Refused);
    });
    sdk.poll();
    CHECK(first == 0);
    CHECK(second == 0);
    sdk.authorizer.refuse(AuthorizationError::Refused);
    sdk.poll();
    CHECK(second == 1);
}

TEST_CASE("Discord fake: the presence shows the last activity until cleared", "[discord]")
{
    testing::FakeSocialSdk sdk;
    CHECK_FALSE(sdk.presence.shown);
    rtype::discord::Activity solo;
    solo.state = "Solo · vague 7/12";
    solo.details = "Dans le hangar";
    sdk.presence.update(solo);
    rtype::discord::Activity online;
    online.state = "Partie · vague 1/12";
    online.party = rtype::discord::Party{.size = 3, .max = 4};
    sdk.presence.update(online);
    REQUIRE(sdk.presence.shown);
    CHECK(sdk.presence.shown->party == rtype::discord::Party{.size = 3, .max = 4});
    CHECK(sdk.presence.updates.size() == 2);
    sdk.presence.clear();
    CHECK_FALSE(sdk.presence.shown);
}

TEST_CASE("Discord fake: the connection changes status at the polls, and tells it", "[discord]")
{
    testing::FakeSocialSdk sdk;
    std::vector<ConnectionStatus> statuses;
    int expiring = 0;
    sdk.connection.onStatusChanged([&](ConnectionStatus status) { statuses.push_back(status); });
    sdk.connection.onTokenExpiring([&] { ++expiring; });
    CHECK(sdk.connection.status() == ConnectionStatus::Disconnected);

    sdk.connection.connect("token");
    CHECK(sdk.connection.status() == ConnectionStatus::Disconnected);
    sdk.poll();
    CHECK(sdk.connection.status() == ConnectionStatus::Connecting);
    sdk.connection.becomeReady();
    sdk.poll();
    CHECK(sdk.connection.status() == ConnectionStatus::Ready);

    // A new token for a connection that expires keeps it ready
    sdk.connection.expireToken();
    sdk.poll();
    CHECK(expiring == 1);
    sdk.connection.connect("refreshed");
    sdk.poll();
    CHECK(sdk.connection.status() == ConnectionStatus::Ready);
    CHECK(sdk.connection.tokens == std::vector<std::string>{"token", "refreshed"});

    sdk.connection.lose();
    sdk.poll();
    CHECK(statuses == std::vector<ConnectionStatus>{ConnectionStatus::Connecting, ConnectionStatus::Ready,
                                                    ConnectionStatus::Disconnected});
}
