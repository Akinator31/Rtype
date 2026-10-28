// A program with a bug on purpose, for the sanitizers.* tests (tests/CMakeLists.txt): each one runs it under
// the sanitizers of the build and passes only when they report the bug. Without them, they would miss real
// ones.
//
//   leak        allocates memory and loses it                      (LeakSanitizer)
//   freed       reads memory already freed                         (AddressSanitizer)
//   undefined   overflows a signed integer                         (UndefinedBehaviorSanitizer)
//   race        two threads write the same integer without a lock  (ThreadSanitizer)
//
// It exits with 0 when the bug went unnoticed.

#include <climits>
#include <cstdlib>
#include <string_view>
#include <thread>

namespace
{

constexpr int Usage = 2;

// Out of the optimizer's sight: the bugs below must happen, not be proved away
volatile int opaque = 0;

void leak()
{
    int *const lost = new int[16];
    lost[0] = opaque;
    opaque = lost[0];
}

void freed()
{
    int *const value = new int(opaque);
    delete value;
    opaque = *value;
}

void undefined()
{
    int value = INT_MAX - opaque;
    value += 1;
    opaque = value;
}

void race()
{
    int shared = 0;
    std::thread other([&shared] { shared += 1; });
    shared += 1;
    other.join();
    opaque = shared;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        return Usage;
    }
    const std::string_view bug = argv[1];

    if (bug == "leak") {
        leak();
    } else if (bug == "freed") {
        freed();
    } else if (bug == "undefined") {
        undefined();
    } else if (bug == "race") {
        race();
    } else {
        return Usage;
    }
    return EXIT_SUCCESS;
}
