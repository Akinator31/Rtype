#include "Commands/ToastCommand.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

std::optional<engine::ToastType> findToastType(const std::string &name)
{
    if (name == "info") {
        return engine::ToastType::Info;
    }
    if (name == "success") {
        return engine::ToastType::Success;
    }
    if (name == "warning") {
        return engine::ToastType::Warning;
    }
    if (name == "error") {
        return engine::ToastType::Error;
    }
    return std::nullopt;
}

std::string joinArguments(const Console::Arguments &arguments, std::size_t first)
{
    std::string text;
    for (std::size_t index = first; index < arguments.size(); ++index) {
        text += (text.empty() ? "" : " ") + arguments[index];
    }
    return text;
}

void showToast(engine::Engine &engine, Console &console, const Console::Arguments &arguments)
{
    const std::optional<engine::ToastType> type =
        arguments.empty() ? std::nullopt : findToastType(arguments[0]);
    const std::string text = joinArguments(arguments, type ? 1 : 0);
    if (text.empty()) {
        console.print("Usage: toast [info|success|warning|error] <text>");
        return;
    }
    engine.showToast(text, type.value_or(engine::ToastType::Info));
}

} // namespace

void registerToastCommand(Console &console, engine::Engine &engine)
{
    console.registerConsoleCommand("toast", "toast [info|success|warning|error] <text>: shows a toast",
                                   [&engine](Console &target, const Console::Arguments &arguments) {
                                       showToast(engine, target, arguments);
                                   });
}

} // namespace rtype::client
