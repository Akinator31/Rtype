#pragma once

#include "Master/Administration.hpp"

#include <crow/json.h>

#include <string>

/// The JSON of the administration API (docs/protocol/admin-api.md). Times are Unix milliseconds, durations
/// milliseconds.
namespace rtype::server
{

[[nodiscard]] crow::json::wvalue toJson(const AdminServer &server);
[[nodiscard]] crow::json::wvalue toJson(const AdminGame &game);
[[nodiscard]] crow::json::wvalue toJson(const AdminPlayer &player);
[[nodiscard]] crow::json::wvalue toJson(const AdminAction &action);
[[nodiscard]] crow::json::wvalue toJson(const AddressBan &ban);
[[nodiscard]] crow::json::wvalue toJson(const AdminEvent &event);

/// A list as a JSON array.
template <typename Item>
[[nodiscard]] crow::json::wvalue toJsonList(const std::vector<Item> &items)
{
    std::vector<crow::json::wvalue> list;
    list.reserve(items.size());
    for (const Item &item : items) {
        list.push_back(toJson(item));
    }
    return crow::json::wvalue(std::move(list));
}

/// `empty`, `over`, `crashed`, `ended_by_admin`, `master_stopping`.
[[nodiscard]] std::string toString(GameEnd reason);

} // namespace rtype::server
