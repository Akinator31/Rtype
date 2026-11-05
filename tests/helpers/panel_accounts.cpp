// Accounts for the end-to-end tests of the panel (web/admin/e2e): written in the database of a master before
// it starts, their emails verified, as if their players had registered.
//
//   rtype_panel_accounts <database address> <username>:<email>:<password>[:admin]...
//
// The migrations of the master are applied first. An account ending with `:admin` has the role admin.

#include "Accounts/AccountStore.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Master/ServerDatabase.hpp"
#include "rtype/database/Address.hpp"
#include "rtype/database/Database.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace
{

constexpr int Usage = 2;
constexpr int Failed = 1;

std::vector<std::string> split(const std::string &text)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t colon = text.find(':', start);
        parts.push_back(text.substr(start, colon - start));
        if (colon == std::string::npos) {
            return parts;
        }
        start = colon + 1;
    }
}

} // namespace

int main(int argc, char **argv)
{
    using namespace rtype::server;
    if (argc < 3) {
        std::cerr
            << "Usage: rtype_panel_accounts <database address> <username>:<email>:<password>[:admin]...\n";
        return Usage;
    }
    try {
        rtype::database::Database database =
            rtype::database::Database::open(rtype::database::Address::parse(argv[1]));
        database.migrate(ServerMigrations);
        const auto now = std::chrono::system_clock::now();
        for (int i = 2; i < argc; ++i) {
            const std::vector<std::string> parts = split(argv[i]);
            if (parts.size() < 3 || (parts.size() == 4 && parts[3] != "admin") || parts.size() > 4) {
                std::cerr << "Not an account: " << argv[i] << '\n';
                return Usage;
            }
            const auto created =
                createAccount(database,
                              {.username = parts[0],
                               .email = parts[1],
                               .passwordHash = hashPassword(parts[2], DefaultArgon2Parameters),
                               .role = parts.size() == 4 ? AccountRole::Admin : AccountRole::Player},
                              now);
            const auto *account = std::get_if<Account>(&created);
            if (account == nullptr) {
                std::cerr << "Cannot create " << parts[0] << ": taken\n";
                return Failed;
            }
            markEmailVerified(database, account->id, now);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return Failed;
    }
}
